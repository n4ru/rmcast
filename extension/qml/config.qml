// vncast — connection config page. Layout closely follows General Settings:
//   - Header: serif title (EB Garamond Regular), thin separator
//   - Form: grouped cards with thin outlines
//   - Action buttons under the form (Restart / Turn off pattern)
//
// Backdrop tap dismisses the keyboard but does NOT close the panel — that
// only happens via Cancel/Connect or by tapping another sidebar item.
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "white"
    focus: true

    signal requestClose()
    signal requestConnect()

    property string host:        Settings.host
    property int    port:        Settings.port
    property int    fps:         Settings.fps
    property string waveform:    Settings.waveform
    property string orientation: Settings.orientation
    property string encoding:    Settings.encoding
    property bool   grayscale:   Settings.grayscale
    property bool   mono1:       Settings.mono1
    property bool   advanced:    false

    Component.onCompleted: console.log("[vncast/config.qml] loaded; host=" + host)
    Keys.onEscapePressed: root.requestClose()

    function dismissKeyboard() {
        root.forceActiveFocus();
        if (Qt.inputMethod) Qt.inputMethod.hide();
    }

    function commitAndConnect() {
        Settings.host        = root.host
        Settings.port        = root.port
        Settings.fps         = root.fps
        Settings.waveform    = root.waveform
        Settings.orientation = root.orientation
        Settings.encoding    = root.encoding
        Settings.grayscale   = root.grayscale
        Settings.mono1       = root.mono1
        Settings.save()
        Vncast.startSession(Settings.asMap())
        root.requestConnect()
    }

    // No backdrop MouseArea — earlier we had one that called
    // forceActiveFocus(root) to dismiss the keyboard, but that stole
    // focus from xochitl's navigator and made the sidebar collapse.
    // Now: taps in empty Cast area are true no-ops. Keyboard dismissal
    // happens via the per-field activeFocusChanged handlers (focus moves
    // to another field or the Connect button → keyboard hides) or by
    // pressing Enter on an alphanumeric field.

    // ===== header =====
    // General Settings reference: title ~64px tall text, anchored ~96px from
    // top edge with another ~32px between title and the first card.
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 168
        Text {
            anchors {
                left: parent.left
                bottom: parent.bottom
                bottomMargin: 24
                leftMargin: 80
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
            leftMargin: 80
            rightMargin: 80
            topMargin: 8
        }
        spacing: 28

        // ---- Connection card (Host) ----
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
                    text: root.host
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
                    onTextChanged: root.host = text
                    onAccepted: root.dismissKeyboard()
                    onActiveFocusChanged: if (!activeFocus && Qt.inputMethod) Qt.inputMethod.hide()
                }
            }
        }

        // ---- Fast B&W toggle (top-level, above Advanced) ----
        // The most-used knob — surfaced at the top of the form. Switches
        // panel waveform to A2 + coerces source pixels to grayscale for
        // ~3-4× faster refresh. Best for text/terminal/document scrolling.
        Item {
            width: parent.width
            height: 64
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "Fast B&W mode"
                font.family: "Noto Sans"
                font.pixelSize: 22
                color: "black"
            }
            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                width: 76
                height: 38
                radius: 19
                color: root.grayscale ? "black" : "white"
                border { color: "black"; width: 2 }
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: root.grayscale ? "white" : "black"
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.grayscale ? parent.width - width - 5 : 5
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.grayscale = !root.grayscale; root.dismissKeyboard() }
                }
            }
        }

        // ---- Mono (1-bit encoding) toggle ----
        // Distinct from Fast B&W: this controls whether vnsee advertises
        // the rcastmono1 pseudo-encoding so an rcast-host server sends
        // 1 bit/pixel on the wire. Pair with Fast B&W for true B&W cast;
        // turn off to keep grayscale levels even on rcast-host.
        Item {
            width: parent.width
            height: 64
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "1-bit mono encoding"
                font.family: "Noto Sans"
                font.pixelSize: 22
                color: "black"
            }
            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                width: 76
                height: 38
                radius: 19
                color: root.mono1 ? "black" : "white"
                border { color: "black"; width: 2 }
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: root.mono1 ? "white" : "black"
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.mono1 ? parent.width - width - 5 : 5
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.mono1 = !root.mono1; root.dismissKeyboard() }
                }
            }
        }

        // ---- Advanced toggle (no card) ----
        Item {
            width: parent.width
            height: 64
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "Advanced options"
                font.family: "Noto Sans"
                font.pixelSize: 22
                color: "black"
            }
            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                width: 76
                height: 38
                radius: 19
                color: root.advanced ? "black" : "white"
                border { color: "black"; width: 2 }
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: root.advanced ? "white" : "black"
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.advanced ? parent.width - width - 5 : 5
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.advanced = !root.advanced; root.dismissKeyboard() }
                }
            }
        }

        // ---- Advanced fields card ----
        Rectangle {
            visible: root.advanced
            width: parent.width
            height: advancedCol.height
            color: "white"
            border { color: "black"; width: 1 }
            radius: 8

            Column {
                id: advancedCol
                width: parent.width

                // Port row
                Item {
                    width: parent.width
                    height: 70
                    Text {
                        text: "Port"
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 24 }
                        font.family: "Noto Sans"
                        font.pixelSize: 19
                        color: "black"
                    }
                    TextInput {
                        id: portField
                        text: String(root.port)
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            rightMargin: 24
                        }
                        width: 200
                        horizontalAlignment: TextInput.AlignRight
                        font.family: "Noto Sans"
                        font.pixelSize: 19
                        color: "black"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 65535 }
                        onTextChanged: {
                            var v = parseInt(text)
                            if (!isNaN(v)) root.port = v
                        }
                        onAccepted: root.dismissKeyboard()
                        onActiveFocusChanged: if (!activeFocus && Qt.inputMethod) Qt.inputMethod.hide()
                    }
                }
                Rectangle { width: parent.width - 32; x: 16; height: 1; color: "black"; opacity: 0.12 }

                // (Refresh rate, Waveform, and Encoding selectors removed.
                // For LAN/USB-tether use the defaults — uncapped fps,
                // copyrect, A2 — are the right answer; live tuning is
                // mostly noise compared to the Fast B&W / Pen toggles.)

                // Orientation row
                Column {
                    width: parent.width - 48
                    x: 24
                    spacing: 10
                    topPadding: 14
                    bottomPadding: 14
                    Text {
                        text: "Orientation"
                        font.family: "Noto Sans"
                        font.pixelSize: 19
                        color: "black"
                    }
                    SegmentRow {
                        width: parent.width
                        height: 48
                        options: ["Auto", "Landscape", "Portrait"]
                        current: root.orientation === "landscape" ? "Landscape"
                               : root.orientation === "portrait"  ? "Portrait"
                               : "Auto"
                        onPicked: function(v) {
                            root.orientation = v.toLowerCase()
                            root.dismissKeyboard()
                        }
                    }
                }
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
