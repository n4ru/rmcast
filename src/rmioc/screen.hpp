#ifndef RMIOC_SCREEN_HPP
#define RMIOC_SCREEN_HPP

#include "qtfb-client.h"
#include "vncast_client.hpp"
#include <cstdint>
#include <memory>

namespace mxcfb
{
    struct update_data;
}

namespace rmioc
{

/**
 * Information about the location of a RGB component in a packed pixel.
 */
struct component_format
{
    /** Offset of the first bit of the component. */
    unsigned short offset;

    /** Number of contiguous bits used to represent the component. */
    unsigned short length;

    /** Maximum value. */
    std::uint32_t max() const;
}; // struct component_format

class screen
{
public:
    /**
     * Connect to a framebuffer server. Picks the vncast qtfb client
     * (rm-cast) when $VNCAST_QTFB_SOCKET is set, otherwise falls back
     * to AppLoad's qtfb client. Both expose the same 16-bit RGB565
     * surface to the rest of vnsee.
     */
    screen();

    void update(
        int x, int y, int w, int h,
        int mode = REFRESH_MODE_UI,
        bool wait = false);

    void update(
        int mode = REFRESH_MODE_UI,
        bool wait = true);

    std::uint8_t* get_data();

    int get_xres() const;
    int get_yres() const;

    unsigned short get_bits_per_pixel() const;
    component_format get_red_format() const;
    component_format get_green_format() const;
    component_format get_blue_format() const;

    auto get_connection() -> qtfb::ClientConnection&;

    /**
     * Push the cursor hotspot through the active backend so the renderer
     * can tag the cursor area with a fast EPDC waveform. Default sprite
     * size of 64x64 covers typical OS cursors. No-op under AppLoad qtfb.
     */
    void update_cursor(int x, int y, bool visible);

    /**
     * True when this screen is talking to vncast (rm-cast) instead of
     * AppLoad qtfb. vncast doesn't (yet) have an input-forwarding side
     * channel, so callers that pull touch/pen/keyboard events out of
     * qtfb must skip when this is true.
     */
    bool is_vncast() const { return using_vncast_; }

private:
    /** True when we're talking to vncast (rm-cast) instead of AppLoad qtfb. */
    bool using_vncast_ = false;

    /** AppLoad qtfb connection. nullptr when using_vncast_ is true. */
    std::unique_ptr<qtfb::ClientConnection>   qtfb_;

    /** vncast qtfb connection. nullptr when using_vncast_ is false. */
    std::unique_ptr<vncast::ClientConnection> vncast_;
}; // class screen

} // namespace rmioc

#endif // RMIOC_screen_HPP
