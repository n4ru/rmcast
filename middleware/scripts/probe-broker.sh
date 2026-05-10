#!/bin/bash
set -e
RMPP=root@192.168.1.186
SSH_OPTS="-o UserKnownHostsFile=/tmp/rmpp_known_hosts"

# Several candidate wire formats. The broker eats the first byte, so the
# remaining string after that byte is what it sees. We try common patterns.
CMDS=(
  "stakeScreenshot /home/root/screenshots,200"
  "ssimpleSignal:takeScreenshot:/home/root/screenshots,200"
  "stakeScreenshot:/home/root/screenshots,200"
  "stakeScreenshot,/home/root/screenshots,200"
  "qtakeScreenshot /home/root/screenshots,200"
  "etakeScreenshot /home/root/screenshots,200"
  "btakeScreenshot /home/root/screenshots,200"
  "atakeScreenshot /home/root/screenshots,200"
)

for cmd in "${CMDS[@]}"; do
  echo "=== sending: '$cmd' ==="
  sshpass -e ssh $SSH_OPTS "$RMPP" "printf '%s\n' \"$cmd\" > /run/xovi-mb"
  sleep 1
done

echo "=== broker log tail ==="
sshpass -e ssh $SSH_OPTS "$RMPP" 'journalctl --no-pager --since "30 seconds ago" | grep broker | tail -30'
