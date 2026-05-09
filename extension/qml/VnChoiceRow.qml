// Horizontal exclusive choice row — pure QtQuick, no Controls.
import QtQuick 2.15

Item {
    id: root
    property string label: ""
    property var    options: []
    property string current: ""
    signal picked(string v)

    height: 60

    Text {
        id: lab
        text: root.label
        font.pixelSize: 22
        verticalAlignment: Text.AlignVCenter
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        width: 140
    }

    Row {
        anchors { left: lab.right; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 16 }
        spacing: 12
        Repeater {
            model: root.options
            delegate: Rectangle {
                width: 200; height: 56
                color: modelData === root.current ? "black" : "white"
                border { color: "black"; width: 2 }
                radius: 4
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 20
                    color: modelData === root.current ? "white" : "black"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.current = modelData; root.picked(modelData) }
                }
            }
        }
    }
}
