#include "screen.hpp"
#include "qtfb-client.h"
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
// of the panel-native one. Useful when the VNC server is in landscape (e.g.
// 2160x1620) and we want qtfb to handle any panel-orientation rotation rather
// than rotating in vnsee. Read from QTFB_RESOLUTION="WIDTHxHEIGHT".
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
}

screen::screen()
    : conn(qtfb::getIDFromAppload(), detect_fb_format(), qtfb_resolution_override(), false)
{
    this->framebuf_fd = this->conn.shmFD;

    if (this->framebuf_fd == -1)
    {
        throw std::system_error(
            errno,
            std::generic_category(),
            "(rmioc::screen) Open shared memory framebuffer"
        );
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): Use of C library
    this->framebuf_ptr = this->conn.shm;
}

void screen::update(
    int x, int y, int w, int h, int mode, bool wait)
{
    if (this->conn.getRefreshMode() != mode)
    {
        this->conn.setRefreshMode(mode);
    }
    this->conn.sendPartialUpdate(x, y, w, h);
}

void screen::update(int mode, bool wait)
{
    if (this->conn.getRefreshMode() != mode)
    {
        this->conn.setRefreshMode(mode);
    }
    this->conn.sendCompleteUpdate();
}

auto screen::get_data() -> std::uint8_t*
{
    return this->framebuf_ptr;
}

auto screen::get_xres() const -> int
{
    return this->conn.width();
}

auto screen::get_yres() const -> int
{
    return this->conn.height();
}

auto screen::get_bits_per_pixel() const -> unsigned short
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    return 16;
}

auto screen::get_red_format() const -> component_format
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    return component_format{/* offset = */ 11, /* length = */ 5};
}

auto screen::get_green_format() const -> component_format
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    return component_format{/* offset = */ 5, /* length = */ 6};
}

auto screen::get_blue_format() const -> component_format
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    return component_format{/* offset = */ 0, /* length = */ 5};
}

auto screen::get_connection() -> qtfb::ClientConnection&
{
    return this->conn;
}

auto component_format::max() const -> std::uint32_t
{
    return (1U << this->length) - 1;
}

} // namespace rmioc
