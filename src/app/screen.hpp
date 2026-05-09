#ifndef APP_SCREEN_HPP
#define APP_SCREEN_HPP

#include "event_loop.hpp"
#include "../orientation.hpp"
#include "../rmioc/screen.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <vector>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

namespace chrono = std::chrono;

namespace rmioc
{
    class screen;
}

namespace app
{

class screen
{
public:
    screen(
        rmioc::screen& device,
        rfbClient* vnc_client,
        vnsee::Orientation orientation = vnsee::Orientation::Auto
    );

    event_loop_status event_loop();

    /**
     * Transform input coordinates from fb0 (panel) space into server space,
     * applying the inverse of the current display rotation. No-op for
     * Portrait orientation.
     */
    void transform_input(int& x, int& y) const;

    /**
     * Resolved orientation (Auto turns into Portrait or LandscapeCW after
     * the framebuffer is initialized).
     */
    vnsee::Orientation effective_orientation() const { return effective_.load(); }

    /**
     * Switch the active blit rotation at runtime (e.g. from the
     * accelerometer-driven orientation sensor) and trigger a full repaint
     * so the new orientation lands on the panel immediately.
     */
    void set_effective_orientation(vnsee::Orientation o);

    /**
     * Force flushing any pending updates to the screen.
     */
    void repaint();

    /**
     * Get the number of usable pixel columns on the screen.
     */
    int get_xres();

    /**
     * Get the number of usable pixel rows on the screen.
     */
    int get_yres();

    /**
     * Available repaint modes.
     */
    enum class repaint_modes
    {
        /**
         * High quality repaints with ~450 ms latency.
         *
         * In this mode, updates are throttled …
         */
        standard = 0,

        /**
         * Black-and-white repaints with ~260 ms latency.
         *
         * Does not clear the flag for pending updates. Is only meant for
         * transitional updates and must be followed by a standard repaint to
         * fully flush pending updates.
         */
        fast = 1
    };

    void set_repaint_mode(repaint_modes mode);

private:
    /** reMarkable screen device. */
    rmioc::screen& device;

    /** VNC connection. */
    rfbClient* vnc_client;

    /**
     * Called by the VNC client library to initialize our local framebuffer.
     *
     * @param client Handle to the VNC client.
     */
    static rfbBool create_framebuf(rfbClient* client);

    /**
     * Called by the VNC client library when a bitmap rectangle is received
     * from the server.
     *
     * @param client Handle to the VNC client.
     * @param buffer Buffer containing the received update.
     * @param x Left bound of the updated rectangle (in pixels).
     * @param y Top bound of the updated rectangle (in pixels).
     * @param w Width of the updated rectangle (in pixels).
     * @param h Height of the updated rectangle (in pixels).
     */
    static void recv_update(
        rfbClient* client,
        const uint8_t* buffer,
        int x, int y, int w, int h
    );

    /**
     * Called by the VNC client library when a server update is completed.
     *
     * @param client Handle to the VNC client.
     * @param x Left bound of the updated rectangle (in pixels).
     * @param y Top bound of the updated rectangle (in pixels).
     * @param w Width of the updated rectangle (in pixels).
     * @param h Height of the updated rectangle (in pixels).
     */
    static void commit_updates(
        rfbClient* client,
        int x, int y, int w, int h
    );

    /** Accumulator for updates received from the VNC server. */
    struct update_info_struct
    {
        /** Left bound of the overall updated rectangle (in pixels). */
        int x = 0;

        /** Top bound of the overall updated rectangle (in pixels). */
        int y = 0;

        /** Width of the overall updated rectangle (in pixels). */
        int w = 0;

        /** Height of the overall updated rectangle (in pixels). */
        int h = 0;

        /** Whether at least one update has been registered. */
        bool has_update = false;
    } update_info;

    /** Last time a repaint was performed. */
    std::chrono::steady_clock::time_point last_repaint;

    /** Tag used for accessing the instance from C callbacks. */
    static void* instance_tag;

    /** Current repaint mode. */
    repaint_modes repaint_mode;

    chrono::milliseconds standard_repaint_delay;

    chrono::milliseconds fast_repaint_delay;

    int standard_waveform_mode;

    int fast_waveform_mode;

    /** Requested orientation (Auto is resolved on framebuffer init). */
    vnsee::Orientation requested_;

    /**
     * Resolved orientation actually applied during blit. Atomic because the
     * orientation sensor thread can flip it while the main thread reads it
     * during blit_rotated / transform_input.
     */
    std::atomic<vnsee::Orientation> effective_;

    /**
     * Intermediate buffer that libvncclient writes into.
     * Sized to source (server) dimensions × bytes-per-pixel so libvncclient's
     * row stride matches its own width — no wrap into fb0.
     */
    std::vector<std::uint8_t> src_buffer_;

    /** Source (server-reported) dimensions, set in create_framebuf. */
    int src_width_  = 0;
    int src_height_ = 0;

    /** Bytes per pixel for both source and fb0 (vnsee assumes 16-bit RGB565). */
    int bytes_per_pixel_ = 2;

    /**
     * Copy a dirty rectangle from the source-shape intermediate buffer into
     * the panel-shape fb0 buffer, applying the current rotation. Returns the
     * resulting rectangle in fb0 (panel) coordinates via the out params.
     */
    void blit_rotated(
        int sx, int sy, int sw, int sh,
        int& dx, int& dy, int& dw, int& dh);
}; // class screen

} // namespace app

#endif // APP_SCREEN_HPP
