#!/bin/bash
set -e
OBJDUMP=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-objdump
[ -x "$OBJDUMP" ] || { echo "missing objdump"; exit 1; }

echo "=== relevant symbols ==="
"$OBJDUMP" -t /tmp/mb.so | grep -iE "parse|pipe|thread|simpleSignal" | head -30
echo
echo "=== disasm around 'Cannot parse' string ==="
"$OBJDUMP" -d /tmp/mb.so | grep -B 2 -A 30 "Cannot parse" | head -80
