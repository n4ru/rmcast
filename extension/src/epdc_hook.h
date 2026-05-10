#pragma once
// Lazy dlsym shim + inline aarch64 hook for libqsgepaper.so's
// EPFramebuffer::swapBuffers. Lets us observe (logging) and influence
// (forced waveform) the Qt-side EPDC call without xochitl knowing.
namespace vncast::epdc {

// Resolve the symbol and patch the first 16 bytes of swapBuffers with a
// jump into our handler. Idempotent. Safe no-op if the lib isn't loaded
// yet or the symbol can't be resolved.
void installHook();

// While true, print rect+contentType+screenMode+flags for every Qt-driven
// swap. Used to map xochitl's EPScreenMode integers → A2/DU/GC16/GLR16 by
// observing what shows up during known UI states.
void setLogging(bool on);

// While active && force_mode >= 0, override the screenMode arg of every
// swapBuffers call with force_mode. Set both to take effect.
void setActive(bool active);
void setForceMode(int mode);

// Manual swapBuffers push (uses dlsym, not the inline hook). Currently
// unused — kept for the per-region tagging path that will issue its own
// composed update via overload 2.
void requestUpdate(int x1, int y1, int x2, int y2,
                   int contentType = 0, int screenMode = 0, int flags = 0);

}  // namespace vncast::epdc
