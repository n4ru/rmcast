#!/bin/bash
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HASHTAB=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
LIVE=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd
WORK=/tmp/appload-source.qmldiff

cp "$LIVE" "$WORK"
"$QMLDIFF" hash-diffs "$HASHTAB" "$WORK" -r
echo "===  unhashed appload diff (source-form qmldiff) ==="
cat "$WORK"
