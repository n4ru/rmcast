// Active session view: paints whatever vnsee writes into the qtfb shm.
// Header + FrameView + Disconnect.
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    color: "white"
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/session.qml] loaded")

    // Backdrop tap = close (Disconnect-equivalent for taps that don't land
    // on the FrameView or the Disconnect button). Matches config.qml's
    // "tap outside the form to dismiss" pattern.
    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: {
            console.log("[vncast/session.qml] backdrop tap → stop + close");
            Vncast.stopSession();
            root.requestClose();
        }
    }

    // ---- header ----
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
        Text {
            anchors {
                right: parent.right
                bottom: parent.bottom
                bottomMargin: 32
                rightMargin: 80
            }
            text: Settings.host + ":" + Settings.port
            font.family: "Noto Sans"
            font.pixelSize: 20
            color: "black"
            opacity: 0.6
        }
    }

    // ---- frame view ----
    FrameView {
        id: frameView
        server: Vncast.qtfbServer
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: disconnectRow.top
            margins: 16
        }
    }

    // ---- disconnect under the frame view ----
    Item {
        id: disconnectRow
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 80
            bottomMargin: 40
        }
        height: 76
        FilledButton {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: 280
            height: 76
            text: "Disconnect"
            onTapped: { Vncast.stopSession(); root.requestClose() }
        }
    }
}
