#!/bin/bash
set -e
QMLDIFF=/mnt/c/Users/user/Documents/Github/qmldiff/target/release/qmldiff

"$QMLDIFF" dump-hashtab /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/hashtab.bin > /tmp/local-tab.txt
SSHPASS=edMHkglxnO sshpass -e scp -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/rmpp_known_hosts \
    root@192.168.1.186:/home/root/xovi/exthome/qt-resource-rebuilder/hashtab /tmp/live-hashtab
"$QMLDIFF" dump-hashtab /tmp/live-hashtab > /tmp/live-tab.txt

echo "=== local hashtab entries: $(wc -l < /tmp/local-tab.txt) ==="
echo "=== live  hashtab entries: $(wc -l < /tmp/live-tab.txt) ==="
echo
echo "=== local has 16055804229128197180? ==="
grep -F '16055804229128197180' /tmp/local-tab.txt || echo "NO"
echo "=== live has 16055804229128197180? ==="
grep -F '16055804229128197180' /tmp/live-tab.txt || echo "NO"
echo
echo "=== diff between hashtabs (sample 20 lines) ==="
diff /tmp/local-tab.txt /tmp/live-tab.txt | head -20
