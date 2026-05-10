#include "screen.hpp"
#include "../log.hpp"
#include "../rmioc/screen.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
, fast_repaint_delay(16) // ~60 FPS — desktop mirror is always dynamic
, standard_waveform_mode(REFRESH_MODE_UI)
, fast_waveform_mode(REFRESH_MODE_ANIMATE)
, requested_(orientation)
{
    effective_.store(orientation == vnsee::Orientation::Auto
                     ? vnsee::Orientation::Portrait : orientation);
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

    // The vncast backend has no local input thread to flip us into "fast"
    // (touch input is what triggers fast mode in the appload-target setup).
    // Without this, vncast sessions stay at standard's 500ms cadence — i.e.
    // 2 fps — even when the desktop is dynamic. Force fast at construction
    // so the desktop mirror runs at the higher cadence by default.
    if (this->device.is_vncast()) {
        this->repaint_mode = repaint_modes::fast;
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
        // Default for vncast on LAN/USB-tether. Order is the priority
        // hint sent to the server; TightVNC will pick Tight first, which
        // (with our compressLevel=0 + JPEG-off settings below) does
        // smart palette/RLE encoding without the expensive zlib pass.
        // That's ~10x smaller than Raw on screen content for ~free CPU,
        // a big win when wire is the limit (USB-tether ≈ 12 MB/s).
        // The trailing fallbacks let weird servers that don't speak Tight
        // still negotiate something other than full Raw.
        // Default encoding list. rcastmono1 is only advertised when the
        // user opted in via Settings → Mono (1-bit) toggle, since on a
        // non-rcast server it's an unknown ID (still safe — the server
        // just ignores it) but on rcast-host it would force B&W output
        // even when the user only wanted grayscale source coercion.
        const char *prefer_mono = std::getenv("VNCAST_PREFER_MONO1");
        const bool advertise_mono = prefer_mono && prefer_mono[0] == '1';
        this->vnc_client->appData.encodingsString = advertise_mono
            ? "rcastmono1 copyrect tight zrle hextile raw"
            : "copyrect tight zrle hextile raw";
    }

    this->vnc_client->appData.useRemoteCursor = true;

    // Default compressLevel=0 (no zlib pass on Tight payloads). User can
    // override with $VNCAST_COMPRESS_LEVEL=0..9 to A/B different sweet
    // spots and compare against the rolling-average frame time we log
    // below. Higher = smaller wire bytes, slower decode CPU. On LAN/USB
    // 0 typically wins; on a slow link 6+ might.
    int compress = 0;
    if (const char *env = std::getenv("VNCAST_COMPRESS_LEVEL")) {
        long v = std::strtol(env, nullptr, 10);
        if (v >= 0 && v <= 9) compress = static_cast<int>(v);
    }
    this->vnc_client->appData.compressLevel = compress;
    this->vnc_client->appData.qualityLevel  = -1;
    this->vnc_client->appData.enableJPEG    = false;
    std::cerr << "[vnsee/tuning] compressLevel=" << compress
              << " encodings=" << this->vnc_client->appData.encodingsString << '\n';

    this->vnc_client->MallocFrameBuffer = screen::create_framebuf;
    this->vnc_client->GotFrameBufferUpdate = screen::commit_updates;
    this->vnc_client->HandleCursorPos     = screen::handle_cursor_pos;
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

    // Resolve Orientation::Auto: pick LandscapeCCW when the source is landscape
    // and the panel is portrait (or vice-versa); otherwise Portrait (1:1).
    // (Was CW briefly while testing the static gradient stub; with real
    // DXGI content the user holds the rMPP such that CCW is upright.)
    if (that->requested_ == vnsee::Orientation::Auto)
    {
        bool fb_portrait  = yres > xres;
        bool src_portrait = sh > sw;
        that->effective_.store((fb_portrait != src_portrait)
            ? vnsee::Orientation::LandscapeCCW
            : vnsee::Orientation::Portrait);
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
        << vnsee::orientation_to_string(that->effective_.load()) << '\n';

    // Also write to a known file so we can read it after the fact even when
    // launched by AppLoad (which doesn't surface stderr).
    if (FILE* dbg = std::fopen("/tmp/vnsee-debug.log", "a"))
    {
        std::fprintf(dbg,
            "Framebuffer initialized: server=%dx%d panel=%dx%d orientation=%s\n",
            sw, sh, xres, yres,
            vnsee::orientation_to_string(that->effective_.load()));
        std::fclose(dbg);
    }

    // Sanity check: warn if the resolved blit won't fully cover the panel.
    auto eff = that->effective_.load();
    auto fits_panel = [&]() -> bool {
        switch (eff)
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
            << vnsee::orientation_to_string(eff)
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

    switch (effective_.load())
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

    switch (effective_.load())
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

rfbBool screen::handle_cursor_pos(rfbClient* vnc_client, int x, int y)
{
    auto* that = reinterpret_cast<screen*>(
        rfbClientGetClientData(vnc_client, screen::instance_tag));
    if (that) {
        // Pass through to backend; vncast will dedupe + forward to QML.
        // appload path is a no-op.
        that->device.update_cursor(x, y, /*visible=*/true);
    }
    return TRUE;
}

void screen::commit_updates(rfbClient* vnc_client, int x, int y, int w, int h)
{
    // Rolling-average timing across recent rect commits. Lets the user
    // compare $VNCAST_COMPRESS_LEVEL choices against ms-per-frame numbers
    // in the journal without having to rebuild. Tracks total commits in
    // this batch and the wall-clock between consecutive batch starts.
    static auto s_last_batch_start = std::chrono::steady_clock::now();
    static int  s_rects_in_batch = 0;
    static long s_pixels_in_batch = 0;
    static int  s_batches = 0;
    auto now = std::chrono::steady_clock::now();
    auto delta_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now - s_last_batch_start).count();
    s_rects_in_batch++;
    s_pixels_in_batch += static_cast<long>(w) * h;
    s_batches++;
    if (s_batches >= 60) {
        std::cerr << "[vnsee/timing] last 60 commits: "
                  << "elapsed=" << (delta_ms / 1000.0) << "ms "
                  << "rects=" << s_rects_in_batch << ' '
                  << "px=" << s_pixels_in_batch << ' '
                  << "avg_ms_per_commit=" << (delta_ms / 1000.0 / 60.0)
                  << '\n';
        s_last_batch_start = now;
        s_rects_in_batch = 0;
        s_pixels_in_batch = 0;
        s_batches = 0;
    }

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
        // Threshold-based merge (inspired by pl-semiotics/rM-vnc-server's
        // multi-rect damage tracking). Naive bbox merging blows up when
        // two small rects arrive far apart — e.g. a cursor moving across
        // the screen produces a single FRAME covering the entire diagonal,
        // which our partial-update path then has to re-upload as if
        // everything changed.
        //
        // Better: if merging would inflate the dirty area more than 2×
        // the sum of the two rects on their own, flush the existing one
        // *now* as its own FRAME and start a fresh accumulator with the
        // new rect. Adjacent / overlapping rects still merge cheaply.
        const int existing_area = that->update_info.w * that->update_info.h;
        const int new_area      = w * h;
        const int left_x   = std::min(x, that->update_info.x);
        const int top_y    = std::min(y, that->update_info.y);
        const int right_x  = std::max(x + w, that->update_info.x + that->update_info.w);
        const int bottom_y = std::max(y + h, that->update_info.y + that->update_info.h);
        const int merged_w = right_x - left_x;
        const int merged_h = bottom_y - top_y;
        const long merged_area = static_cast<long>(merged_w) * merged_h;
        const long sum_area    = static_cast<long>(existing_area) + new_area;

        if (merged_area > sum_area * 2)
        {
            // Bad merge — flush the existing bbox as its own FRAME and
            // restart accumulating with the new rect. This bypasses the
            // repaint() throttle for this one flush, but each rect is
            // small so wire/CPU cost stays low; the server-side fps cap
            // protects against pathological flood.
            const int mode = (that->repaint_mode == repaint_modes::standard)
                ? that->standard_waveform_mode
                : that->fast_waveform_mode;
            that->device.update(
                that->update_info.x, that->update_info.y,
                that->update_info.w, that->update_info.h, mode);
            that->update_info.x = x;
            that->update_info.y = y;
            that->update_info.w = w;
            that->update_info.h = h;
            that->update_info.has_update = true;
            return;
        }

        that->update_info.x = left_x;
        that->update_info.y = top_y;
        that->update_info.w = merged_w;
        that->update_info.h = merged_h;
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

void screen::set_effective_orientation(vnsee::Orientation o)
{
    if (o == vnsee::Orientation::Auto) return; // no-op for Auto sentinel
    auto prev = effective_.exchange(o);
    if (prev == o) return;

    std::cerr << "Switching blit orientation: "
              << vnsee::orientation_to_string(prev) << " → "
              << vnsee::orientation_to_string(o) << '\n';

    // Mark the entire source as dirty so the next event-loop tick re-blits
    // the full image with the new rotation. update_info is touched only
    // from the main thread elsewhere; flipping a bool + writing rect ints
    // is benign here even without a lock since the worst case is a
    // single-frame race that resolves on the following repaint cycle.
    if (src_width_ > 0 && src_height_ > 0)
    {
        update_info.x = 0;
        update_info.y = 0;
        update_info.w = src_width_;
        update_info.h = src_height_;
        update_info.has_update = true;
    }
}

} // namespace app
