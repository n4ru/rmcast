# rmpp-vnsee — architecture

## What this extension does

Adds a "VNSee" icon to xochitl's home burger menu (the same place AppLoad
adds itself today). Tapping the icon opens a small Qt window with three
controls — host IP, refresh waveform, blit orientation — and a Connect
button. Connect spawns the `vnsee` binary in our own qtfb session and
hands the resulting framebuffer to xochitl for display.

When the user drags from the top to dismiss, vnsee exits and the icon
returns to the menu.

```
┌─────────────────── xochitl ────────────────────┐
│  home menu  ──qmldiff──>  [VNSee icon] click   │
│                                  │             │
│  ┌───────────── our xovi ext ────┴──────────┐  │
│  │  popup Qt window                          │  │
│  │  ├── QML config UI (main.qml)             │  │
│  │  └── FBController displays qtfb buffer    │  │
│  │                                            │  │
│  │  on Connect:                              │  │
│  │  ├── persist Settings                     │  │
│  │  ├── allocate qtfb buffer (fbmanagement)  │  │
│  │  └── exec vnsee with QTFB_KEY env         │  │
│  └─────────────┬──────────────────────────────┘  │
│                │ writes qtfb shm                 │
│       ┌────────┴───────────┐                     │
│       │   vnsee binary     │                     │
│       └────────────────────┘                     │
└────────────────────────────────────────────────┘
```

## What we vendored from rm-appload

```
extension/src/qtfb/
├── FBController.{cpp,h}    Qt Quick item that paints a qtfb buffer + handles
│                            input.
├── fbmanagement.{cpp,h}    qtfb session/socket management — accepts client
│                            connections, allocates shm, dispatches input
│                            events, etc.
├── common.h                shared protocol constants (message types,
│                            REFRESH_MODE_*, format constants).
└── log.h                   tiny logging macro stub.
```

Why vendor instead of depend? rm-appload bundles its own QML grid UI, app
manifest scanner, app library, virtual keyboard, and many other things we
don't need. We want a single button that launches one app. Pulling in the
full extension means tracking its release cadence and risking breakage on
unrelated changes (qmd hashes, manifest format, qtfb_key allocation
strategy). The qtfb code itself is small (~1k LOC) and well-isolated, so
copying once + maintaining is cheaper than depending across repos.

We don't ship `appload.so`. We assume `qt-resource-rebuilder.so` is
installed (which is also a dependency of AppLoad and several other
extensions, so it's almost certainly already there).

## qmldiff hashes

The `menu-icon.qmd` in this repo embeds hashes (djb2 with seed 5481) of
xochitl QML identifiers — typically the home-screen menu file's path and
the element id where we want to insert. xochitl's qt-resource-rebuilder
loads our `.qmd`, looks each hash up in its runtime hashtab, and patches
the matching QML resource in memory before xochitl's QML engine parses it.

When xochitl is updated, identifiers may rename or move. Detection: our
icon disappears or xochitl crashes on boot. Repair:

1. Rebuild the hashtab on the rMPP (xochitl ships
   `/home/root/xovi/rebuild_hashtable`).
2. `scripts/build-qmd.sh` re-emits `menu-icon.qmd` against
   `menu-icon.qmldiff` using a current hashtab.

If an identifier we depend on was *removed* (vs. renamed), update the
qmldiff source to point at whatever replaced it.

## Why is the qmldiff *source* in the repo?

A `.qmd` file is the hashed/binary form. Reading it tells you nothing
about *which* xochitl QML element it patches — every name is a number.
The `.qmldiff` source uses identifier strings, so when you read it you
see things like `AFFECT QmlNamespace.HomeMenu` and know what the
extension is doing. We commit the source; the `.qmd` is a build artifact.

## Independence from AppLoad

This extension does not link against `appload.so`. It does not call any
AppLoad symbols. It loads from the same `qt-resource-rebuilder`
infrastructure as AppLoad does (since that's how qmldiff works on the
rMPP), but otherwise is parallel. AppLoad can be installed, uninstalled,
or updated without affecting us.
