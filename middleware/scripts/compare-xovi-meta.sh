#!/bin/bash
# Compare xovi descriptor strings inside our build vs known-good extensions.
TOOL=/home/user/codex/ferrari/5.2.96-dirty/sysroots/x86_64-codexsdk-linux/usr/bin/aarch64-remarkable-linux/aarch64-remarkable-linux-strings

SSHPASS=edMHkglxnO sshpass -e scp -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/rmpp_known_hosts \
  root@192.168.1.186:/home/root/xovi/extensions.d/framebuffer-spy.so /tmp/fbspy.so 2>/dev/null

for so in /tmp/fbspy.so /mnt/c/Users/user/Documents/Github/rmpp-vnsee/extension/build/vncast.so; do
  echo "=== $so ==="
  ls -l "$so"
  # First few human-readable strings (xovi descriptor data is usually right at the start of .rodata)
  "$TOOL" -n 6 "$so" 2>&1 | head -40
  echo
done
