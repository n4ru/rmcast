// Placeholder session view — pure QtQuick, native typography.
// Replaced by FBController + Disconnect once qtfb is vendored.
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    color: "white"
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/session.qml] loaded")

    // EB Garamond title for symmetry with the config page header.
    Text {
        id: title
        anchors {
            top: parent.top
            left: parent.left
            verticalCenter: undefined
            leftMargin: 64
            topMargin: 50
        }
        text: "Cast"
        font.family: "EB Garamond"
        font.pixelSize: 44
        color: "black"
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: title.bottom; topMargin: 40 }
        height: 1
        color: "black"
        opacity: 0.15
    }

    // Session status text (replaced by FBController paint once qtfb lands)
    Column {
        anchors {
            top: title.bottom
            left: parent.left
            right: parent.right
            margins: 64
            topMargin: 80
        }
        spacing: 16
        Text {
            text: "Connecting to " + Settings.host + ":" + Settings.port + "…"
            font.family: "Noto Sans"
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: "black"
        }
        Text {
            text: "(qtfb not wired yet — vnsee will exit; tap Disconnect)"
            font.family: "Noto Sans"
            font.pixelSize: 18
            color: "black"
            opacity: 0.6
        }
    }

    // Disconnect under the session content, "Turn off"-styled.
    FilledButton {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 64
        }
        height: 80
        text: "Disconnect"
        onTapped: { Vncast.stopSession(); root.requestClose() }
    }
}
