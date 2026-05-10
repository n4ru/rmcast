#!/bin/bash
# Hash extension/menu-icon.qmldiff against the rMPP's hashtab to produce
# extension/menu-icon.qmd, which is what the xovi extension embeds.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
SRC="$REPO_ROOT/extension/menu-icon.qmldiff"
HASHTAB="$REPO_ROOT/extension/hashtab.bin"
DST="$REPO_ROOT/extension/menu-icon.qmd"

[ -x "$QMLDIFF" ] || { echo "Missing qmldiff binary at $QMLDIFF (build it: cd $(dirname $QMLDIFF)/../.. && cargo build --release --bin qmldiff)"; exit 1; }
[ -f "$SRC" ]     || { echo "Missing $SRC"; exit 1; }
[ -f "$HASHTAB" ] || { echo "Missing $HASHTAB (run scripts/extract-hashtab.sh)"; exit 1; }

cp "$SRC" "$DST"
"$QMLDIFF" hash-diffs "$HASHTAB" "$DST"
ls -lh "$DST"
echo "First 200 chars (sanity check):"
head -c 200 "$DST"
echo
