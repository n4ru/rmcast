// epdc_hook.cpp — runtime dlopen+dlsym shim for libqsgepaper.so's
// EPFramebuffer::swapBuffers(QRect, EPContentType, EPScreenMode, QFlags<UpdateFlag>).
//
// We can't use xovi's load-time `override` because libqsgepaper.so is a
// Qt scenegraph plugin loaded AFTER xovi extensions; the override
// symbol resolution fails and xochitl crashes on boot.
//
// Instead, lazily resolve the symbol after Qt has had time to load the
// scenegraph plugin (first call to ensure_resolved()), and provide a
// helper that callers (FrameView::onFrameReady) can use to push our own
// swapBuffers with the chosen waveform.

#include "epdc_hook.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>

struct CQRect { int x1, y1, x2, y2; };

namespace {

constexpr const char *kLibPath = "/usr/lib/plugins/scenegraph/libqsgepaper.so";
constexpr const char *kSwapSymbol =
    "_ZN13EPFramebuffer11swapBuffersE5QRect13EPContentType12EPScreenMode"
    "6QFlagsINS_10UpdateFlagEE";
constexpr const char *kInstanceSymbol = "_ZN13EPFramebuffer8instanceEv";

using SwapFn     = void (*)(void *self, CQRect rect, int contentType,
                            int screenMode, int flags);
using InstanceFn = void* (*)();

std::once_flag         g_resolve_once;
SwapFn                 g_swap     = nullptr;
InstanceFn             g_instance = nullptr;
std::atomic<bool>      g_active{false};
std::atomic<int>       g_force_mode{-1};

void resolve_once() {
    void *h = dlopen(kLibPath, RTLD_NOW | RTLD_GLOBAL);
    if (!h) {
        std::fprintf(stderr, "[vncast/epdc] dlopen(%s) failed: %s\n",
                     kLibPath, dlerror());
        return;
    }
    g_swap     = reinterpret_cast<SwapFn>    (dlsym(h, kSwapSymbol));
    g_instance = reinterpret_cast<InstanceFn>(dlsym(h, kInstanceSymbol));
    std::fprintf(stderr,
        "[vncast/epdc] resolved swapBuffers=%p instance=%p\n",
        reinterpret_cast<void *>(g_swap),
        reinterpret_cast<void *>(g_instance));
}

}  // namespace

namespace vncast::epdc {

void setActive(bool active)  { g_active.store(active); }
void setForceMode(int mode)  { g_force_mode.store(mode); }

void requestUpdate(int x1, int y1, int x2, int y2,
                   int contentType, int screenMode, int flags)
{
    std::call_once(g_resolve_once, resolve_once);
    if (!g_swap || !g_instance) return;

    int forced = g_force_mode.load(std::memory_order_relaxed);
    if (g_active.load(std::memory_order_relaxed) && forced >= 0) {
        screenMode = forced;
    }

    void *self = g_instance();
    if (!self) return;
    g_swap(self, CQRect{x1, y1, x2, y2}, contentType, screenMode, flags);
}

}  // namespace vncast::epdc
