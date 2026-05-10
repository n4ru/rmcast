#!/bin/bash
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HASHTAB=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
LIVE=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd

# Round-trip: unhash live qmd → re-hash → compare bytes
cp "$LIVE" /tmp/rt.qmd
"$QMLDIFF" hash-diffs "$HASHTAB" /tmp/rt.qmd -r   # unhash
"$QMLDIFF" hash-diffs "$HASHTAB" /tmp/rt.qmd      # re-hash

echo "=== diff sizes ==="
ls -l "$LIVE" /tmp/rt.qmd
echo
echo "=== first divergence ==="
diff <(xxd "$LIVE") <(xxd /tmp/rt.qmd) | head -10
