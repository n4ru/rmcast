#!/bin/bash
# Trigger an rm-shot screenshot via xovi-message-broker on the rMPP, wait
# for it, then scp the newest file in /home/root/screenshots back here.
#
# Usage: trigger-screenshot.sh [delay_ms] [out_dir_local]
set -e
DELAY_MS=${1:-200}
OUT_DIR=${2:-/mnt/c/Users/user/Documents/Github/rmpp-vnsee/screenshots}
RMPP=${RMPP:-root@192.168.1.186}
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/rmpp_known_hosts"
[ -n "${SSHPASS:-}" ] || { echo "set SSHPASS"; exit 1; }

mkdir -p "$OUT_DIR"

# Wire format (decoded from xovi-message-broker.so disasm):
#   <TYPE><NAME>:<PAYLOAD>\n
#   TYPE = 'e' → broadcast to extensions, 'u' → broadcast to qml,
#          '>' prefix = wait for response.
# rm-shot listens on signal "takeScreenshot" with payload "<dir>,<delay_ms>".
sshpass -e ssh $SSH_OPTS "$RMPP" "echo 'etakeScreenshot:/home/root/screenshots,$DELAY_MS' > /run/xovi-mb"

# rm-shot writes one file per shot. Wait long enough for the delay + IO.
sleep_s=$(awk "BEGIN { printf \"%.2f\", ($DELAY_MS+800)/1000 }")
sleep "$sleep_s"

NEWEST=$(sshpass -e ssh $SSH_OPTS "$RMPP" 'ls -t /home/root/screenshots/*.png 2>/dev/null | head -n 1')
[ -n "$NEWEST" ] || { echo "no screenshot produced"; exit 2; }

echo "→ pulling $NEWEST"
sshpass -e scp $SSH_OPTS "$RMPP:$NEWEST" "$OUT_DIR/" >/dev/null
echo "saved $OUT_DIR/$(basename "$NEWEST")"
