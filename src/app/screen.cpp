#include "screen.hpp"
#include "../log.hpp"
#include "../rmioc/screen.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <rfb/rfbclient.h>
// IWYU pragma: no_include <type_traits>

namespace chrono = std::chrono;

namespace app
{

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-avoid-magic-numbers)
void* screen::instance_tag = reinterpret_cast<void*>(6803);

screen::screen(rmioc::screen& device, rfbClient* vnc_client, vnsee::Orientation orientation)
: device(device)
, vnc_client(vnc_client)
, repaint_mode(repaint_modes::standard)
, standard_repaint_delay(500)
, fast_repaint_delay(50) // 15 FPS
, standard_waveform_mode(REFRESH_MODE_UI)
, fast_waveform_mode(REFRESH_MODE_ANIMATE)
, requested_(orientation)
, effective_(orientation == vnsee::Orientation::Auto
             ? vnsee::Orientation::Portrait : orientation)
{
    bytes_per_pixel_ = (this->device.get_bits_per_pixel() + 7) / 8;
    char *env_waveform = std::getenv("VNSEE_WAVEFORM_MODE");

    if (env_waveform != NULL) {
        if (strcmp(env_waveform, "FASTEST") == 0) {
            standard_repaint_delay = chrono::milliseconds(50);
            standard_waveform_mode = REFRESH_MODE_ANIMATE;
        } else if (strcmp(env_waveform, "FAST") == 0) {
            standard_repaint_delay = chrono::milliseconds(200);
            standard_waveform_mode = REFRESH_MODE_FAST;
        } else if (strcmp(env_waveform, "STANDARD") == 0) {
            standard_repaint_delay = chrono::milliseconds(500);
            standard_waveform_mode = REFRESH_MODE_UI;
        } else if (strcmp(env_waveform, "SLOW") == 0) {
            standard_repaint_delay = chrono::milliseconds(1000);
            standard_waveform_mode = REFRESH_MODE_CONTENT;
        }
    }

    rfbClientSetClientData(
        this->vnc_client,
        screen::instance_tag,
        this
    );

    // Ask the server to send pixels in the same format as the screen buffer
    this->vnc_client->format.bitsPerPixel = this->device.get_bits_per_pixel();
    this->vnc_client->format.depth = this->device.get_bits_per_pixel();
    this->vnc_client->format.redShift = this->device.get_red_format().offset;
    this->vnc_client->format.redMax = this->device.get_red_format().max();
    this->vnc_client->format.greenShift = this->device.get_green_format().offset;
    this->vnc_client->format.greenMax = this->device.get_green_format().max();
    this->vnc_client->format.blueShift = this->device.get_blue_format().offset;
    this->vnc_client->format.blueMax = this->device.get_blue_format().max();

    char *env_encoding = std::getenv("VNSEE_ENCODING");
    if (env_encoding != NULL) {
        if (strcmp(env_encoding, "RAW") == 0) {
            this->vnc_client->appData.encodingsString = "raw";
        } else if (strcmp(env_encoding, "COPYRECT") == 0) {
            this->vnc_client->appData.encodingsString = "copyrect";
        } else if (strcmp(env_encoding, "TIGHT") == 0) {
            this->vnc_client->appData.encodingsString = "tight";
            this->vnc_client->appData.compressLevel = 9;
            this->vnc_client->appData.enableJPEG = true;
            this->vnc_client->appData.qualityLevel = 0;
        } else if (strcmp(env_encoding, "HEXTILE") == 0) {
            this->vnc_client->appData.encodingsString = "hextile";
        } else if (strcmp(env_encoding, "ZLIB") == 0) {
            this->vnc_client->appData.encodingsString = "zlib";
            this->vnc_client->appData.compressLevel = 9;
        } else if (strcmp(env_encoding, "ZLIBHEX") == 0) {
            this->vnc_client->appData.encodingsString = "zlibhex";
            this->vnc_client->appData.compressLevel = 9;
        } else if (strcmp(env_encoding, "TRLE") == 0) {
            this->vnc_client->appData.encodingsString = "trle";
        } else if (strcmp(env_encoding, "ZRLE") == 0) {
            this->vnc_client->appData.encodingsString = "zrle";
        } else if (strcmp(env_encoding, "ZYWRLE") == 0) {
            this->vnc_client->appData.encodingsString = "zywrle";
            this->vnc_client->appData.qualityLevel = 0;
        } else if (strcmp(env_encoding, "ULTRA") == 0) {
            this->vnc_client->appData.encodingsString = "ultra";
        } else if (strcmp(env_encoding, "ULTRAZIP") == 0) {
            this->vnc_client->appData.encodingsString = "ultrazip";
        } else if (strcmp(env_encoding, "CORRE") == 0) {
            this->vnc_client->appData.encodingsString = "corre";
        } else if (strcmp(env_encoding, "RRE") == 0) {
            this->vnc_client->appData.encodingsString = "rre";
        }
    } else {
        this->vnc_client->appData.encodingsString = "copyrect";
    }

    this->vnc_client->appData.useRemoteCursor = true;
    this->vnc_client->MallocFrameBuffer = screen::create_framebuf;
    this->vnc_client->GotFrameBufferUpdate = screen::commit_updates;
}

void screen::repaint()
{
    // Clear the has_update flag only in standard repaint mode
    // In fast mode, a clean update will be needed in the future
    if (this->repaint_mode == repaint_modes::standard)
    {
        this->update_info.has_update = false;
    }

    this->last_repaint = chrono::steady_clock::now();

    // The accumulated dirty rect is in SOURCE coordinates because
    // libvncclient now writes into our intermediate buffer (src_buffer_).
    // Translate + rotate-copy into fb0, get back the panel-space rect.
    int dx = 0, dy = 0, dw = 0, dh = 0;
    this->blit_rotated(
        this->update_info.x, this->update_info.y,
        this->update_info.w, this->update_info.h,
        dx, dy, dw, dh);

    vnsee::log::print("Screen update")
        << "src " << this->update_info.w << 'x' << this->update_info.h
        << '+' << this->update_info.x << '+' << this->update_info.y
        << " → dst " << dw << 'x' << dh << '+' << dx << '+' << dy << '\n';

    if (dw <= 0 || dh <= 0) return;

    this->device.update(
        dx, dy, dw, dh,
        this->repaint_mode == repaint_modes::standard
            ? standard_waveform_mode
            : fast_waveform_mode
    );
}

auto screen::get_xres() -> int
{
    return this->device.get_xres();
}

auto screen::get_yres() -> int
{
    return this->device.get_yres();
}

void screen::set_repaint_mode(repaint_modes mode)
{
    this->repaint_mode = mode;

    vnsee::log::print("Screen update") << (mode == repaint_modes::standard
        ? "Switched to standard mode\n"
        : "Switched to fast mode\n");
}

auto screen::event_loop() -> event_loop_status
{
    if (this->update_info.has_update)
    {
        auto next_update_time = this->last_repaint + (
            this->repaint_mode == repaint_modes::standard
            ? standard_repaint_delay
            : fast_repaint_delay
        );

        auto now = chrono::steady_clock::now();
        long wait_time = chrono::duration_cast<chrono::milliseconds>(
            next_update_time - now
        ).count();

        if (wait_time <= 0)
        {
            this->repaint();
        }
        else
        {
            // Wait until the next update is due
            return {
                /* quit = */ false,
                /* timeout = */ wait_time
            };
        }
    }

    return {/* quit = */ false, /* timeout = */ -1};
}

auto screen::create_framebuf(rfbClient* vnc_client) -> rfbBool
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* that = reinterpret_cast<screen*>(
        rfbClientGetClientData(
            vnc_client,
            screen::instance_tag
        ));

    int xres = static_cast<int>(that->device.get_xres());
    int yres = static_cast<int>(that->device.get_yres());
    int sw   = vnc_client->width;
    int sh   = vnc_client->height;

    if (sw <= 0 || sh <= 0)
    {
        std::stringstream msg;
        msg << "Invalid server resolution (" << sw << 'x' << sh << ')';
        throw std::runtime_error{msg.str()};
    }

    that->src_width_  = sw;
    that->src_height_ = sh;

    // Resolve Orientation::Auto: pick LandscapeCW when the source is landscape
    // and the panel is portrait (or vice-versa); otherwise Portrait (1:1).
    if (that->requested_ == vnsee::Orientation::Auto)
    {
        bool fb_portrait  = yres > xres;
        bool src_portrait = sh > sw;
        that->effective_ = (fb_portrait != src_portrait)
            ? vnsee::Orientation::LandscapeCW
            : vnsee::Orientation::Portrait;
    }

    // Allocate an intermediate buffer at the SOURCE shape so libvncclient
    // writes pixels with its own row stride (sw) and never spills into fb0.
    // The rotated/identity blit into fb0 happens in repaint().
    const std::size_t bytes =
        static_cast<std::size_t>(sw)
        * static_cast<std::size_t>(sh)
        * static_cast<std::size_t>(that->bytes_per_pixel_);
    that->src_buffer_.assign(bytes, 0);
    vnc_client->frameBuffer = that->src_buffer_.data();

    std::cerr
        << "Framebuffer initialized: server=" << sw << 'x' << sh
        << " panel=" << xres << 'x' << yres
        << " orientation="
        << vnsee::orientation_to_string(that->effective_) << '\n';

    // Sanity check: warn if the resolved blit won't fully cover the panel.
    auto fits_panel = [&]() -> bool {
        switch (that->effective_)
        {
            case vnsee::Orientation::Portrait:
            case vnsee::Orientation::InvertedPortrait:
                return sw == xres && sh == yres;
            case vnsee::Orientation::LandscapeCW:
            case vnsee::Orientation::LandscapeCCW:
                return sw == yres && sh == xres;
            case vnsee::Orientation::Auto:
                return false;
        }
        return false;
    }();
    if (!fits_panel)
    {
        std::cerr << "Warning: server "
            << sw << 'x' << sh << " under orientation "
            << vnsee::orientation_to_string(that->effective_)
            << " does not exactly tile fb0 "
            << xres << 'x' << yres
            << " — image will be cropped/letterboxed.\n";
    }

    return TRUE;
}

// Copy a dirty rect from src_buffer_ → fb0 with the configured rotation.
// Returns the rect in fb0 (panel) coordinates so device.update() can refresh
// only what changed.
void screen::blit_rotated(
    int sx, int sy, int sw, int sh,
    int& dx, int& dy, int& dw, int& dh)
{
    const int Bpp     = bytes_per_pixel_;
    const int src_w   = src_width_;
    const int src_h   = src_height_;
    const int panel_w = device.get_xres();
    const int panel_h = device.get_yres();

    const std::uint8_t* src = src_buffer_.data();
    std::uint8_t*       dst = device.get_data();

    // Clamp the input rect to source bounds.
    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sx + sw > src_w) sw = src_w - sx;
    if (sy + sh > src_h) sh = src_h - sy;
    if (sw <= 0 || sh <= 0) { dx = dy = 0; dw = dh = 0; return; }

    auto put = [&](int dst_col, int dst_row, const std::uint8_t* p) {
        if (dst_col < 0 || dst_col >= panel_w) return;
        if (dst_row < 0 || dst_row >= panel_h) return;
        std::memcpy(
            dst + (static_cast<std::size_t>(dst_row) * panel_w + dst_col) * Bpp,
            p, Bpp);
    };

    switch (effective_)
    {
        case vnsee::Orientation::Portrait:
        case vnsee::Orientation::Auto: // shouldn't happen post-resolve
        {
            // Identity row-copy. Crop to panel size on each axis.
            int copy_w = std::min(sw, panel_w - sx);
            int copy_h = std::min(sh, panel_h - sy);
            if (copy_w <= 0 || copy_h <= 0) { dx=dy=0; dw=dh=0; return; }
            for (int y = 0; y < copy_h; ++y)
            {
                std::memcpy(
                    dst + (static_cast<std::size_t>(sy + y) * panel_w + sx) * Bpp,
                    src + (static_cast<std::size_t>(sy + y) * src_w   + sx) * Bpp,
                    static_cast<std::size_t>(copy_w) * Bpp);
            }
            dx = sx; dy = sy; dw = copy_w; dh = copy_h;
            break;
        }

        case vnsee::Orientation::InvertedPortrait:
        {
            // 180°: src(sx, sy) → dst(src_w-1-sx, src_h-1-sy)
            for (int y = 0; y < sh; ++y)
            {
                int src_row = sy + y;
                int dst_row = src_h - 1 - src_row;
                for (int x = 0; x < sw; ++x)
                {
                    int src_col = sx + x;
                    int dst_col = src_w - 1 - src_col;
                    put(dst_col, dst_row,
                        src + (static_cast<std::size_t>(src_row) * src_w + src_col) * Bpp);
                }
            }
            dx = src_w - sx - sw;
            dy = src_h - sy - sh;
            dw = sw;
            dh = sh;
            break;
        }

        case vnsee::Orientation::LandscapeCW:
        {
            // 90° CW: src(sx, sy) → dst(src_h-1-sy, sx)
            // Source W×H lands as panel H×W.
            for (int y = 0; y < sh; ++y)
            {
                int src_row = sy + y;
                int dst_col = src_h - 1 - src_row;
                const std::uint8_t* row = src + static_cast<std::size_t>(src_row) * src_w * Bpp;
                for (int x = 0; x < sw; ++x)
                {
                    int src_col = sx + x;
                    int dst_row = src_col;
                    put(dst_col, dst_row, row + static_cast<std::size_t>(src_col) * Bpp);
                }
            }
            dx = src_h - sy - sh;
            dy = sx;
            dw = sh;
            dh = sw;
            break;
        }

        case vnsee::Orientation::LandscapeCCW:
        {
            // 90° CCW: src(sx, sy) → dst(sy, src_w-1-sx)
            for (int y = 0; y < sh; ++y)
            {
                int src_row = sy + y;
                int dst_col = src_row;
                const std::uint8_t* row = src + static_cast<std::size_t>(src_row) * src_w * Bpp;
                for (int x = 0; x < sw; ++x)
                {
                    int src_col = sx + x;
                    int dst_row = src_w - 1 - src_col;
                    put(dst_col, dst_row, row + static_cast<std::size_t>(src_col) * Bpp);
                }
            }
            dx = sy;
            dy = src_w - sx - sw;
            dw = sh;
            dh = sw;
            break;
        }
    }

    // Final clamp to panel bounds (in case rotation pushed out of frame).
    if (dx < 0) { dw += dx; dx = 0; }
    if (dy < 0) { dh += dy; dy = 0; }
    if (dx + dw > panel_w) dw = panel_w - dx;
    if (dy + dh > panel_h) dh = panel_h - dy;
    if (dw < 0) dw = 0;
    if (dh < 0) dh = 0;
}

void screen::transform_input(int& x, int& y) const
{
    // If the framebuffer hasn't been initialized yet, leave coords untouched.
    if (src_width_ <= 0 || src_height_ <= 0) return;

    // Touch events arrive in fb0 (panel) coordinates. The VNC server expects
    // source coordinates. Apply the inverse of the current rotation.
    const int src_w = src_width_;
    const int src_h = src_height_;

    switch (effective_)
    {
        case vnsee::Orientation::Portrait:
        case vnsee::Orientation::Auto:
            // identity
            break;
        case vnsee::Orientation::InvertedPortrait:
        {
            int nx = src_w - 1 - x;
            int ny = src_h - 1 - y;
            x = nx; y = ny;
            break;
        }
        case vnsee::Orientation::LandscapeCW:
        {
            // Forward: src(sx,sy) → dst(src_h-1-sy, sx). Inverse: sx=dy, sy=src_h-1-dx.
            int sx = y;
            int sy = src_h - 1 - x;
            x = sx; y = sy;
            break;
        }
        case vnsee::Orientation::LandscapeCCW:
        {
            // Forward: src(sx,sy) → dst(sy, src_w-1-sx). Inverse: sx=src_w-1-dy, sy=dx.
            int sx = src_w - 1 - y;
            int sy = x;
            x = sx; y = sy;
            break;
        }
    }
}

void screen::commit_updates(rfbClient* vnc_client, int x, int y, int w, int h)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* that = reinterpret_cast<screen*>(
        rfbClientGetClientData(
            vnc_client,
            screen::instance_tag
        ));

    // Register the region as pending update, potentially extending
    // an existing one
    vnsee::log::print("VNC update") << w << 'x' << h << '+' << x << '+' << y << '\n';

    if (that->update_info.has_update)
    {
        // Merge new rectangle with existing one
        int left_x = std::min(x, that->update_info.x);
        int top_y = std::min(y, that->update_info.y);
        int right_x = std::max(x + w,
                that->update_info.x + that->update_info.w);
        int bottom_y = std::max(y + h,
                that->update_info.y + that->update_info.h);

        that->update_info.x = left_x;
        that->update_info.y = top_y;
        that->update_info.w = right_x - left_x;
        that->update_info.h = bottom_y - top_y;
    }
    else
    {
        that->update_info.x = x;
        that->update_info.y = y;
        that->update_info.w = w;
        that->update_info.h = h;
        that->update_info.has_update = true;
    }
}

} // namespace app
