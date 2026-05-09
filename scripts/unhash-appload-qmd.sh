#!/bin/bash
# Unhash rm-appload's qmd against the rMPP's current hashtab so we can see
# what xochitl QML elements it targets (and use the same anchor for our
# own icon).
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HASHTAB=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
SRC=/mnt/c/Users/user/Documents/Github/vnsee/vendor/rm-appload/xovi/template/appload.qmd
DST=/tmp/appload-unhashed.qmd

cp "$SRC" "$DST"
"$QMLDIFF" hash-diffs "$HASHTAB" "$DST" -r
echo "=== unhashed appload.qmd ==="
cat "$DST"
