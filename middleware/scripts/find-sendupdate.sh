#!/bin/bash
set -e
NM=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-nm
[ -x "$NM" ] || { echo "Missing $NM"; exit 1; }
"$NM" --dynamic --defined-only /tmp/xochitl > /tmp/xochitl-syms.txt 2>/dev/null || true
echo "=== EPFramebuffer-related dynamic symbols ==="
grep -iE 'EPFramebuffer|sendUpdate|epdc|waveform' /tmp/xochitl-syms.txt | head -40
echo
echo "=== sendUpdate (any T or t) ==="
grep -E ' [Tt] .*sendUpdate' /tmp/xochitl-syms.txt | head -30
echo
echo "=== c++filt of all EPFramebuffer methods ==="
CXXFILT=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-c++filt
grep 'EPFramebuffer' /tmp/xochitl-syms.txt | awk '{print $3}' | "$CXXFILT" 2>/dev/null | sort -u | head -40
