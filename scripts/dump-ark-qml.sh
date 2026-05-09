#!/bin/bash
# Extract Ark QML files (SegmentedControl, InputField, Button, etc.) from
# xochitl's QRC by finding the qresource section and decompressing it.
# Quicker hack: just have qmlscene-style strings + offset extract via
# `strings` + grep to learn the property API.
set -e
RMPP=root@192.168.1.186
SSH_OPTS="-o UserKnownHostsFile=/tmp/rmpp_known_hosts"
[ -n "${SSHPASS:-}" ] || { echo "set SSHPASS"; exit 1; }

# Most reliable: pull the QML files directly via xochitl's QRC reader. They
# live as resources, but xochitl was built statically with rcc — we can't
# scp them out. Best alternative: rmldumper or strings+pattern.
#
# strings approach — grep for property declarations near component name.
sshpass -e ssh $SSH_OPTS "$RMPP" 'strings /usr/bin/xochitl | grep -B 0 -A 0 -E "(property|signal|function)" | head -200' \
    > /tmp/xochitl-qml-strings.txt
wc -l /tmp/xochitl-qml-strings.txt
echo
# Look for SegmentedControl-related QML
sshpass -e ssh $SSH_OPTS "$RMPP" 'strings /usr/bin/xochitl | grep -E "SegmentedControl|InputField"' | head -50
