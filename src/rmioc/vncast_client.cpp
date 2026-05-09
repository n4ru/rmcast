#include "vncast_client.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace vncast {

namespace {
void send_all(int fd, const void *buf, size_t n) {
    auto *p = static_cast<const char *>(buf);
    while (n > 0) {
        ssize_t r = ::send(fd, p, n, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "vncast send");
        }
        n -= r; p += r;
    }
}
void recv_all(int fd, void *buf, size_t n) {
    auto *p = static_cast<char *>(buf);
    while (n > 0) {
        ssize_t r = ::recv(fd, p, n, 0);
        if (r == 0) throw std::runtime_error("vncast: server closed during recv");
        if (r < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "vncast recv");
        }
        n -= r; p += r;
    }
}
}  // namespace

ClientConnection::ClientConnection(const char *socket_path) {
    if (!socket_path || !*socket_path) {
        socket_path = std::getenv("VNCAST_QTFB_SOCKET");
        if (!socket_path || !*socket_path) socket_path = kDefaultSocket;
    }

    m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) throw std::system_error(errno, std::generic_category(), "vncast socket()");

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (::connect(m_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        int e = errno;
        ::close(m_fd); m_fd = -1;
        throw std::system_error(e, std::generic_category(),
            std::string("vncast connect ") + socket_path);
    }

    handshake();
}

ClientConnection::~ClientConnection() {
    if (m_fd >= 0) {
        Bye b{};
        b.header.magic = kMagic;
        b.header.tag   = static_cast<uint32_t>(Tag::Bye);
        b.reason       = 0;
        try { send_all(m_fd, &b, sizeof(b)); } catch (...) {}
        ::close(m_fd);
    }
    if (m_shadow) {
        delete[] m_shadow;
    }
    if (m_shm && m_shm_bytes) {
        ::munmap(m_shm, m_shm_bytes);
    }
    if (m_shm_fd >= 0) ::close(m_shm_fd);
}

void ClientConnection::handshake() {
    // VNCAST_FPS env carries the user's "Refresh rate" choice from
    // vncast.so. 0 = uncapped. Server enforces by dropping FRAME emits
    // that would violate the rate.
    uint32_t fps_cap = 0;
    if (const char *env = std::getenv("VNCAST_FPS")) {
        long v = std::strtol(env, nullptr, 10);
        if (v > 0 && v <= 120) fps_cap = static_cast<uint32_t>(v);
    }

    // VNCAST_WAVEFORM env carries the user's panel-refresh preference.
    // A2 / DU are fast greyscale waveforms; we coerce frames to grayscale
    // before writing into the shm so xochitl's renderer picks the fast
    // EPDC mode automatically (it picks waveform from content, not API).
    if (const char *env = std::getenv("VNCAST_WAVEFORM")) {
        m_waveform = env;
        m_force_grayscale = (m_waveform == "A2" || m_waveform == "DU");
    }

    HelloC2S h{};
    h.header.magic = kMagic;
    h.header.tag   = static_cast<uint32_t>(Tag::HelloC2S);
    h.client_version = kVersion;
    h.requested_w  = 0;
    h.requested_h  = 0;
    h.requested_fps = fps_cap;
    send_all(m_fd, &h, sizeof(h));

    HelloAckS2C ack{};
    recv_all(m_fd, &ack, sizeof(ack));
    if (ack.header.magic != kMagic
        || ack.header.tag != static_cast<uint32_t>(Tag::HelloAckS2C)) {
        throw std::runtime_error("vncast: bad hello-ack header");
    }
    if (!ack.accepted) {
        throw std::runtime_error("vncast: server rejected handshake");
    }
    m_w = ack.w; m_h = ack.h; m_stride = ack.stride;
    m_bpp = ack.bpp; m_format = ack.format;
    m_shm_name.assign(ack.shm_name,
                      std::min<size_t>(ack.shm_name_len, sizeof(ack.shm_name)));
    open_shm(m_shm_name.c_str(), m_stride * m_h);

    // 16-bit RGB565 shadow buffer that vnsee paints into. We convert to
    // the real shm format on update.
    m_shadow_bytes = static_cast<size_t>(m_w) * m_h * 2u;
    m_shadow       = new uint8_t[m_shadow_bytes]();

    std::fprintf(stderr,
        "[vncast/client] connected: %ux%u stride=%u bpp=%u fmt=%u shm=%s shadow=%zuB\n",
        m_w, m_h, m_stride, m_bpp, m_format, m_shm_name.c_str(), m_shadow_bytes);
}

void ClientConnection::open_shm(const char *name, size_t bytes) {
    std::string path = std::string("/dev/shm") + name;
    m_shm_fd = ::open(path.c_str(), O_RDWR);
    if (m_shm_fd < 0) {
        throw std::system_error(errno, std::generic_category(),
            std::string("vncast: open shm ") + path);
    }
    void *p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
    if (p == MAP_FAILED) {
        int e = errno; ::close(m_shm_fd); m_shm_fd = -1;
        throw std::system_error(e, std::generic_category(), "vncast mmap");
    }
    m_shm = static_cast<uint8_t *>(p);
    m_shm_bytes = bytes;
}

void ClientConnection::blit_rect_to_shm(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > static_cast<int>(m_w)) w = static_cast<int>(m_w) - x;
    if (y + h > static_cast<int>(m_h)) h = static_cast<int>(m_h) - y;
    if (w <= 0 || h <= 0) return;

    const uint8_t  *src     = m_shadow;
    const size_t    src_bpr = static_cast<size_t>(m_w) * 2u;       // RGB565 bytes-per-row
    uint8_t        *dst     = m_shm;
    const size_t    dst_bpr = m_stride;

    if (m_format == 1 /* RGBA8888 */) {
        for (int yy = 0; yy < h; ++yy) {
            const uint8_t  *sp = src + (static_cast<size_t>(y + yy) * src_bpr) + x * 2u;
            uint8_t        *dp = dst + (static_cast<size_t>(y + yy) * dst_bpr) + x * 4u;
            for (int xx = 0; xx < w; ++xx) {
                uint16_t px = static_cast<uint16_t>(sp[0]) | (static_cast<uint16_t>(sp[1]) << 8);
                uint8_t r = (px >> 11) & 0x1F;
                uint8_t g = (px >>  5) & 0x3F;
                uint8_t b =  px        & 0x1F;
                // Expand 5/6-bit components to 8-bit.
                uint8_t r8 = static_cast<uint8_t>((r << 3) | (r >> 2));
                uint8_t g8 = static_cast<uint8_t>((g << 2) | (g >> 4));
                uint8_t b8 = static_cast<uint8_t>((b << 3) | (b >> 2));
                if (m_force_grayscale) {
                    // Coerce to grayscale so xochitl's renderer picks the
                    // fast greyscale EPDC waveform (A2 / DU). Equal R=G=B
                    // means xochitl's content-classifier sees no chroma.
                    uint8_t y8 = static_cast<uint8_t>((r8 * 30 + g8 * 59 + b8 * 11) / 100);
                    r8 = g8 = b8 = y8;
                }
                dp[0] = r8;
                dp[1] = g8;
                dp[2] = b8;
                dp[3] = 0xFF;
                sp += 2; dp += 4;
            }
        }
    } else if (m_format == 0 /* Grayscale16 */) {
        for (int yy = 0; yy < h; ++yy) {
            const uint8_t  *sp = src + (static_cast<size_t>(y + yy) * src_bpr) + x * 2u;
            uint8_t        *dp = dst + (static_cast<size_t>(y + yy) * dst_bpr) + x * 2u;
            for (int xx = 0; xx < w; ++xx) {
                uint16_t px = static_cast<uint16_t>(sp[0]) | (static_cast<uint16_t>(sp[1]) << 8);
                uint8_t r = (px >> 11) & 0x1F;
                uint8_t g = (px >>  5) & 0x3F;
                uint8_t b =  px        & 0x1F;
                uint16_t r8 = (r << 3) | (r >> 2);
                uint16_t g8 = (g << 2) | (g >> 4);
                uint16_t b8 = (b << 3) | (b >> 2);
                uint16_t y16 = static_cast<uint16_t>((r8 * 30 + g8 * 59 + b8 * 11) / 100) * 257u;
                dp[0] = static_cast<uint8_t>(y16 & 0xFF);
                dp[1] = static_cast<uint8_t>(y16 >> 8);
                sp += 2; dp += 2;
            }
        }
    }
}

void ClientConnection::send_partial_update(int x, int y, int w, int h) {
    blit_rect_to_shm(x, y, w, h);

    Frame f{};
    f.header.magic = kMagic;
    f.header.tag   = static_cast<uint32_t>(Tag::Frame);
    f.seq = ++m_seq;
    f.x = x; f.y = y; f.w = w; f.h = h;
    send_all(m_fd, &f, sizeof(f));
}

void ClientConnection::send_complete_update() {
    send_partial_update(0, 0, static_cast<int>(m_w), static_cast<int>(m_h));
}

}  // namespace vncast
