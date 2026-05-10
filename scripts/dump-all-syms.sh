#!/bin/bash
SDK=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux
NM=$SDK/aarch64-remarkable-linux-nm
CXXFILT=$SDK/aarch64-remarkable-linux-c++filt

LIB=${1:-/tmp/epaper.so}
$NM --dynamic --defined-only "$LIB" \
  | awk '{print $3}' \
  | $CXXFILT \
  | sort -u
