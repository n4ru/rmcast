#!/bin/bash
# Cross-build vnsee.so (the xovi extension) using the codex SDK already
# installed at ~/codex/ferrari/5.2.96-dirty. Mirrors rm-appload's
# xovi/make.sh but lean — no upstream src/ tree to copy in.
set -eo pipefail

SDK_ROOT="${SDK_ROOT:-$HOME/codex/ferrari/5.2.96-dirty}"
ENV_SETUP="$SDK_ROOT/environment-setup-cortexa53-crypto-remarkable-linux"
[ -f "$ENV_SETUP" ] || { echo "Missing $ENV_SETUP"; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXT_DIR="$REPO_ROOT/extension"
XOVI_REPO="${XOVI_REPO:-$REPO_ROOT/../xovi}"

[ -f "$EXT_DIR/menu-icon.qmd" ] || { echo "Missing menu-icon.qmd — run scripts/build-qmd.sh first"; exit 1; }
[ -f "$XOVI_REPO/util/xovigen.py" ] || { echo "Missing xovigen at $XOVI_REPO/util/xovigen.py"; exit 1; }

echo "[1/4] Sourcing SDK environment..."
. "$ENV_SETUP"
QT_LIBEXEC="$SDK_ROOT/sysroots/x86_64-codexsdk-linux/usr/libexec"
[ -d "$QT_LIBEXEC" ] && export PATH="$QT_LIBEXEC:$PATH"

echo "[2/4] Staging build dir..."
BUILD="$EXT_DIR/build"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cp -r "$EXT_DIR/src" "$BUILD/src"
cp "$EXT_DIR/vncast.pro" "$BUILD/"
cp "$EXT_DIR/vncast.xovi" "$BUILD/"
cp "$EXT_DIR/menu-icon.qmd" "$BUILD/"

echo "[3/4] rcc on application.qrc..."
rcc --no-compress -g cpp "$EXT_DIR/application.qrc" \
    | sed '/#ifdef _MSC_VER/,/#endif/d' \
    | sed -n '/#ifdef/q;p' \
    > "$BUILD/resources.cpp"

echo "[4/4] xovigen + qmake6 + make..."
cd "$BUILD"
export XOVI_REPO
python3 "$XOVI_REPO/util/xovigen.py" -o xovi.cpp -H xovi.h vncast.xovi
qmake6 .
make -j"$(nproc)"

echo
echo "Built: $(realpath vncast.so)"
file vncast.so
