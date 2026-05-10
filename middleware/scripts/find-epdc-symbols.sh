#!/bin/bash
set -e
NM=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-nm
CXXFILT=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-c++filt

for so in /tmp/libqsgepaper.so /tmp/libepaper.so; do
  echo "=== $so ==="
  "$NM" --dynamic --defined-only "$so" 2>/dev/null \
    | grep -iE 'sendUpdate|EPFramebuffer|Waveform|Update[A-Z]' \
    | head -50 \
    | "$CXXFILT" 2>/dev/null
  echo
done

echo "=== full demangled exports of libqsgepaper.so ==="
"$NM" --dynamic --defined-only /tmp/libqsgepaper.so 2>/dev/null | awk '{print $3}' | "$CXXFILT" 2>/dev/null | grep -iE 'epfram|sendUp|waveform|update' | sort -u | head -60
