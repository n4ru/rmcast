# rmcast tablet-side installer

Three scripts for shipping rmcast onto an rMPP:

| Script         | Where it runs   | What it does                                                         |
|----------------|-----------------|----------------------------------------------------------------------|
| `bundle.sh`    | dev machine     | Packages the built artifacts + install scripts into `dist/rmcast-<ver>-<ts>.tar.gz` |
| `install.sh`   | rMPP            | Copies artifacts to standard xovi locations, backs up existing files, restarts xochitl |
| `uninstall.sh` | rMPP            | Restores most recent backup (or removes the file), restarts xochitl  |

## End-user install (recipient side)

```sh
# from your dev machine
scp dist/rmcast-<ver>-<ts>.tar.gz root@<rmpp-ip>:/tmp/

# on the rMPP
cd /tmp
tar xzf rmcast-<ver>-<ts>.tar.gz
cd rmcast-<ver>-<ts>
./install.sh
```

The Cast item should appear in xochitl's home sidebar after xochitl restarts.

## Build a bundle (developer side)

Pre-reqs (run from `middleware/scripts/` first):
```sh
./build-extension.sh        # produces extension/build/vncast.so
./build-qmd.sh              # produces extension/menu-icon.qmd
./build-vnsee-ferrari.sh    # produces ../build-ferrari/dist/vnsee/vnsee
```

Then:
```sh
./bundle.sh                 # writes dist/rmcast-<ver>-<ts>.tar.gz
```

## Install layout on the device

| Source artifact            | Installed to                                                      |
|----------------------------|-------------------------------------------------------------------|
| `vncast.so`                | `/home/root/xovi/extensions.d/vncast.so`                          |
| `vncast-menu-icon.qmd`     | `/home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd` |
| `vnsee`                    | `/home/root/xovi/exthome/appload/vnsee/vnsee`                     |

The vnsee binary path lands inside `appload/` because that's where the
launcher (`extension/src/launcher.cpp`'s `kVnseeBin`) looks for it. The
extension itself doesn't depend on the AppLoad xovi extension — only the
file path is shared. A future change can move vnsee under `exthome/rmcast/`
and update `kVnseeBin` together.

## What backups look like

`install.sh` writes timestamped copies of any file it overwrites:

```
/home/root/xovi/extensions.d/vncast.so.20260509-235013.bak
/home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd.20260509-235013.bak
/home/root/xovi/exthome/appload/vnsee/vnsee.20260509-235013.bak
```

`uninstall.sh` looks for the newest `.YYYYMMDD-HHMMSS.bak` sibling and
restores it. If none exists, it just removes the installed file.
