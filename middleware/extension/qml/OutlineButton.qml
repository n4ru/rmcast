// Outlined button — matches "Cancel" / "Restart" pattern: thick black
// outline, white fill, inverts on press. Same size/typography as
// FilledButton so they can sit side-by-side without looking mismatched.
import QtQuick 2.15

Item {
    id: root
    property string text: ""
    signal tapped()

    implicitWidth: Math.max(220, label.implicitWidth + 80)
    implicitHeight: 76

    Rectangle {
        anchors.fill: parent
        color: ma.pressed ? "black" : "white"
        border { color: "black"; width: 2 }
        radius: 6
    }
    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.family: "Noto Sans"
        font.pixelSize: 22
        font.weight: Font.Medium
        color: ma.pressed ? "white" : "black"
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        onClicked: root.tapped()
    }
}
