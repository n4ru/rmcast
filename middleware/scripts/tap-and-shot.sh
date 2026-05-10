#!/bin/bash
# Tap at (FB_X, FB_Y) on the rMPP, wait, screenshot, pull. Coordinates are
# in framebuffer pixels (1620 portrait × 2160) — the script scales them
# into the Elan touchscreen's native (2064 × 2832) range. Settle delay
# defaults to 500 ms.
#
# Usage: tap-and-shot.sh <fb_x> <fb_y> [settle_ms] [out_label]
set -e
FB_X=$1; FB_Y=$2; SETTLE=${3:-500}; LABEL=${4:-tap}
RMPP=${RMPP:-root@192.168.1.186}
SSH_OPTS="-o UserKnownHostsFile=/tmp/rmpp_known_hosts"
[ -n "${SSHPASS:-}" ] || { echo "set SSHPASS"; exit 1; }
[ -n "$FB_X" ] && [ -n "$FB_Y" ] || { echo "usage: $0 fb_x fb_y [settle_ms]"; exit 1; }

# Scale fb pixels → touch units. Touch ABS reported by Elan: X 0..2064, Y 0..2832.
TOUCH_X=$(awk "BEGIN { printf \"%d\", $FB_X * 2064 / 1620 }")
TOUCH_Y=$(awk "BEGIN { printf \"%d\", $FB_Y * 2832 / 2160 }")

OUT_DIR=/mnt/c/Users/user/Documents/Github/rmpp-vnsee/screenshots
mkdir -p "$OUT_DIR"

echo "tap fb=($FB_X,$FB_Y) → touch=($TOUCH_X,$TOUCH_Y), settle=${SETTLE}ms, label=$LABEL"
sshpass -e ssh $SSH_OPTS "$RMPP" "/home/root/tap.py $TOUCH_X $TOUCH_Y"
awk "BEGIN { system(\"sleep \" $SETTLE/1000) }"
sshpass -e ssh $SSH_OPTS "$RMPP" "echo 'etakeScreenshot:/home/root/screenshots,100' > /run/xovi-mb"
sleep 1
NEWEST=$(sshpass -e ssh $SSH_OPTS "$RMPP" 'ls -t /home/root/screenshots/*.png 2>/dev/null | head -n 1')
[ -n "$NEWEST" ] || { echo "no screenshot produced"; exit 2; }
LOCAL="$OUT_DIR/$(basename "$NEWEST" .png)-$LABEL.png"
sshpass -e scp $SSH_OPTS "$RMPP:$NEWEST" "$LOCAL" >/dev/null
echo "saved $LOCAL"
