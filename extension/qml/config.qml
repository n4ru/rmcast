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
        Settings.save()
        Vncast.startSession(Settings.asMap())
        root.requestConnect()
    }

    // Backdrop tap inside the panel: only dismiss the keyboard. Closing
    // happens via Cancel/Connect or via the sidebar tap-watcher in
    // MainView's qmldiff.
    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: root.dismissKeyboard()
    }

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

        // ---- Refresh rate (no card; matches Flight mode row) ----
        Item {
            width: parent.width
            height: 64
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "Refresh rate"
                font.family: "Noto Sans"
                font.pixelSize: 22
                color: "black"
            }
            SegmentRow {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                width: 480
                height: 48
                options: ["4", "8", "12", "free"]
                current: root.fps === 4 ? "4" : root.fps === 8 ? "8" : root.fps === 12 ? "12" : "free"
                onPicked: function(v) {
                    root.fps = v === "free" ? 0 : parseInt(v)
                    root.dismissKeyboard()
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

                // Waveform row
                Column {
                    width: parent.width - 48
                    x: 24
                    spacing: 10
                    topPadding: 14
                    bottomPadding: 14
                    Text {
                        text: "Waveform"
                        font.family: "Noto Sans"
                        font.pixelSize: 19
                        color: "black"
                    }
                    SegmentRow {
                        width: parent.width
                        height: 48
                        options: ["A2 fast", "DU flicker", "GC16 sharp"]
                        current: root.waveform === "A2"   ? "A2 fast"
                               : root.waveform === "DU"   ? "DU flicker"
                               : "GC16 sharp"
                        onPicked: function(v) {
                            root.waveform = v === "A2 fast" ? "A2"
                                          : v === "DU flicker" ? "DU"
                                          : "GC16"
                            root.dismissKeyboard()
                        }
                    }
                }
                Rectangle { width: parent.width - 32; x: 16; height: 1; color: "black"; opacity: 0.12 }

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
                Rectangle { width: parent.width - 32; x: 16; height: 1; color: "black"; opacity: 0.12 }

                // Encoding row
                Column {
                    width: parent.width - 48
                    x: 24
                    spacing: 10
                    topPadding: 14
                    bottomPadding: 14
                    Text {
                        text: "Encoding"
                        font.family: "Noto Sans"
                        font.pixelSize: 19
                        color: "black"
                    }
                    SegmentRow {
                        width: parent.width
                        height: 48
                        options: ["COPYRECT", "TIGHT", "ZRLE"]
                        current: root.encoding
                        onPicked: function(v) {
                            root.encoding = v
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
