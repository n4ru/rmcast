#!/bin/bash
# Build patched rm-appload.so for the rMPP using the existing Codex SDK.
# Mirrors what rm-appload/xovi/make.sh does, with absolute paths so we don't
# rely on $XOVI_REPO being set globally.
set -eo pipefail   # SDK env scripts read unset vars; no -u

SDK_ROOT="${SDK_ROOT:-$HOME/codex/ferrari/5.2.96-dirty}"
ENV_SETUP="$SDK_ROOT/environment-setup-cortexa53-crypto-remarkable-linux"

[ -f "$ENV_SETUP" ] || { echo "Missing $ENV_SETUP"; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
APPLOAD_DIR="$REPO_ROOT/vendor/rm-appload"
XOVI_REPO="${XOVI_REPO:-$REPO_ROOT/../xovi}"

[ -d "$APPLOAD_DIR/xovi/template" ] || { echo "Missing $APPLOAD_DIR/xovi/template"; exit 1; }
[ -f "$XOVI_REPO/util/xovigen.py" ] || { echo "Missing $XOVI_REPO/util/xovigen.py"; exit 1; }

export XOVI_REPO

echo "[1/5] Sourcing SDK environment..."
# shellcheck disable=SC1090
. "$ENV_SETUP"
# Qt6 host tools (rcc, moc, uic) live in libexec, not bin — extend PATH.
QT_LIBEXEC="$SDK_ROOT/sysroots/x86_64-codexsdk-linux/usr/libexec"
[ -d "$QT_LIBEXEC" ] && export PATH="$QT_LIBEXEC:$PATH"

echo "[2/5] Preparing temporary build dir under $APPLOAD_DIR/xovi/temporary..."
cd "$APPLOAD_DIR/xovi"
rm -rf temporary
mkdir temporary
cp -r ../src temporary/
cp -r template/* temporary/

echo "[3/5] rcc on resources..."
rcc --no-compress -g cpp ../resources/resources.qrc \
    | sed '/#ifdef _MSC_VER/,/#endif/d' \
    | sed -n '/#ifdef/q;p' \
    > temporary/resources.cpp

cd temporary
echo "[4/5] xovigen..."
python3 "$XOVI_REPO/util/xovigen.py" -o xovi.cpp -H xovi.h appload.xovi

echo "[5/5] qmake6 + make..."
qmake6 .
make -j"$(nproc)"

echo
echo "Built: $(realpath appload.so)"
file appload.so
