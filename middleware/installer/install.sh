#!/bin/sh
# rmcast tablet-side installer. Runs ON the rMPP.
#
# Expects to be unpacked alongside the artifacts it installs:
#   ./install.sh
#   ./vncast.so
#   ./vncast-menu-icon.qmd
#   ./vnsee
#
# Drops files into the standard xovi locations and bounces xochitl. Existing
# files are backed up to a timestamped sibling before being overwritten, so
# `uninstall.sh` (or a manual `mv`) can restore them.
set -eu

# --- environment sanity --------------------------------------------------
if [ "$(uname -m)" != "aarch64" ]; then
    echo "ERROR: install.sh must run on the rMPP (aarch64). Detected: $(uname -m)." >&2
    echo "       From your dev machine, scp the bundle to the device first."  >&2
    exit 1
fi

if [ ! -d /home/root/xovi ]; then
    echo "ERROR: /home/root/xovi not found. Install xovi first:" >&2
    echo "       https://xovi.bearblog.dev"                       >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
for f in vncast.so vncast-menu-icon.qmd vnsee; do
    if [ ! -f "$HERE/$f" ]; then
        echo "ERROR: missing artifact $HERE/$f. The bundle is incomplete." >&2
        exit 1
    fi
done

# --- destinations --------------------------------------------------------
DEST_SO=/home/root/xovi/extensions.d/vncast.so
DEST_QMD=/home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd
DEST_VNSEE_DIR=/home/root/xovi/exthome/appload/vnsee
DEST_VNSEE=$DEST_VNSEE_DIR/vnsee

mkdir -p "$(dirname "$DEST_SO")" \
         "$(dirname "$DEST_QMD")" \
         "$DEST_VNSEE_DIR"

# --- backup + install one file -------------------------------------------
TS=$(date +%Y%m%d-%H%M%S)
install_one() {
    src=$1; dst=$2
    if [ -f "$dst" ]; then
        bak="$dst.$TS.bak"
        cp -a "$dst" "$bak"
        echo "  backed up: $bak"
    fi
    cp -a "$src" "$dst.new"
    chmod 0755 "$dst.new"
    mv "$dst.new" "$dst"
    echo "  installed: $dst ($(wc -c <"$dst") bytes)"
}

echo "[1/4] vncast.so"
install_one "$HERE/vncast.so"             "$DEST_SO"

echo "[2/4] vncast-menu-icon.qmd"
install_one "$HERE/vncast-menu-icon.qmd"  "$DEST_QMD"

echo "[3/4] vnsee"
install_one "$HERE/vnsee"                 "$DEST_VNSEE"

# --- restart xochitl via xovi --------------------------------------------
echo "[4/4] Restarting xochitl..."
systemctl stop xochitl 2>/dev/null || true
sleep 1
# Run xovi's entry-point in the background; it re-execs xochitl with our
# .so injected. Detach so the install script can return cleanly.
nohup /home/root/xovi/start >/tmp/rmcast-xovi-start.log 2>&1 &
sleep 4
if pidof xochitl >/dev/null; then
    echo
    echo "Done. Look for the Cast item in xochitl's home sidebar."
    echo "Logs: /tmp/rmcast-xovi-start.log and journalctl -u xochitl"
else
    echo
    echo "WARNING: xochitl didn't come back up within 4s." >&2
    echo "         Check /tmp/rmcast-xovi-start.log for clues." >&2
    exit 2
fi
