#!/bin/bash
NM=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-nm
CXXFILT=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-c++filt
OBJDUMP=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-objdump

for f in /tmp/libqsgepaper.so /tmp/libepaper.so /tmp/xochitl; do
  echo "=== $f ==="
  echo "  dyn defined : $($NM --dynamic --defined-only $f 2>/dev/null | wc -l)"
  echo "  dyn undefined: $($NM --dynamic --undefined-only $f 2>/dev/null | wc -l)"
  echo "  --- strings: epdc/waveform/sendUpdate ---"
  strings $f | grep -iE 'sendUpdate|EPFramebuffer|waveform' | head -20
  echo
done
