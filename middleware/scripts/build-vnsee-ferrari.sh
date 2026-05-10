#!/bin/bash
# Cross-build vnsee for the reMarkable Paper Pro using the codex SDK at
# ~/codex/ferrari/5.2.96-dirty.
set -eo pipefail

# Layout (after middleware was merged into the vnsee fork):
#   <vnsee-fork>/                 ← VNSEE_DIR (CMake source root)
#   <vnsee-fork>/middleware/      ← MIDDLEWARE_DIR (this script lives in middleware/scripts)
#   <vnsee-fork>/patches/         ← libvncserver patches we apply at build time
#   <vnsee-fork>/vendor/libvncserver/  ← upstream submodule
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MIDDLEWARE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VNSEE_DIR="$(cd "$MIDDLEWARE_DIR/.." && pwd)"

SDK_ROOT="${SDK_ROOT:-$HOME/codex/ferrari/5.2.96-dirty}"
ENV_SETUP="$SDK_ROOT/environment-setup-cortexa53-crypto-remarkable-linux"
TOOLCHAIN="$SDK_ROOT/sysroots/x86_64-codexsdk-linux/usr/share/cmake/cortexa53-crypto-remarkable-linux-toolchain.cmake"

[ -f "$ENV_SETUP" ] || { echo "Missing $ENV_SETUP"; exit 1; }
[ -f "$TOOLCHAIN" ] || { echo "Missing $TOOLCHAIN"; exit 1; }

BUILD="$VNSEE_DIR/build-ferrari"
PATCH_DIR="$VNSEE_DIR/patches"

echo "[1/4] Sourcing SDK environment..."
. "$ENV_SETUP"

echo "[2/4] Applying patches against vendored libvncserver..."
# We don't fork the entire LibVNC/libvncserver tree — instead we vendor
# upstream as a submodule and apply our local diffs (mono1 decoder etc.)
# during build. Idempotent: stamp file in build/ records what's applied.
LVS_DIR="$VNSEE_DIR/vendor/libvncserver"
STAMP="$BUILD/.patches-applied"
mkdir -p "$BUILD"
if [ -d "$PATCH_DIR" ]; then
    NEED_APPLY=0
    for p in "$PATCH_DIR"/*.patch; do
        [ -f "$p" ] || continue
        if [ ! -f "$STAMP" ] || ! grep -qF "$(basename "$p")" "$STAMP" 2>/dev/null; then
            NEED_APPLY=1
            break
        fi
    done
    if [ "$NEED_APPLY" = "1" ]; then
        # Reset libvncserver to upstream HEAD so re-applying is clean.
        git -C "$LVS_DIR" checkout -- src/libvncclient/rfbclient.c 2>/dev/null || true
        : > "$STAMP"
        for p in "$PATCH_DIR"/*.patch; do
            [ -f "$p" ] || continue
            echo "    apply $(basename "$p")"
            git -C "$LVS_DIR" apply --whitespace=nowarn "$p"
            basename "$p" >> "$STAMP"
        done
    else
        echo "    (already applied)"
    fi
fi

echo "[3/4] Configuring CMake..."
cd "$BUILD"
cmake -S "$VNSEE_DIR" -B . \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -Wno-dev

echo "[4/4] Building..."
cmake --build . -j"$(nproc)"

ls -la "$BUILD/dist/vnsee/vnsee" 2>/dev/null || ls -la "$BUILD/vnsee" 2>/dev/null || true
echo "Done."
