#!/bin/bash
# Extract the qmd blob from the rMPP's currently-installed appload.so so we
# have an un-hashable copy that matches the current hashtab.
set -e
TOOL=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-objdump
SO=/home/user/appload.so.live
[ -f "$SO" ] || { echo "missing $SO"; exit 1; }

echo "=== r\$apploadDiff symbol ==="
"$TOOL" -t "$SO" | grep apploadDiff

read OFF SIZE <<< "$("$TOOL" -t "$SO" | awk '/r\$apploadDiff/ {print strtonum("0x" $1), strtonum("0x" $5); exit}')"
echo "off=$OFF size=$SIZE"

dd if="$SO" of=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd \
   bs=1 skip="$OFF" count="$SIZE" 2>&1 | tail -1

# Strip trailing NUL byte (xovigen adds one).
truncate -s $((SIZE - 1)) /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd
ls -l /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd
echo "=== first 200 chars ==="
head -c 200 /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd
echo
