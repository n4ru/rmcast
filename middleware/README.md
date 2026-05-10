# rm-cast

End-to-end display mirroring for the reMarkable family. Tap a **Cast** icon in
the home sidebar, point it at a host, see your desktop on the e-ink panel.

```
┌─────────────────┐                                ┌──────────────────┐
│ rcast-host      │ ──── RFB + rmcast/1 ────────── │ vnsee fork       │
│ (Windows)       │                                │ (rM)             │
│  IDD virtual    │                                │  reads frames    │
│  display +      │                                │  from a shm      │
│  tuned VNC      │                                │  region          │
│  server         │                                │                  │
└─────────────────┘                                │      qtfb        │
                                                   │       │          │
                                                   │       ▼          │
                                                   │ vncast.so        │
                                                   │  (xovi ext.)     │
                                                   │  paints shm      │
                                                   │  into xochitl    │
                                                   │  via FBController│
                                                   └──────────────────┘
```

The project ships three things, in this repo and a sibling vnsee fork:

| Component       | Role                                                   | Status      |
|-----------------|--------------------------------------------------------|-------------|
| `vncast.so`     | xovi extension on the rM. Sidebar icon → config UI →   | landed      |
|                 | spawns vnsee, hosts the qtfb shm, paints the frames    | (UI; qtfb   |
|                 | into xochitl's QML scene                               |  in flight) |
| `vnsee` fork    | VNC client on the rM. Backend that writes frames into  | scaffolded  |
|                 | the qtfb shm instead of `/dev/fb0`                     |             |
| `rcast-host`    | Windows companion. IDD virtual display + tuned VNC     | not yet     |
|                 | server speaking the `rmcast/1` pseudo-encoding         |             |

Devices supported by the rM-side bits:

- **rMPP** (Paper Pro) — primary target, panel 1620×2160, qtfb required.
- **rMPPM** (Paper Pro Move) — panel size detected at runtime; qtfb required.
- **rM2** / **rM1** — `/dev/fb0` works, qtfb path is bypassed.

## Status

Early WIP. See [docs/architecture.md](docs/architecture.md) for the design and
[PROTOCOL.md](PROTOCOL.md) for the over-the-wire `rmcast/1` extensions.

## Layout

```
extension/                   ← the xovi extension (vncast.so)
├── vncast.xovi              ← descriptor, depends-on qt-resource-rebuilder
├── menu-icon.qmldiff        ← source qmldiff that adds Cast to the sidebar
│                              and hosts the in-MainView Loader (text
│                              identifiers, hashed at build time)
├── application.qrc          ← QML resource bundle
├── icons/vnsee.svg          ← monochrome monitor icon
├── qml/
│   ├── launcher.qml         ← stage Loader: config → session
│   ├── config.qml           ← settings-style connect form
│   ├── session.qml          ← placeholder; FBController paint lands here
│   ├── SegmentRow.qml       ← native-style pill picker
│   ├── OutlineButton.qml    ← Cancel-style outlined action button
│   └── FilledButton.qml     ← Connect/Disconnect "Turn off"-style button
└── src/
    ├── main.cpp             ← xovi entry; registers QML singletons + QRC
    ├── launcher.{h,cpp}     ← Vncast singleton (overlay open/close, spawn)
    ├── settings.{h,cpp}     ← persisted config in ~/.config/vncast.json
    ├── detect.{h,cpp}       ← runtime device + framebuffer detection
    └── qtfb/
        ├── protocol.h       ← wire format for the in-rM qtfb socket
        ├── server.{h,cpp}   ← Unix socket + shm allocator
        └── fbcontroller.*   ← QML-instantiable item that paints the shm

vnsee/                       ← submodule → n4ru/vnsee
                              (the actual VNC client; gains a qtfb backend)

scripts/
├── build-qmd.sh             ← run qmldiff to convert .qmldiff → .qmd
├── verify-qmd-resolvable.sh ← refuse to deploy a qmd with stale hashes
├── build-extension.sh       ← cross-build vncast.so via codex SDK
├── deploy.sh                ← scp the .so + .qmd, restart xochitl
├── trigger-screenshot.sh    ← call rm-shot via the xovi-mb FIFO
├── tap.py                   ← inject a tap event on the device touchscreen
└── tap-and-shot.sh          ← tap + screenshot helper for UI iteration
```

## Build prerequisites

- The reMarkable Paper Pro Codex SDK (5.2.96-dirty, ferrari):
  installed at `~/codex/ferrari/5.2.96-dirty` per the build scripts.
- Rust (stable) — for building the qmldiff tool one-time, then we cache its
  binary. `apt install -y rustc cargo` is enough on Ubuntu/WSL.
- An rMPP running xochitl 3.27.x with **xovi** + **qt-resource-rebuilder**
  already installed (this extension does not bundle xovi itself).

## How the icon hashes work

xochitl's QML element identifiers are hashed (djb2 with seed 5481) into the
qt-resource-rebuilder's `hashtab` at boot. The `.qmd` file in this repo
references xochitl elements by hash. When xochitl ships a new version that
renames or removes an element, our hashes go stale and the icon fails to
land — the fix is to:

1. Pull a fresh hashtab from your rMPP via
   `scripts/extract-hashtab.sh`.
2. Re-run `scripts/build-qmd.sh` to re-hash `menu-icon.qmldiff` against the
   new hashtab.
3. `scripts/verify-qmd-resolvable.sh` will refuse to deploy if any hash in
   the qmd doesn't resolve — keeps us out of the panic loops we hit early on
   (where AppLoad's pre-hashed qmd referenced a now-removed identifier).
4. Rebuild + redeploy.

(This is why `menu-icon.qmldiff` is the source of truth in the repo, not
`menu-icon.qmd`.)
