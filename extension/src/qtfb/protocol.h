#pragma once
#include <stdint.h>

// vncast qtfb wire protocol v1
// =============================
// One client at a time. Framed messages over a SEQPACKET-style Unix socket
// at /run/vncast.sock. The server (in xochitl, owned by vncast.so) accepts
// a connection from vnsee, hands back the shm name + framebuffer geometry,
// then receives FRAME notifications whenever vnsee finishes painting. The
// server's QQuickPaintedItem repaints on each FRAME message.
//
// Framing: every message starts with a 4-byte little-endian magic + tag,
// followed by a fixed-size body. No length prefix yet — every message is
// fixed-size for v1.

namespace vncast::qtfb {

constexpr uint32_t MAGIC      = 0x56434B31u;  // 'VCK1' — vncast wire v1
constexpr const char *SOCKET  = "/run/vncast.sock";
constexpr uint32_t VERSION    = 1;

enum class Tag : uint32_t {
    HelloC2S    = 0x01,  // client → server, opens session
    HelloAckS2C = 0x02,  // server → client, replies with shm + geom
    Frame       = 0x03,  // client → server, "frame N is ready in shm"
    InputS2C    = 0x04,  // server → client, touch/pen event (round 2)
    Bye         = 0xFF,  // either direction, orderly shutdown
};

#pragma pack(push, 1)
struct Header {
    uint32_t magic;       // == MAGIC
    uint32_t tag;         // Tag enum
};

struct HelloC2S {
    Header header;
    uint32_t client_version;     // VERSION the client speaks
    uint32_t requested_w;        // client's preferred width  (0 = ask server)
    uint32_t requested_h;        // client's preferred height (0 = ask server)
    uint32_t requested_fps;      // 0 = uncapped ("free"); otherwise frames/sec cap
};

struct HelloAckS2C {
    Header header;
    uint32_t accepted;           // 1 = ok, 0 = rejected (close)
    uint32_t w;                  // negotiated width  (px)
    uint32_t h;                  // negotiated height (px)
    uint32_t stride;             // bytes per row in shm
    uint32_t bpp;                // bits per pixel: 16 (rM1/rM2) or 32 (rMPP)
    uint32_t format;             // 0 = grayscale16-le, 1 = RGBA8888
    uint32_t shm_name_len;       // bytes of shm_name[] that follow
    char     shm_name[64];       // /vncast-XXXXX, NUL-padded
};

struct Frame {
    Header header;
    uint32_t seq;                // monotonic frame id
    uint32_t x, y, w, h;         // dirty rect; (0,0,0,0) → full screen
};

struct InputS2C {
    Header header;
    uint32_t kind;     // 0 = touch_down, 1 = touch_move, 2 = touch_up,
                       // 3 = pen_down, 4 = pen_move, 5 = pen_up,
                       // 6 = key
    int32_t  x, y;
    uint32_t pressure; // 0..4095 for pen; 0 for touch
    uint32_t key;      // X11 keysym for `kind == 6`, else 0
};

struct Bye {
    Header header;
    uint32_t reason;   // 0 = normal, 1 = error, 2 = protocol violation
};
#pragma pack(pop)

}  // namespace vncast::qtfb
