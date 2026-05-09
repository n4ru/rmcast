#!/bin/bash
# Cross-build vnsee for the reMarkable Paper Pro using the codex SDK at
# ~/codex/ferrari/5.2.96-dirty.
set -eo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_ROOT="${SDK_ROOT:-$HOME/codex/ferrari/5.2.96-dirty}"
ENV_SETUP="$SDK_ROOT/environment-setup-cortexa53-crypto-remarkable-linux"
TOOLCHAIN="$SDK_ROOT/sysroots/x86_64-codexsdk-linux/usr/share/cmake/cortexa53-crypto-remarkable-linux-toolchain.cmake"

[ -f "$ENV_SETUP" ] || { echo "Missing $ENV_SETUP"; exit 1; }
[ -f "$TOOLCHAIN" ] || { echo "Missing $TOOLCHAIN"; exit 1; }

VNSEE_DIR="$REPO_ROOT/vnsee"
BUILD="$VNSEE_DIR/build-ferrari"

echo "[1/3] Sourcing SDK environment..."
. "$ENV_SETUP"

echo "[2/3] Configuring CMake..."
mkdir -p "$BUILD"
cd "$BUILD"
cmake -S "$VNSEE_DIR" -B . \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -Wno-dev

echo "[3/3] Building..."
cmake --build . -j"$(nproc)"

ls -la "$BUILD/dist/vnsee/vnsee" 2>/dev/null || ls -la "$BUILD/vnsee" 2>/dev/null || true
echo "Done."
