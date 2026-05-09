#!/bin/bash
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff

"$QMLDIFF" dump-hashtab /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin > /tmp/hashtab.txt
echo "Total local entries: $(wc -l < /tmp/hashtab.txt)"
echo
echo "=== 17477757197668945522 (first AFFECT in WORKING qmd) ==="
grep -F '17477757197668945522' /tmp/hashtab.txt || echo "NOT FOUND"
echo
echo "=== 16055804229128197180 (first AFFECT in BROKEN qmd) ==="
grep -F '16055804229128197180' /tmp/hashtab.txt || echo "NOT FOUND"
echo
echo "=== Other hashes from broken stanza line 1 of template ==="
for h in 7711468349764991 6502786168 477062473974076915 8545339034058226003 8399878573055752961 15264041274426464048; do
  found=$(grep -F "$h" /tmp/hashtab.txt | head -1)
  if [ -z "$found" ]; then
    echo "$h: NOT FOUND"
  else
    echo "$h: $found"
  fi
done
