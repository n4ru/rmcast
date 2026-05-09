#!/bin/bash
# Deploy patched vnsee to the rMPP at 192.168.1.186, with timestamped backup.
set -e

RMPP_HOST="${RMPP_HOST:-192.168.1.186}"
RMPP_USER="${RMPP_USER:-root}"
LOCAL_BIN="${LOCAL_BIN:-/mnt/c/Users/user/Documents/Github/vnsee/build-ferrari/dist/vnsee/vnsee}"
REMOTE_DIR="${REMOTE_DIR:-/home/root/xovi/exthome/appload/vnsee}"
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/rmpp_known_hosts"

if [ -z "${SSHPASS:-}" ]; then
  echo "Set SSHPASS env var with the rMPP root password."
  exit 1
fi

if [ ! -f "$LOCAL_BIN" ]; then
  echo "Local binary not found: $LOCAL_BIN"
  exit 1
fi

TARGET="$RMPP_USER@$RMPP_HOST"

echo "[1/3] Backup existing binary on rMPP..."
sshpass -e ssh $SSH_OPTS "$TARGET" 'set -e
  cd "'"$REMOTE_DIR"'"
  TS=$(date +%Y%m%d-%H%M%S)
  cp -a vnsee "vnsee.$TS.bak"
  ln -sf "vnsee.$TS.bak" vnsee.bak
  echo "backup: $(ls -l vnsee.$TS.bak)"
  echo "symlink: $(ls -l vnsee.bak)"
  echo -n "orig sha256: "; sha256sum vnsee | cut -c1-12
'

echo "[2/3] Copy new binary..."
sshpass -e scp $SSH_OPTS "$LOCAL_BIN" "$TARGET:$REMOTE_DIR/vnsee.new"

echo "[3/3] Activate new binary..."
sshpass -e ssh $SSH_OPTS "$TARGET" 'set -e
  cd "'"$REMOTE_DIR"'"
  chmod +x vnsee.new
  echo -n "new sha256: "; sha256sum vnsee.new | cut -c1-12
  mv vnsee.new vnsee
  echo "deployed: $(ls -l vnsee)"
  echo "siblings:"
  ls -l vnsee* | sed s/^/  /
'

echo
echo "Done. To revert:  ssh $TARGET cp $REMOTE_DIR/vnsee.bak $REMOTE_DIR/vnsee"
