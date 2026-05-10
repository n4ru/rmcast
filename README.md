# VNSee-QTFB

[![reMarkable 1 is supported](https://img.shields.io/badge/rM1-supported-green)](https://remarkable.com/products/remarkable-1)
[![reMarkable 2 is supported](https://img.shields.io/badge/rM2-supported-green)](https://remarkable.com/products/remarkable-2)
[![reMarkable Paper Pro is supported](https://img.shields.io/badge/rMPP-supported-green)](https://remarkable.com/products/remarkable-paper/pro)
[![reMarkable Paper Pro Move is supported](https://img.shields.io/badge/rMPPM-supported-green)](https://remarkable.com/products/remarkable-paper/pro-move)

VNSee-QTFB is a [VNC](https://en.wikipedia.org/wiki/Virtual_Network_Computing) client for the [reMarkable tablet](https://remarkable.com) allowing you to use the device as a second screen or view a desktop environment running on your device. This is a fork of [VNSee](https://github.com/matteodelabre/vnsee) that has been ported to QTFB to enhance performance and compatibility, and has many new features and fixes.

## rmcast (this repo)

This repo is **rmcast**, the rMPP-side tablet client of the rm-cast display-mirroring stack. It bundles two things:

- The **VNC client** at the repo root (forked from VNSee-QTFB) — receives RFB frames over the wire, decodes them, writes them into a shm region. The compiled binary is still named `vnsee` for now.
- `middleware/` — the **vncast.so** xovi extension that loads into xochitl, hosts the qtfb shm server, paints the decoded frames into a QML overlay, and hooks the EPDC waveform path. See [middleware/README.md](middleware/README.md).

The **server** lives in a separate repo: [`n4ru/rewire`](https://github.com/n4ru/rewire) — a Windows RFB server using DXGI Desktop Duplication and a custom 1-bit luma encoding (`rcastmono1`).

Local patches against the vendored libvncserver submodule live in `patches/` and are applied at build time by `middleware/scripts/build-vnsee-ferrari.sh`.

## Disclaimer

This project is not affiliated with, nor endorsed by, [reMarkable AS](https://remarkable.com/).\
**I hold no responsibility for any damage done to your device as a result of using this software.**

## Install

1) Install xovi and AppLoad on your reMarkable. [Here is a guide.](https://xovi.bearblog.dev)
2) Copy the release for your device into `/home/root/xovi/exthome/appload`.
3) Run `cd xovi/exthome/appload` from the device.
4) Run `unzip vnsee-DEVICE.zip` from the device.
5) Delete the zip archive.
6) Refresh AppLoad.

The VNC server resolution must match your device resolution.

## Technologies

This client was built in C++ using [libvncserver](https://github.com/LibVNC/libvncserver) and [rm-appload](https://github.com/asivery/rm-appload).

## Acknowledgments

Huge thanks to [Mattéo Delabre](https://github.com/matteodelabre), for writing the original VNSee which made this all possible! Go support them [here](https://github.com/sponsors/matteodelabre).

Additional thanks to:

- [asivery](https://github.com/asivery) for creating rm-appload and helping me port to QTFB.
- [Noa Himesaka](https://github.com/NoaHimesaka1873) for helping me understand how QTFB works and testing on the reMarkable Paper Pro.
- GreySim for testing on the reMarkable 1.
- [notfrants](https://github.com/notfrants) for testing on the reMarkable 2.
- [ingatellent](https://github.com/ingatellent) for inspiring me to port VNSee to QTFB.
- The amazing community of the [reMarkable Discord Server](https://discord.gg/u3P9sDW) for helping me with my dumb mistakes and having faith in me.

\- @khyryra

## Original Acknowledgements

Many thanks to:

- [libremarkable](https://github.com/canselcik/libremarkable) and [FBInk](https://github.com/NiLuJe/FBInk), on which input/output handling in this client is based.
- [Damien Challet](https://github.com/damienchallet) and [Qwertystop](https://news.ycombinator.com/item?id=13115739) for providing the inspiration of a VNC client for the reMarkable.
- The [Discord developer community](https://discord.gg/JSSGnFY) for providing initial feedback and testing.
- [Florian Magin](https://github.com/fmagin) for contributing a driver-generic way of setting up Linux/X11 systems.
- [@asmanur](https://github.com/asmanur) for improving the repaint latency.
- [@mhhf](https://github.com/mhhf) for helping with TigerVNC compatibility.
- [@rowancallahan](https://github.com/rowancallahan) and [@Axenntio](https://github.com/Axenntio) for contributing a guide for setting up second-screen on Sway.
- [@torwag](https://github.com/torwag) for writing the initial version of the `vnsee-gui` script.

[Martin Sandsmark](https://github.com/sandsmark) also built [a VNC client for the reMarkable](https://github.com/sandsmark/revncable) which only depends on Qt.

\- @matteodelabre

## License

This work is licensed under the GPL v3.
