#!/bin/bash
# Deploy vnsee.so to /home/root/xovi/extensions.d/ on the rMPP and restart
# xochitl so xovi reloads + qmldiff reapplies.
set -e
RMPP="${RMPP:-root@192.168.1.186}"
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/rmpp_known_hosts"
[ -n "${SSHPASS:-}" ] || { echo "set SSHPASS"; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SO="$REPO_ROOT/extension/build/vncast.so"
QMD="$REPO_ROOT/extension/menu-icon.qmd"
[ -f "$SO" ] || { echo "missing $SO — run scripts/build-extension.sh first"; exit 1; }
[ -f "$QMD" ] || { echo "missing $QMD — run scripts/build-qmd.sh first"; exit 1; }

# Sanity-check: every hash in our qmd must resolve against the live hashtab.
# This is the exact invariant the broken appload.so 022a7b94 violated, which
# nuked xochitl into a panic loop. Refuse to deploy if it fails.
if ! bash "$REPO_ROOT/scripts/verify-qmd-resolvable.sh" "$QMD" >/tmp/verify-qmd.log 2>&1; then
    echo "REFUSING to deploy — qmd has unresolvable hashes:"
    cat /tmp/verify-qmd.log
    exit 1
fi

REMOTE_SO=/home/root/xovi/extensions.d/vncast.so
REMOTE_QMD=/home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd

echo "→ vncast.so          ($(stat -c%s "$SO") bytes)"
sshpass -e scp $SSH_OPTS "$SO" "$RMPP:$REMOTE_SO.new"
echo "→ vncast-menu-icon.qmd ($(stat -c%s "$QMD") bytes)"
sshpass -e scp $SSH_OPTS "$QMD" "$RMPP:$REMOTE_QMD.new"
sshpass -e ssh $SSH_OPTS "$RMPP" "mv $REMOTE_SO.new $REMOTE_SO && mv $REMOTE_QMD.new $REMOTE_QMD && ls -l $REMOTE_SO $REMOTE_QMD"

echo
echo "Restarting xochitl..."
sshpass -e ssh $SSH_OPTS "$RMPP" 'systemctl stop xochitl; sleep 2; /home/root/xovi/start & sleep 8; pidof xochitl && systemctl is-active xochitl'
echo
echo "Done. Look for the VNSee item in xochitl's home sidebar."
