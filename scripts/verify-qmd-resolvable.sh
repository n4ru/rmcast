#!/bin/bash
# Verify every [[hash]] in a .qmd file is resolvable against the live hashtab.
# Catches the same class of bug that crashed xochitl with the broken appload.so.
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HASHTAB=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
QMD=${1:-/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.qmd}

if [ ! -f "$QMD" ]; then
  echo "qmd not found: $QMD"; exit 1
fi

"$QMLDIFF" dump-hashtab "$HASHTAB" > /tmp/hashtab.txt
# Grab decimal hashes inside [[...]] (skip ones starting with " which are hashed strings already valid)
grep -oE '\[\[[0-9.]+\]\]' "$QMD" | tr -d '[]' | tr '.' '\n' | sort -u > /tmp/qmd-hashes.txt
TOTAL=$(wc -l < /tmp/qmd-hashes.txt)
MISSING=0
> /tmp/qmd-missing.txt
while read -r h; do
  [ -z "$h" ] && continue
  if ! grep -qF " = $h" /tmp/hashtab.txt; then
    echo "$h" >> /tmp/qmd-missing.txt
    MISSING=$((MISSING+1))
  fi
done < /tmp/qmd-hashes.txt

echo "qmd:        $QMD"
echo "hashes used: $TOTAL"
echo "missing:     $MISSING"
if [ "$MISSING" -gt 0 ]; then
  echo "--- unresolvable hashes ---"
  cat /tmp/qmd-missing.txt
  exit 2
fi
echo "OK: every hash resolves against the live hashtab."
