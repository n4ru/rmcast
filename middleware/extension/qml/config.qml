// vncast — connection config page (fullscreen).
//
// Sidebar is hidden while Cast is open (the Loader anchors fill the entire
// panel — see menu-icon.qmldiff). The only exits are Cancel/Connect.
//
// Knobs:
//   - Host           the VNC server address
//   - Color mode     Color | Grayscale | Monochrome — collapses the older
//                    Fast-B&W and 1-bit-mono toggles into one progression.
//
// Rotation, port, and waveform are no longer surfaced. Defaults work for
// the LAN/USB-tether case; rotation can be locked from the rMPP's
// quicksettings panel if needed. Power users can edit ~/.config/vncast.json
// directly to override the hidden knobs (port, fps, waveform, encoding,
// compressLevel).
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "white"
    focus: true

    signal requestClose()
    signal requestConnect()

    // Single field combining host and port for the UI. Stored in Settings as
    // two separate values so the launcher can keep using the existing
    // (host, port) signature and so we don't surface ":5900" noise when the
    // user is on the default port.
    property string hostInput:
          Settings.port === 5900
              ? Settings.host
              : Settings.host + ":" + Settings.port
    property bool   grayscale: Settings.grayscale
    property bool   mono1:     Settings.mono1

    // Parse "host", "host:port", or "[ipv6]:port" into {host, port}. Falls
    // back to (input, 5900) on any ambiguity (bare IPv6 with multiple
    // colons, non-numeric trailing segment, etc).
    function parseHostPort(s) {
        var t = (s || "").trim();
        if (t === "")               return { host: "", port: 5900 };
        // [ipv6]:port form
        if (t.charAt(0) === "[") {
            var rb = t.indexOf("]");
            if (rb > 0) {
                var ipv6 = t.substring(1, rb);
                var rest = t.substring(rb + 1);
                if (rest.length === 0) return { host: ipv6, port: 5900 };
                if (rest.charAt(0) === ":") {
                    var pp = parseInt(rest.substring(1));
                    if (!isNaN(pp) && pp >= 1 && pp <= 65535)
                        return { host: ipv6, port: pp };
                }
            }
            return { host: t, port: 5900 };
        }
        // host:port — split on the LAST colon, but only if there's exactly
        // one colon (avoids misparsing a bare unbracketed IPv6).
        var first = t.indexOf(":");
        var last  = t.lastIndexOf(":");
        if (first === -1)            return { host: t, port: 5900 };
        if (first !== last)          return { host: t, port: 5900 };  // ambiguous
        var p = parseInt(t.substring(last + 1));
        if (isNaN(p) || p < 1 || p > 65535)
            return { host: t, port: 5900 };
        return { host: t.substring(0, last), port: p };
    }

    // Derived: which segment is currently selected. The progression is
    //   Color      → grayscale=false, mono1=false  (full-luma, color encoding)
    //   Grayscale  → grayscale=true,  mono1=false  (force luma, color encoding)
    //   Monochrome → grayscale=true,  mono1=true   (force luma + 1-bit wire)
    // The (false, true) corner — color content, 1-bit wire — isn't a
    // useful combination; we map it to Monochrome so the UI is total.
    readonly property string colorMode:
          root.mono1               ? "Monochrome"
        : root.grayscale           ? "Grayscale"
                                   : "Color"

    Component.onCompleted: console.log("[vncast/config.qml] loaded; hostInput=" + hostInput)
    Keys.onEscapePressed: root.requestClose()

    function dismissKeyboard() {
        root.forceActiveFocus();
        if (Qt.inputMethod) Qt.inputMethod.hide();
    }

    function commitAndConnect() {
        var hp = root.parseHostPort(root.hostInput)
        Settings.host        = hp.host
        Settings.port        = hp.port
        Settings.grayscale   = root.grayscale
        Settings.mono1       = root.mono1
        // (fps/waveform/orientation/encoding/compressLevel keep their
        // already-loaded values — we don't touch them here.)
        Settings.save()
        Vncast.startSession(Settings.asMap())
        root.requestConnect()
    }

    // ===== header =====
    // Cast is fullscreen now; bumping margins so the form doesn't sit
    // flush against the very left edge of the panel.
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 168
        Text {
            anchors {
                left: parent.left
                bottom: parent.bottom
                bottomMargin: 24
                leftMargin: 160
            }
            text: "Cast"
            font.family: "EB Garamond"
            font.weight: Font.Normal
            font.pixelSize: 56
            color: "black"
        }
    }

    // ===== form =====
    Column {
        id: form
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            leftMargin: 160
            rightMargin: 160
            topMargin: 8
        }
        spacing: 28

        // ---- Host card ----
        Column {
            width: parent.width
            spacing: 8
            Rectangle {
                width: parent.width
                height: hostCell.height
                color: "white"
                border { color: "black"; width: 1 }
                radius: 8

                Item {
                    id: hostCell
                    width: parent.width
                    height: 88
                    Text {
                        id: hostLabel
                        text: "Host"
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 24 }
                        font.family: "Noto Sans"
                        font.pixelSize: 22
                        color: "black"
                    }
                    TextInput {
                        id: hostField
                        text: root.hostInput
                        anchors {
                            left: hostLabel.right
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: 24
                            rightMargin: 24
                        }
                        horizontalAlignment: TextInput.AlignRight
                        font.family: "Noto Sans"
                        font.pixelSize: 22
                        color: "black"
                        selectByMouse: true
                        onTextChanged: root.hostInput = text
                        onAccepted: root.dismissKeyboard()
                        onActiveFocusChanged: if (!activeFocus && Qt.inputMethod) Qt.inputMethod.hide()
                    }
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignRight
                text: "Optional :port (default 5900)"
                font.family: "Noto Sans"
                font.pixelSize: 16
                color: "black"
                opacity: 0.5
            }
        }

        // ---- Color mode segment ----
        Column {
            width: parent.width
            spacing: 10
            Text {
                text: "Color mode"
                font.family: "Noto Sans"
                font.pixelSize: 22
                color: "black"
            }
            SegmentRow {
                width: parent.width
                height: 56
                options: ["Color", "Grayscale", "Monochrome"]
                current: root.colorMode
                onPicked: function(v) {
                    if (v === "Color") {
                        root.grayscale = false
                        root.mono1     = false
                    } else if (v === "Grayscale") {
                        root.grayscale = true
                        root.mono1     = false
                    } else { // "Monochrome"
                        root.grayscale = true
                        root.mono1     = true
                    }
                    root.dismissKeyboard()
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: root.colorMode === "Color"
                          ? "Full-luma source — best fidelity, slowest refresh."
                    : root.colorMode === "Grayscale"
                          ? "Luma-only source — A2-friendly, no chroma ghosting."
                    : "1-bit on the wire — fastest, requires a rewire-aware host."
                font.family: "Noto Sans"
                font.pixelSize: 18
                color: "black"
                opacity: 0.5
            }
        }

        // ---- Action buttons (Cancel | Connect) ----
        Item { width: parent.width; height: 16 }   // spacer
        Row {
            width: parent.width
            spacing: 24
            OutlineButton {
                width: (parent.width - 24) / 2
                height: 76
                text: "Cancel"
                onTapped: { root.dismissKeyboard(); root.requestClose() }
            }
            FilledButton {
                width: (parent.width - 24) / 2
                height: 76
                text: "Connect"
                onTapped: { root.dismissKeyboard(); root.commitAndConnect() }
            }
        }
    }
}
