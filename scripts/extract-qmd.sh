#!/bin/bash
set -e
SDK=/home/user/codex/ferrari/5.2.96-dirty
TOOL_DIR="$SDK/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux"
OBJDUMP="$TOOL_DIR/aarch64-remarkable-linux-objdump"
READELF="$TOOL_DIR/aarch64-remarkable-linux-readelf"
BAK=/home/user/appload.so.bak

[ -x "$OBJDUMP" ] || { echo "missing objdump at $OBJDUMP"; exit 1; }
[ -f "$BAK" ]    || { echo "missing $BAK"; exit 1; }

echo "=== r\$apploadDiff in .bak (objdump) ==="
"$OBJDUMP" -t "$BAK" | grep -i apploadDiff

echo "=== r\$apploadDiff in .bak (readelf) ==="
"$READELF" -s "$BAK" | grep -i apploadDiff

echo "=== sections containing strings around the qmd ==="
"$OBJDUMP" -h "$BAK" | grep -E '\.rodata|\.data|\.text' | head -10
