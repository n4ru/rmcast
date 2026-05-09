// Active session view. Shows header + status line + Disconnect while we're
// connecting; once frames are flowing the chrome hides and the FrameView
// fills the full panel for true fullscreen mirror.
import QtQuick 2.15
import net.example.Vncast 1.0

Rectangle {
    id: root
    color: "white"
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/session.qml] loaded")

    readonly property bool fullscreen: Vncast.sessionStatus === "Receiving frames"

    // ---- header (visible only while connecting) ----
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 168
        visible: !root.fullscreen

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

    // ---- live status line (visible only while connecting) ----
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
        visible: !root.fullscreen
        text: Vncast.sessionStatus || ""
        font.family: "Noto Sans"
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: "black"
        opacity: 0.8
        wrapMode: Text.WordWrap
    }

    // ---- frame view ----
    // While connecting: anchored under header/status with a Disconnect row
    // below.  Once fullscreen: fills the entire root.
    FrameView {
        id: frameView
        server: Vncast.qtfbServer
        // Orientation: Auto/Landscape rotate the image 90 CW so a landscape
        // source paints upright when the rMPP is held landscape; Portrait
        // leaves the source unrotated (it'll letterbox if the source is
        // landscape, but at least it isn't squished).
        rotation: Settings.orientation === "portrait" ? 0 : 90
        anchors {
            top:    root.fullscreen ? parent.top    : statusLine.bottom
            left:   parent.left
            right:  parent.right
            bottom: root.fullscreen ? parent.bottom : disconnectRow.top
            margins: root.fullscreen ? 0 : 16
            topMargin: root.fullscreen ? 0 : 24
        }
    }

    // ---- disconnect row (chrome) ----
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
        visible: !root.fullscreen
        FilledButton {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: 280
            height: 76
            text: "Disconnect"
            onTapped: { Vncast.stopSession(); root.requestClose() }
        }
    }

    // ---- floating Disconnect (visible only in fullscreen) ----
    // Small corner control so the user can always exit even when the
    // FrameView is covering everything.
    FilledButton {
        anchors {
            top:   parent.top
            right: parent.right
            margins: 16
        }
        visible: root.fullscreen
        width: 200
        height: 56
        text: "Disconnect"
        onTapped: { Vncast.stopSession(); root.requestClose() }
    }
}
