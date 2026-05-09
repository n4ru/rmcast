// Segmented pill row, mimicking xochitl's Battery Standby selector:
// thick black outline around the whole row, options share borders, selected
// option fills black with white text.
import QtQuick 2.15

Item {
    id: root
    property var    options: []
    property string current: ""
    signal picked(string v)

    height: 56

    Rectangle {
        anchors.fill: parent
        color: "white"
        border { color: "black"; width: 2 }
        radius: 6
    }

    Row {
        anchors.fill: parent
        anchors.margins: 2
        Repeater {
            model: root.options
            delegate: Rectangle {
                width: (root.width - 4) / Math.max(1, root.options.length)
                height: parent.height
                color: modelData === root.current ? "black" : "white"
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.family: "Noto Sans"
                    font.pixelSize: 20
                    color: modelData === root.current ? "white" : "black"
                }
                // separator between segments (except after last)
                Rectangle {
                    visible: index < root.options.length - 1
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    width: 2
                    color: "black"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.current = modelData; root.picked(modelData) }
                }
            }
        }
    }
}
