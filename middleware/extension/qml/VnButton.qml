// Tiny tap target — pure QtQuick, no Controls dependency.
import QtQuick 2.15

Rectangle {
    id: root
    property string label: ""
    property bool   bold: false
    signal tapped()
    color: ma.pressed ? "black" : "white"
    border { color: "black"; width: 3 }
    radius: 6
    Text {
        anchors.centerIn: parent
        text: root.label
        font.pixelSize: 26
        font.bold: root.bold
        color: ma.pressed ? "white" : "black"
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        onClicked: root.tapped()
    }
}
