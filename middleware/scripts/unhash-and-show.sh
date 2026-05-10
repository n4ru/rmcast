#!/bin/bash
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HT=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
SRC=${1:-/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd}
"$QMLDIFF" unhash "$SRC" --hashtab "$HT"
