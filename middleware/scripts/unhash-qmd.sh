#!/bin/bash
# Reverse-lookup every [[N]] in a qmd against the live hashtab, producing a
# human-readable text qmd. Useful for figuring out where AppLoad and friends
# put their hooks.
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff
HT=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin
SRC=${1:-/mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/menu-icon.live.qmd}

"$QMLDIFF" dump-hashtab "$HT" > /tmp/hashtab.txt
# Build a sed program: hashtab is "value = hash". We want to flip and rewrite [[hash]] -> value.
awk -F' = ' '{
  gsub(/[\\\/&]/,"\\\\&",$1);
  print "s/\\[\\[" $2 "\\]\\]/" $1 "/g"
}' /tmp/hashtab.txt > /tmp/unhash.sed

sed -f /tmp/unhash.sed "$SRC"
