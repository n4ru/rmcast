#!/bin/bash
set -e
SDK=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux
NM=$SDK/aarch64-remarkable-linux-nm
CXXFILT=$SDK/aarch64-remarkable-linux-c++filt

LIB=${1:-/tmp/qsgepaper.so}
[ -f "$LIB" ] || { echo "missing $LIB"; exit 1; }

echo "=== EPFramebuffer methods in $LIB ==="
$NM --dynamic --defined-only "$LIB" \
  | awk '{print $3}' \
  | $CXXFILT \
  | grep -E 'EPFramebuffer|EPDC|sendUpdate|swapBuffers|sendUpdate|waveform' \
  | sort -u
echo
echo "=== All defined symbols mentioning Update ==="
$NM --dynamic --defined-only "$LIB" \
  | awk '{print $3}' \
  | $CXXFILT \
  | grep -iE 'update|paint|swap' \
  | sort -u | head -60
