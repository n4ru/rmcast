#pragma once
//
// vncast qtfb client. Talks the v1 wire protocol to vncast.so's
// qtfb::Server (/run/vncast.sock by default; override via the
// VNCAST_QTFB_SOCKET env var).
//
// Used by rmioc::screen when VNCAST_QTFB_SOCKET is set in the environment;
// transparently replaces the AppLoad qtfb client. Same shape so no other
// vnsee code has to change.
//
// Wire format must stay in sync with rm-cast/extension/src/qtfb/protocol.h.

#include <cstdint>
#include <cstddef>
#include <string>

namespace vncast {

constexpr uint32_t kMagic   = 0x56434B31u;          // 'VCK1'
constexpr const char *kDefaultSocket = "/run/vncast.sock";
constexpr uint32_t kVersion = 1;

enum class Tag : uint32_t {
    HelloC2S    = 0x01,
    HelloAckS2C = 0x02,
    Frame       = 0x03,
    InputS2C    = 0x04,
    Bye         = 0xFF,
};

#pragma pack(push, 1)
struct Header     { uint32_t magic; uint32_t tag; };
struct HelloC2S   { Header header; uint32_t client_version, requested_w, requested_h, requested_fps; };
struct HelloAckS2C{
    Header header;
    uint32_t accepted, w, h, stride, bpp, format, shm_name_len;
    char     shm_name[64];
};
struct Frame      { Header header; uint32_t seq, x, y, w, h; };
struct Bye        { Header header; uint32_t reason; };
#pragma pack(pop)

class ClientConnection {
public:
    // Throws std::system_error on connection / handshake / mmap failure.
    explicit ClientConnection(const char *socket_path = nullptr);
    ~ClientConnection();

    ClientConnection(const ClientConnection &)            = delete;
    ClientConnection &operator=(const ClientConnection &) = delete;

    // shadow_data() points at the 16-bit RGB565 shadow buffer that vnsee
    // writes into. update() converts the dirty rect to RGBA8888 (or
    // Grayscale16) and copies into the real qtfb shm, then signals.
    uint8_t *shadow_data() const { return m_shadow; }
    size_t   shadow_bytes() const { return m_shadow_bytes; }

    int      width()  const { return static_cast<int>(m_w); }
    int      height() const { return static_cast<int>(m_h); }
    uint32_t bpp()    const { return m_bpp; }   // shm bpp (16 or 32)
    uint32_t format() const { return m_format; } // 0=gray16, 1=rgba8888

    // Convert + push a dirty rect (in shadow coordinates) and signal a
    // FRAME to the server.
    void send_partial_update(int x, int y, int w, int h);
    void send_complete_update();

private:
    void handshake();
    void open_shm(const char *name, size_t bytes);
    void blit_rect_to_shm(int x, int y, int w, int h);

    int       m_fd        = -1;
    int       m_shm_fd    = -1;
    uint8_t  *m_shm       = nullptr;
    size_t    m_shm_bytes = 0;
    uint8_t  *m_shadow    = nullptr;     // 16-bit RGB565 buffer vnsee writes into
    size_t    m_shadow_bytes = 0;
    uint32_t  m_w = 0, m_h = 0, m_stride = 0, m_bpp = 0, m_format = 0;
    uint32_t  m_seq = 0;
    std::string m_shm_name;
};

}  // namespace vncast
