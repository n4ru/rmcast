#!/bin/bash
# Build vnsee for the reMarkable Paper Pro (ferrari) using a user-home SDK
# install instead of the /opt/codex layout that make-all.sh hardcodes.
# Mirrors what make-all.sh does in its `if [ -d /opt/codex/ferrari/... ]` block.
set -eo pipefail   # NOTE: no -u; the SDK's env scripts read unset vars

SDK_ROOT="${SDK_ROOT:-$HOME/codex/ferrari/5.2.96-dirty}"
ENV_SETUP="$SDK_ROOT/environment-setup-cortexa53-crypto-remarkable-linux"
TOOLCHAIN_FILE="$SDK_ROOT/sysroots/x86_64-codexsdk-linux/usr/share/cmake/cortexa53-crypto-remarkable-linux-toolchain.cmake"

[ -f "$ENV_SETUP" ]      || { echo "Missing $ENV_SETUP"; exit 1; }
[ -f "$TOOLCHAIN_FILE" ] || { echo "Missing $TOOLCHAIN_FILE"; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$REPO_ROOT/build-ferrari"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Sourcing SDK environment..."
# shellcheck disable=SC1090
. "$ENV_SETUP"

echo "Configuring CMake..."
cmake -S .. -B . \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build . -- -j"$(nproc)"

echo "Build artifacts:"
find dist -maxdepth 3 -type f -printf "%p\t%s bytes\n"

echo
echo "vnsee binary:"
file dist/vnsee/vnsee
