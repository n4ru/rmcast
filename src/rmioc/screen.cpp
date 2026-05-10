#include "screen.hpp"
#include "qtfb-client.h"
#include "vncast_client.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <system_error>
#include <tuple>
#include <utility>
#include <new>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace rmioc
{

namespace {
int detect_fb_format()
{
    std::ifstream device_id_file{"/sys/devices/soc0/machine"};
    std::string device_id;
    std::getline(device_id_file, device_id);

    if (device_id == "reMarkable Ferrari") {
        return FBFMT_RMPP_RGB565;
    } else if (device_id == "reMarkable Chiappa") {
        return FBFMT_RMPPM_RGB565;
    } else {
        return FBFMT_RM2FB;
    }
}

// Optional override: request a custom-shaped qtfb shared-memory buffer instead
// of the panel-native one.
std::optional<std::tuple<uint16_t, uint16_t>> qtfb_resolution_override()
{
    const char* env = std::getenv("QTFB_RESOLUTION");
    if (!env) return {};
    int w = 0, h = 0;
    if (std::sscanf(env, "%dx%d", &w, &h) != 2) return {};
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) return {};
    std::fprintf(stderr, "QTFB_RESOLUTION override: %dx%d\n", w, h);
    return std::make_tuple(static_cast<uint16_t>(w), static_cast<uint16_t>(h));
}

bool prefer_vncast()
{
    const char* env = std::getenv("VNCAST_QTFB_SOCKET");
    return env != nullptr && env[0] != '\0';
}
}

screen::screen()
{
    if (prefer_vncast()) {
        std::fprintf(stderr,
            "[rmioc::screen] $VNCAST_QTFB_SOCKET set — using vncast backend\n");
        vncast_         = std::make_unique<vncast::ClientConnection>();
        using_vncast_   = true;
    } else {
        qtfb_ = std::make_unique<qtfb::ClientConnection>(
            qtfb::getIDFromAppload(),
            detect_fb_format(),
            qtfb_resolution_override(),
            false);
        if (qtfb_->shmFD == -1) {
            throw std::system_error(errno, std::generic_category(),
                "(rmioc::screen) Open shared memory framebuffer");
        }
    }
}

void screen::update(int x, int y, int w, int h, int mode, bool /*wait*/)
{
    if (using_vncast_) {
        vncast_->send_partial_update(x, y, w, h);
    } else {
        if (qtfb_->getRefreshMode() != mode) qtfb_->setRefreshMode(mode);
        qtfb_->sendPartialUpdate(x, y, w, h);
    }
}

void screen::update_cursor(int x, int y, bool visible)
{
    if (!using_vncast_ || !vncast_) return;  // appload qtfb path: no-op
    constexpr int kCursorSize = 64;          // generous; covers typical OS cursors
    vncast_->send_cursor_pos(x, y, kCursorSize, kCursorSize, visible);
}

void screen::update(int mode, bool /*wait*/)
{
    if (using_vncast_) {
        vncast_->send_complete_update();
    } else {
        if (qtfb_->getRefreshMode() != mode) qtfb_->setRefreshMode(mode);
        qtfb_->sendCompleteUpdate();
    }
}

auto screen::get_data() -> std::uint8_t*
{
    return using_vncast_ ? vncast_->shadow_data() : qtfb_->shm;
}

auto screen::get_xres() const -> int
{
    return using_vncast_ ? vncast_->width()  : qtfb_->width();
}

auto screen::get_yres() const -> int
{
    return using_vncast_ ? vncast_->height() : qtfb_->height();
}

auto screen::get_bits_per_pixel() const -> unsigned short
{
    // Both backends present a 16-bit RGB565 surface to the vnsee client
    // (vncast converts to its native shm format inside send_*_update).
    return 16;
}

auto screen::get_red_format() const -> component_format
{
    return component_format{/* offset = */ 11, /* length = */ 5};
}

auto screen::get_green_format() const -> component_format
{
    return component_format{/* offset = */ 5, /* length = */ 6};
}

auto screen::get_blue_format() const -> component_format
{
    return component_format{/* offset = */ 0, /* length = */ 5};
}

auto screen::get_connection() -> qtfb::ClientConnection&
{
    if (using_vncast_) {
        // Caller is reaching for AppLoad-specific qtfb features (vkb,
        // refresh modes, etc.) which we don't have under vncast yet.
        // Throwing keeps the compile-time signature stable while making
        // misuse loud.
        throw std::runtime_error(
            "rmioc::screen::get_connection: not available under vncast backend");
    }
    return *qtfb_;
}

auto component_format::max() const -> std::uint32_t
{
    return (1U << this->length) - 1;
}

} // namespace rmioc
