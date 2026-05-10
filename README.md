# rmcast

[![reMarkable Paper Pro is supported](https://img.shields.io/badge/rMPP-supported-green)](https://remarkable.com/products/remarkable-paper/pro)
[![reMarkable Paper Pro Move is supported](https://img.shields.io/badge/rMPPM-supported-green)](https://remarkable.com/products/remarkable-paper/pro-move)
[![reMarkable 2 is supported](https://img.shields.io/badge/rM2-supported-green)](https://remarkable.com/products/remarkable-2)
[![reMarkable 1 is supported](https://img.shields.io/badge/rM1-supported-green)](https://remarkable.com/products/remarkable-1)

The rMPP-side tablet client of **rm-cast** — an end-to-end display-mirroring stack for reMarkable tablets. Tap **Cast** in xochitl's home sidebar, point it at a host, see your desktop on the e-ink panel.

Forked from [VNSee-QTFB](https://github.com/asivery/vnsee-qtfb) (which itself forked [matteodelabre/vnsee](https://github.com/matteodelabre/vnsee)). We renamed and pulled significantly off the original methods to make the client a first-class part of the rm-cast pipeline rather than a standalone VNC viewer launched from AppLoad.

## What's in this repo

```
rmcast/
├── src/                  ← VNC client (libvncclient + qtfb shm backend)
├── vendor/libvncserver/  ← upstream libvncserver as a submodule
├── patches/              ← local libvncserver patches applied at build time
└── middleware/           ← the rMPP-side glue (xovi extension, qmldiff, scripts)
    ├── extension/        ← vncast.so source — loads into xochitl, hosts shm,
    │                       paints frames into a QML overlay, routes input
    ├── installer/        ← end-user install bundle (bundle.sh / install.sh)
    ├── scripts/          ← cross-build helpers, touch injection, EPDC dumps
    └── docs/             ← architecture + protocol notes
```

The companion **server** lives in [`n4ru/rewire`](https://github.com/n4ru/rewire) — a Windows RFB server using DXGI Desktop Duplication and the custom `rcastmono1z` 1-bit RLE encoding (see below).

## Pipeline at a glance

```
┌─────────────────┐                ┌─────────────────────────┐
│  rewire         │                │  rmcast (this repo)     │
│  (Windows)      │                │                         │
│                 │  RFB 3.8 over  │  vnsee process          │
│  DXGI capture   │  TCP (mono1z   │   ↓ qtfb shm            │
│  + RFB server   │ ─────────────► │  vncast.so              │
│                 │  preferred)    │   ↓ QML Image           │
│  pen events ◄── │                │  xochitl scene → EPDC   │
└─────────────────┘                └─────────────────────────┘
```

`rmcast` works with **any** RFB-3.8-compatible server; the rm-cast-specific encodings just make it faster. See the fallback ladder below.

## Wire-level changes (and graceful fallback)

We added two private RFB pseudo-encodings on top of standard RFB. Both are **negotiated, never required** — if the server doesn't recognize them, RFB ignores unknown encoding IDs and the client falls through to the standard set, no breakage.

| Encoding ID | Name           | Wire payload                                                                             | When picked                                            |
|-------------|----------------|------------------------------------------------------------------------------------------|--------------------------------------------------------|
| **−791**    | `rcastmono1z`  | `uint8 threshold` + `uint32 BE compressed_len` + RLE-packed 1-bit luma                    | Server's first preference if it implements it          |
| **−789**    | `rcastmono1`   | `uint8 threshold` + `ceil(w/8)·h` raw packed 1-bit luma                                   | Fallback if mono1z isn't supported                     |
| `0`–`16`    | standard       | Raw / CopyRect / Tight / ZRLE / Hextile / etc.                                            | Fallback if neither mono1* is supported                |

Wire format details and the RLE control-byte layout: see [middleware/PROTOCOL.md](middleware/PROTOCOL.md).

The client always advertises every encoding it knows in priority order: `rcastmono1z rcastmono1 copyrect tight zrle hextile raw`. The server picks the highest-priority one it supports for each rect. So:

- **rewire host**: every rect uses `rcastmono1z` → typical text content compresses ~5–15× over raw mono1, ~40× over raw BGRA, ~1240× over uncompressed-color. WiFi is no longer the bottleneck for typing or scrolling.
- **stock VNC server (TigerVNC, x11vnc, TightVNC, …)**: drops to Tight / Hextile / Raw, exactly as upstream VNSee. Slower, but works.

The same fallback logic applies to the in-rMPP transport (`qtfb` shm between `vnsee` and `vncast.so`): the qtfb HELLO negotiates pixel format (8-bit grayscale or 32-bit RGBA) and an optional fps cap; either side can downgrade if the other doesn't support a feature. See [middleware/extension/src/qtfb/protocol.h](middleware/extension/src/qtfb/protocol.h) for the wire format.

## Install

For end users, see [middleware/installer/README.md](middleware/installer/README.md). Three scripts:

- `bundle.sh` (dev machine) — packages `vncast.so` + `vncast-menu-icon.qmd` + cross-built `vnsee` binary into `dist/rmcast-<ver>-<ts>.tar.gz`.
- `install.sh` (rMPP) — backs up existing files, atomic-replaces them, restarts xochitl. Refuses to run on non-aarch64.
- `uninstall.sh` (rMPP) — restores most-recent `.bak`, restarts xochitl.

Standard end-user flow:

```sh
# from your dev machine
scp dist/rmcast-<ver>-<ts>.tar.gz root@<rmpp-ip>:/tmp/

# on the rMPP
cd /tmp && tar xzf rmcast-<ver>-<ts>.tar.gz && cd rmcast-<ver>-<ts>
./install.sh
```

The Cast item appears in xochitl's home sidebar after restart.

## Build (from source)

Pre-reqs:

- The reMarkable Paper Pro Codex SDK (`5.2.96-dirty`, ferrari) installed at `~/codex/ferrari/5.2.96-dirty`.
- Rust toolchain (for the qmldiff hashing tool).
- An rMPP running xochitl 3.27.x with **xovi** + **qt-resource-rebuilder** already installed.

```sh
cd middleware/scripts
./build-vnsee-ferrari.sh    # cross-builds the vnsee binary (applies patches/*.patch
                            # against vendor/libvncserver during build)
./build-qmd.sh              # hashes menu-icon.qmldiff against the device's hashtab
./build-extension.sh        # cross-builds vncast.so via the codex SDK
cd ../installer
./bundle.sh                 # packages everything into dist/
```

The libvncserver patches in `patches/` add the `rcastmono1` and `rcastmono1z` decoders. We don't fork the entire libvncserver tree — we vendor it as a submodule and apply our diffs at build time, idempotently (a stamp file in `build-ferrari/.patches-applied` records what's applied so re-runs are no-ops).

## Differences from upstream VNSee-QTFB

What we kept:
- Most of the libvncclient plumbing in `src/`.
- The qtfb shm protocol from rm-appload (extended with our HELLO-negotiated fps cap and grayscale shm format).
- asivery's per-frame waveform-mode hint mechanism (`MESSAGE_SET_REFRESH_MODE`) for fast EPDC refreshes during interactive content.

What we changed:
- **Decoupled from AppLoad.** The client used to ship as an AppLoad app; rm-cast hosts it via its own xovi extension (`vncast.so`), which provides the qtfb server, paint surface, and input forwarding directly inside xochitl.
- **Two new pseudo-encodings** (`rcastmono1`, `rcastmono1z`) for B&W desktop content. Decoders live in our libvncserver patch so they expand to whatever pixel format the client is in.
- **Source-side rotation** (`blit_rotated` in `src/app/screen.cpp`) so a landscape Windows desktop fits a portrait rMPP panel without stride wrap.
- **No `/dev/fb0` writes.** Frames go to the qtfb shm; QML's `FrameView` blits to the panel via xochitl's scene, which means we cooperate with the rest of xochitl's UI (cursor overlays, EPDC waveform tagging, etc.) instead of fighting it.
- **Latency carve-out** in the qtfb server: small dirty rects (cursor blink, pen strokes, keystrokes) bypass the fps cap and emit `frameReady` immediately. Large/full-frame updates still respect the cap.

What we dropped:
- `/dev/input/virtual_keyboard` integration (we have no need for an on-device keyboard layer; the Windows side handles that).
- The `vnsee-gui` script.
- Some upstream RFB encoding paths we don't use (`zlib`, `zlibhex`, `ultra`, `corre`, `rre`) — the negotiation still advertises a usable set and falls through correctly, but our code paths exercise `Tight` / `CopyRect` / `Raw` / `Hextile` / `ZRLE` plus our two private encodings.

## Disclaimer

This project is not affiliated with, nor endorsed by, [reMarkable AS](https://remarkable.com/).
**No responsibility is taken for damage done to your device by running this software.**

## Acknowledgments

rm-cast / rmcast wouldn't exist without the work that came before:

- [Mattéo Delabre](https://github.com/matteodelabre) — the original [VNSee](https://github.com/matteodelabre/vnsee), on which everything here is built. [Sponsor him](https://github.com/sponsors/matteodelabre).
- [@khyryra](https://github.com/khyryra) — the [VNSee-QTFB](https://github.com/asivery/vnsee-qtfb) port we forked from.
- [asivery](https://github.com/asivery) — [rm-appload](https://github.com/asivery/rm-appload), the qtfb shm protocol, the `xovi` extension model, and `qt-resource-rebuilder` (without which `qmldiff` wouldn't be possible).
- [Noa Himesaka](https://github.com/NoaHimesaka1873), [GreySim](https://github.com/), [notfrants](https://github.com/notfrants), [ingatellent](https://github.com/ingatellent), and the [reMarkable Discord](https://discord.gg/u3P9sDW) testers from the upstream VNSee-QTFB acknowledgments.
- Original VNSee acknowledgments preserved: [@asmanur](https://github.com/asmanur) (repaint-latency improvements that we still benefit from), [@mhhf](https://github.com/mhhf), [@rowancallahan](https://github.com/rowancallahan), [@Axenntio](https://github.com/Axenntio), [@torwag](https://github.com/torwag), [libremarkable](https://github.com/canselcik/libremarkable), [FBInk](https://github.com/NiLuJe/FBInk).
- [Martin Sandsmark](https://github.com/sandsmark)'s [revncable](https://github.com/sandsmark/revncable) for the reference design of a Qt-only RFB client.

## License

GPL v3 (inherited from upstream VNSee).
