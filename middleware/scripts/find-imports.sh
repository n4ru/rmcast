#!/bin/bash
SDK=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux
NM=$SDK/aarch64-remarkable-linux-nm
CXXFILT=$SDK/aarch64-remarkable-linux-c++filt
LIB=${1:-$HOME/rmpp-symbols/libepaper.so}
PATTERN=${2:-EPFramebuffer}
echo "=== Undefined symbols in $LIB matching '$PATTERN' ==="
$NM --dynamic --undefined-only "$LIB" \
  | awk '{print $NF}' \
  | $CXXFILT \
  | grep -E "$PATTERN" \
  | sort -u
