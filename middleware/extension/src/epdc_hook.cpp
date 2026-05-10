// epdc_hook.cpp — runtime dlopen+dlsym + inline aarch64 hook for
// libqsgepaper.so's EPFramebuffer::swapBuffers(QRect, EPContentType,
// EPScreenMode, QFlags<UpdateFlag>).
//
// We can't use xovi's load-time `override` because libqsgepaper.so is a
// Qt scenegraph plugin loaded AFTER xovi extensions; the override
// symbol resolution fails and xochitl crashes on boot.
//
// Instead, lazily resolve the symbol once Qt has had time to load the
// scenegraph plugin, then patch the first 16 bytes of swapBuffers with
// a trampoline into our handler. The handler logs the args (when
// logging is on) and may rewrite the screenMode arg before tail-calling
// the original via a saved-prologue thunk.

#include "epdc_hook.h"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#include <QRegion>
#include <QRect>
#include <QHash>

struct CQRect { int x1, y1, x2, y2; };

namespace {

constexpr const char *kLibPath = "/usr/lib/plugins/scenegraph/libqsgepaper.so";
constexpr const char *kSwapSymbol =
    "_ZN13EPFramebuffer11swapBuffersE5QRect13EPContentType12EPScreenMode"
    "6QFlagsINS_10UpdateFlagEE";
// Per-region overload — base class dispatch wrapper. Vtable resolves to
// the Acep2 _impl below for the rMPP's Gallery 3 panel; hook both so we
// catch the call no matter how xochitl reaches it.
constexpr const char *kSwapRegionSymbol =
    "_ZN13EPFramebuffer11swapBuffersERK7QRegionRK12EPContentMapRK"
    "15EPScreenModeMap6QFlagsINS_10UpdateFlagEE";
// EPFramebufferAcep2::swapBuffers_impl(QRegion, EPContentMap, EPScreenModeMap, UpdateFlag)
// The actual panel-level update implementation for the rMPP. xochitl
// almost certainly arrives here via virtual dispatch from the base.
constexpr const char *kSwapImplSymbol =
    "_ZN18EPFramebufferAcep216swapBuffers_implERK7QRegionRK12EPContentMapRK"
    "15EPScreenModeMap6QFlagsIN13EPFramebuffer10UpdateFlagEE";
constexpr const char *kInstanceSymbol = "_ZN13EPFramebuffer8instanceEv";

using SwapFn       = void (*)(void *self, CQRect rect, int contentType,
                              int screenMode, int flags);
using SwapRegionFn = void (*)(void *self, const QRegion *region,
                              const void *contentMap, const void *modeMap,
                              int flags);
using InstanceFn   = void* (*)();

std::once_flag         g_resolve_once;
std::once_flag         g_install_once;
SwapFn                 g_swap                   = nullptr;
SwapRegionFn           g_swap_region            = nullptr;
SwapRegionFn           g_swap_impl              = nullptr;   // Acep2 _impl
InstanceFn             g_instance               = nullptr;
SwapFn                 g_thunk_to_orig          = nullptr;   // simple-rect overload
SwapRegionFn           g_thunk_to_orig_region   = nullptr;   // per-region base
SwapRegionFn           g_thunk_to_orig_impl     = nullptr;   // Acep2 _impl
std::atomic<bool>      g_hook_installed{false};
std::atomic<bool>      g_log_swaps{false};
std::atomic<bool>      g_active{false};
std::atomic<int>       g_force_mode{-1};
std::atomic<uint64_t>  g_swap_count{0};
std::atomic<uint64_t>  g_swap_region_count{0};
std::atomic<uint64_t>  g_swap_impl_count{0};

void resolve_once() {
    void *h = dlopen(kLibPath, RTLD_NOW | RTLD_GLOBAL);
    if (!h) {
        std::fprintf(stderr, "[vncast/epdc] dlopen(%s) failed: %s\n",
                     kLibPath, dlerror());
        return;
    }
    g_swap        = reinterpret_cast<SwapFn>      (dlsym(h, kSwapSymbol));
    g_swap_region = reinterpret_cast<SwapRegionFn>(dlsym(h, kSwapRegionSymbol));
    g_swap_impl   = reinterpret_cast<SwapRegionFn>(dlsym(h, kSwapImplSymbol));
    g_instance    = reinterpret_cast<InstanceFn>  (dlsym(h, kInstanceSymbol));
    std::fprintf(stderr,
        "[vncast/epdc] resolved swap=%p region=%p impl=%p instance=%p\n",
        reinterpret_cast<void *>(g_swap),
        reinterpret_cast<void *>(g_swap_region),
        reinterpret_cast<void *>(g_swap_impl),
        reinterpret_cast<void *>(g_instance));
}

// ---- inline hook (aarch64 only) ----------------------------------------
//
// Trampoline written at original symbol (16 bytes):
//   LDR  x16, [pc, #8]    ; 0x58000050
//   BR   x16              ; 0xD61F0200
//   .quad &hooked_swap
//
// Thunk we allocate (32 bytes), executable:
//   <16 bytes copied from original prologue>
//   LDR  x16, [pc, #8]
//   BR   x16
//   .quad (orig_addr + 16)
//
// Calling g_thunk_to_orig executes the saved prologue then resumes the
// original function past the patched bytes. Works iff the first 16 bytes
// contain no PC-relative instructions (ADR/ADRP/literal LDR/B/BL/CB/TB).
// Standard Itanium-ABI method prologues (stp x29,x30 / mov x29,sp / sub
// sp,...) are not PC-relative, so this is safe for typical C++ methods.

constexpr size_t kPrologueBytes = 16;

void hooked_swap(void *self, CQRect rect, int ct, int sm, int fl) {
    g_swap_count.fetch_add(1, std::memory_order_relaxed);
    if (g_log_swaps.load(std::memory_order_relaxed)) {
        std::fprintf(stderr,
            "[vncast/epdc] swap rect=(%d,%d)-(%d,%d) ct=%d sm=%d fl=%d\n",
            rect.x1, rect.y1, rect.x2, rect.y2, ct, sm, fl);
    }
    int forced = g_force_mode.load(std::memory_order_relaxed);
    if (g_active.load(std::memory_order_relaxed) && forced >= 0) {
        sm = forced;
    }
    if (g_thunk_to_orig) g_thunk_to_orig(self, rect, ct, sm, fl);
}

void hooked_swap_region(void *self, const QRegion *region,
                        const void *cmap, const void *mmap_, int fl)
{
    g_swap_region_count.fetch_add(1, std::memory_order_relaxed);
    if (g_log_swaps.load(std::memory_order_relaxed)) {
        // QRegion::rectCount + boundingRect are O(1), safe to call.
        // Iterate and sample the screen-mode map values to learn the
        // EPScreenMode integers xochitl uses per UI region.
        QRect bb = region->boundingRect();
        int nrects = region->rectCount();

        // EPScreenModeMap = QHash<QRect, EPScreenMode>; assume EPScreenMode
        // is a 4-byte enum (the aarch64 default).
        const auto *mmap = static_cast<const QHash<QRect, int> *>(mmap_);
        const auto *ctmap = static_cast<const QHash<QRect, int> *>(cmap);

        // Pull up to a couple of (rect → sm,ct) samples for logging.
        std::fprintf(stderr,
            "[vncast/epdc] swap-region nrects=%d bb=(%d,%d %dx%d) "
            "mmap.size=%lld cmap.size=%lld fl=%d\n",
            nrects, bb.x(), bb.y(), bb.width(), bb.height(),
            static_cast<long long>(mmap->size()),
            static_cast<long long>(ctmap->size()), fl);

        int n_logged = 0;
        for (auto it = mmap->cbegin(); it != mmap->cend() && n_logged < 4; ++it, ++n_logged) {
            const QRect &k = it.key();
            int sm = it.value();
            int ct = ctmap->value(k, -1);
            std::fprintf(stderr,
                "[vncast/epdc]   rect=(%d,%d %dx%d) sm=%d ct=%d\n",
                k.x(), k.y(), k.width(), k.height(), sm, ct);
        }
    }

    // If active+forced, rewrite every entry in the (const) map.
    // We're casting away const: the map is a QHash<QRect, int> and we
    // overwrite the int value at each existing slot. Layout-wise this
    // is the same QHash QtCore links — value(key) returns by-value, but
    // the iterators expose stored-by-ref values we can poke.
    int forced = g_force_mode.load(std::memory_order_relaxed);
    if (g_active.load(std::memory_order_relaxed) && forced >= 0) {
        auto *mmap = const_cast<QHash<QRect, int> *>(
            static_cast<const QHash<QRect, int> *>(mmap_));
        for (auto it = mmap->begin(); it != mmap->end(); ++it) {
            it.value() = forced;
        }
    }

    if (g_thunk_to_orig_region)
        g_thunk_to_orig_region(self, region, cmap, mmap_, fl);
}

void hooked_swap_impl(void *self, const QRegion *region,
                      const void *cmap, const void *mmap_, int fl)
{
    g_swap_impl_count.fetch_add(1, std::memory_order_relaxed);
    if (g_log_swaps.load(std::memory_order_relaxed)) {
        QRect bb = region->boundingRect();
        int nrects = region->rectCount();
        const auto *mmap = static_cast<const QHash<QRect, int> *>(mmap_);
        const auto *ctmap = static_cast<const QHash<QRect, int> *>(cmap);
        std::fprintf(stderr,
            "[vncast/epdc] IMPL nrects=%d bb=(%d,%d %dx%d) "
            "mmap.size=%lld cmap.size=%lld fl=%d\n",
            nrects, bb.x(), bb.y(), bb.width(), bb.height(),
            static_cast<long long>(mmap->size()),
            static_cast<long long>(ctmap->size()), fl);
        int n_logged = 0;
        for (auto it = mmap->cbegin(); it != mmap->cend() && n_logged < 4; ++it, ++n_logged) {
            const QRect &k = it.key();
            int sm = it.value();
            int ct = ctmap->value(k, -1);
            std::fprintf(stderr,
                "[vncast/epdc]   IMPL rect=(%d,%d %dx%d) sm=%d ct=%d\n",
                k.x(), k.y(), k.width(), k.height(), sm, ct);
        }
    }

    int forced = g_force_mode.load(std::memory_order_relaxed);
    if (g_active.load(std::memory_order_relaxed) && forced >= 0) {
        auto *mmap = const_cast<QHash<QRect, int> *>(
            static_cast<const QHash<QRect, int> *>(mmap_));
        for (auto it = mmap->begin(); it != mmap->end(); ++it) {
            it.value() = forced;
        }
    }

    if (g_thunk_to_orig_impl)
        g_thunk_to_orig_impl(self, region, cmap, mmap_, fl);
}

// Patch one symbol with a trampoline into `handler`. Returns the saved
// prologue thunk (callable to invoke the original function) or nullptr on
// failure.
void *install_inline_hook_at(void *target_sym, void *handler, const char *name) {
    if (!target_sym) return nullptr;

    void *thunk = mmap(nullptr, 32,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (thunk == MAP_FAILED) {
        std::fprintf(stderr, "[vncast/epdc] mmap thunk failed: %s\n",
                     std::strerror(errno));
        return nullptr;
    }
    std::memcpy(thunk, target_sym, kPrologueBytes);
    auto *t = reinterpret_cast<uint32_t*>(
        static_cast<uint8_t*>(thunk) + kPrologueBytes);
    t[0] = 0x58000050;  // LDR x16, [pc, #8]
    t[1] = 0xD61F0200;  // BR  x16
    uint64_t cont_addr = reinterpret_cast<uint64_t>(target_sym) + kPrologueBytes;
    std::memcpy(&t[2], &cont_addr, sizeof(cont_addr));
    __builtin___clear_cache(static_cast<char*>(thunk),
                            static_cast<char*>(thunk) + 32);

    auto addr = reinterpret_cast<uintptr_t>(target_sym);
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_addr = addr & ~(static_cast<uintptr_t>(page_size) - 1);
    size_t span = (addr + kPrologueBytes) - page_addr;
    size_t prot_size = ((span + page_size - 1) / page_size) * page_size;

    if (mprotect(reinterpret_cast<void*>(page_addr), prot_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        std::fprintf(stderr, "[vncast/epdc] mprotect rwx failed for %s: %s\n",
                     name, std::strerror(errno));
        munmap(thunk, 32);
        return nullptr;
    }
    auto *p = reinterpret_cast<uint32_t*>(target_sym);
    uint64_t handler_addr = reinterpret_cast<uint64_t>(handler);
    p[0] = 0x58000050;  // LDR x16, [pc, #8]
    p[1] = 0xD61F0200;  // BR  x16
    std::memcpy(&p[2], &handler_addr, sizeof(handler_addr));
    __builtin___clear_cache(reinterpret_cast<char*>(target_sym),
                            reinterpret_cast<char*>(target_sym) + kPrologueBytes);
    mprotect(reinterpret_cast<void*>(page_addr), prot_size,
             PROT_READ | PROT_EXEC);

    std::fprintf(stderr,
        "[vncast/epdc] inline hook installed (%s): orig=%p handler=%p thunk=%p\n",
        name, target_sym, handler, thunk);
    return thunk;
}

bool install_inline_hook_locked() {
    if (g_swap) {
        g_thunk_to_orig = reinterpret_cast<SwapFn>(
            install_inline_hook_at(reinterpret_cast<void*>(g_swap),
                                   reinterpret_cast<void*>(&hooked_swap),
                                   "swapBuffers/rect"));
    }
    if (g_swap_region) {
        g_thunk_to_orig_region = reinterpret_cast<SwapRegionFn>(
            install_inline_hook_at(reinterpret_cast<void*>(g_swap_region),
                                   reinterpret_cast<void*>(&hooked_swap_region),
                                   "swapBuffers/region"));
    }
    if (g_swap_impl) {
        g_thunk_to_orig_impl = reinterpret_cast<SwapRegionFn>(
            install_inline_hook_at(reinterpret_cast<void*>(g_swap_impl),
                                   reinterpret_cast<void*>(&hooked_swap_impl),
                                   "swapBuffers_impl/Acep2"));
    }
    g_hook_installed.store(g_thunk_to_orig != nullptr ||
                           g_thunk_to_orig_region != nullptr ||
                           g_thunk_to_orig_impl != nullptr);
    return g_hook_installed.load();
}

}  // namespace

namespace vncast::epdc {

void setActive(bool active)  { g_active.store(active); }
void setForceMode(int mode)  { g_force_mode.store(mode); }
void setLogging(bool on)     { g_log_swaps.store(on); }

void installHook() {
    std::call_once(g_resolve_once, resolve_once);
    std::call_once(g_install_once, []{
        if (!install_inline_hook_locked()) {
            std::fprintf(stderr, "[vncast/epdc] hook install failed\n");
        }
    });
}

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
    // If the hook is installed, calling g_swap goes through our handler;
    // use the thunk to avoid recursion / double-logging on manual pushes.
    SwapFn target = g_thunk_to_orig ? g_thunk_to_orig : g_swap;
    target(self, CQRect{x1, y1, x2, y2}, contentType, screenMode, flags);
}

}  // namespace vncast::epdc
