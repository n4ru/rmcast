#!/bin/bash
SDK=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux
NM=$SDK/aarch64-remarkable-linux-nm
LIB=${1:-$HOME/rmpp-symbols/libepaper.so}
PAT=${2:-flush}
$NM --dynamic --defined-only "$LIB" | grep -E "$PAT"
