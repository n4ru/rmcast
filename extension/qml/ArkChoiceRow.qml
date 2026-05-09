// One-of-N selector. Ark.SegmentedControl needs a declarative-children
// pattern we haven't reverse-engineered yet, and Ark.Button has no visible
// selected state. So this stays as a hand-rolled row with the same visual
// language as xochitl's filter chips: thick black border, filled-black =
// selected, white = unselected. Replace with Ark.SegmentedControl once we
// confirm the inner-Button delegate API.
import QtQuick 2.15

Item {
    id: root
    property string label: ""
    property var    options: []
    property string current: ""
    signal picked(string v)

    height: 64

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
            delegate: Item {
                width: 180; height: 56
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: modelData === root.current ? "black" : "white"
                    border { color: "black"; width: 2 }
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 20
                    color: modelData === root.current ? "white" : "black"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { root.current = modelData; root.picked(modelData); }
                }
            }
        }
    }
}
