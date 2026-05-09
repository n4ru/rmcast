#pragma once
// Lazy dlsym shim for libqsgepaper.so's EPFramebuffer::swapBuffers.
namespace vncast::epdc {

void setActive(bool active);
void setForceMode(int mode);

// Push a manual swapBuffers with the chosen waveform. Caller decides
// the rect (in panel coordinates), contentType, and any flags.
// Resolves the symbol on first call and silently no-ops if not found.
void requestUpdate(int x1, int y1, int x2, int y2,
                   int contentType = 0, int screenMode = 0, int flags = 0);

}  // namespace vncast::epdc
