#!/bin/bash
# Build the rmcast tablet-side install bundle.
#
# Assumes the dev-machine builds have already happened:
#   middleware/scripts/build-extension.sh   → extension/build/vncast.so
#   middleware/scripts/build-qmd.sh         → extension/menu-icon.qmd
#   middleware/scripts/build-vnsee-ferrari.sh → build-ferrari/dist/vnsee/vnsee
#                                              (or build-ferrari/vnsee — first match wins)
#
# Output: dist/rmcast-<git-describe>-<timestamp>.tar.gz
#         containing install.sh, uninstall.sh, and the three artifacts.
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"        # middleware/installer
MIDDLEWARE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"     # middleware
RMCAST_ROOT="$(cd "$MIDDLEWARE_DIR/.." && pwd)"    # vnsee fork root

SO_SRC="$MIDDLEWARE_DIR/extension/build/vncast.so"
QMD_SRC="$MIDDLEWARE_DIR/extension/menu-icon.qmd"
VNSEE_CANDIDATES="
$RMCAST_ROOT/build-ferrari/dist/vnsee/vnsee
$RMCAST_ROOT/build-ferrari/vnsee
"

VNSEE_SRC=""
for c in $VNSEE_CANDIDATES; do
    if [ -f "$c" ]; then VNSEE_SRC="$c"; break; fi
done

missing=()
[ -f "$SO_SRC"    ] || missing+=("$SO_SRC (run middleware/scripts/build-extension.sh)")
[ -f "$QMD_SRC"   ] || missing+=("$QMD_SRC (run middleware/scripts/build-qmd.sh)")
[ -n "$VNSEE_SRC" ] || missing+=("vnsee binary (run middleware/scripts/build-vnsee-ferrari.sh)")

if [ ${#missing[@]} -gt 0 ]; then
    echo "Missing build artifacts:" >&2
    for m in "${missing[@]}"; do echo "  - $m" >&2; done
    exit 1
fi

# Version tag: prefer git-describe, fall back to short SHA, then timestamp.
VER=$(git -C "$RMCAST_ROOT" describe --tags --dirty 2>/dev/null \
   || git -C "$RMCAST_ROOT" rev-parse --short HEAD 2>/dev/null \
   || date +%Y%m%d-%H%M%S)
TS=$(date +%Y%m%d-%H%M%S)
NAME="rmcast-$VER-$TS"

DIST_DIR="$RMCAST_ROOT/dist"
STAGE="$DIST_DIR/$NAME"
mkdir -p "$STAGE"

cp "$SO_SRC"                       "$STAGE/vncast.so"
cp "$QMD_SRC"                      "$STAGE/vncast-menu-icon.qmd"
cp "$VNSEE_SRC"                    "$STAGE/vnsee"
cp "$SCRIPT_DIR/install.sh"        "$STAGE/install.sh"
cp "$SCRIPT_DIR/uninstall.sh"      "$STAGE/uninstall.sh"
chmod 0755 "$STAGE/install.sh" "$STAGE/uninstall.sh" "$STAGE/vnsee"

# Drop a manifest so the user can verify what they're installing.
{
    echo "rmcast bundle"
    echo "version: $VER"
    echo "built:   $TS"
    echo "host:    $(hostname)"
    echo
    echo "files:"
    (cd "$STAGE" && for f in *; do
        printf "  %-28s  %10d bytes  sha256=%s\n" \
            "$f" "$(wc -c <"$f")" "$(sha256sum "$f" | cut -d' ' -f1)"
    done)
} > "$STAGE/MANIFEST.txt"

TARBALL="$DIST_DIR/$NAME.tar.gz"
tar -C "$DIST_DIR" -czf "$TARBALL" "$NAME"
rm -rf "$STAGE"

echo "Built: $TARBALL ($(wc -c <"$TARBALL") bytes)"
echo
echo "To install on the rMPP:"
echo "  scp $TARBALL root@<rmpp-ip>:/tmp/"
echo "  ssh root@<rmpp-ip> 'cd /tmp && tar xzf $NAME.tar.gz && cd $NAME && ./install.sh'"
