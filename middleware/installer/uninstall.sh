#!/bin/sh
# rmcast tablet-side uninstaller. Runs ON the rMPP.
#
# Removes the three installed artifacts. If a `.bak` sibling exists from the
# most recent install (created by install.sh), restores it; otherwise just
# deletes the file. xochitl is bounced afterwards.
set -eu

if [ "$(uname -m)" != "aarch64" ]; then
    echo "ERROR: uninstall.sh must run on the rMPP (aarch64)." >&2
    exit 1
fi

DEST_SO=/home/root/xovi/extensions.d/vncast.so
DEST_QMD=/home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd
DEST_VNSEE=/home/root/xovi/exthome/appload/vnsee/vnsee

# Restore the most-recent timestamped backup of $1 if any exists, else rm $1.
remove_or_restore() {
    target=$1
    [ -f "$target" ] || { echo "  (not installed) $target"; return; }
    # Find newest backup matching $target.YYYYMMDD-HHMMSS.bak
    newest=$(ls -1t "$target".*.bak 2>/dev/null | head -1 || true)
    if [ -n "$newest" ]; then
        cp -a "$newest" "$target"
        echo "  restored: $target  ←  $(basename "$newest")"
    else
        rm -f "$target"
        echo "  removed:  $target"
    fi
}

echo "[1/4] vncast.so"
remove_or_restore "$DEST_SO"
echo "[2/4] vncast-menu-icon.qmd"
remove_or_restore "$DEST_QMD"
echo "[3/4] vnsee"
remove_or_restore "$DEST_VNSEE"

echo "[4/4] Restarting xochitl..."
systemctl stop xochitl 2>/dev/null || true
sleep 1
nohup /home/root/xovi/start >/tmp/rmcast-xovi-start.log 2>&1 &
sleep 4
if pidof xochitl >/dev/null; then
    echo "Done."
else
    echo "WARNING: xochitl didn't come back up within 4s." >&2
    exit 2
fi
