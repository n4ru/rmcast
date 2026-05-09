// Active session view: paints whatever vnsee writes into the qtfb shm,
// with a live status line under the header so the connect handshake is
// observable. Header / typography mirrors the config page.
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    color: "white"
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/session.qml] loaded")

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

    // ---- live status line ----
    Text {
        id: statusLine
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            leftMargin: 80
            rightMargin: 80
            topMargin: 8
        }
        text: Vncast.sessionStatus || ""
        font.family: "Noto Sans"
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: "black"
        opacity: 0.8
        wrapMode: Text.WordWrap
    }

    // ---- frame view ----
    FrameView {
        id: frameView
        server: Vncast.qtfbServer
        anchors {
            top: statusLine.bottom
            left: parent.left
            right: parent.right
            bottom: disconnectRow.top
            margins: 16
            topMargin: 24
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
