// rmpp-vnsee — placeholder overlay. Just proves the sidebar icon → Loader
// pipeline is wired correctly. The real config UI + FBController go here
// in the next iteration.
import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    // Required by Navigator.qml's Loader hookup (windowParent is bound when
    // the Loader's onLoaded fires).
    property var windowParent: null
    signal requestClose()

    Rectangle {
        anchors.fill: parent
        color: "white"
        border { color: "black"; width: 4 }

        Text {
            anchors.centerIn: parent
            text: "VNSee\n(placeholder — config UI lands next)"
            font.pixelSize: 48
            font.bold: true
            color: "black"
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            anchors { top: parent.top; right: parent.right; margins: 16 }
            width: 64; height: 64; radius: 32
            color: closeMa.pressed ? "black" : "white"
            border { color: "black"; width: 3 }
            Text {
                anchors.centerIn: parent
                text: "✕"
                font.pixelSize: 28; font.bold: true
                color: closeMa.pressed ? "white" : "black"
            }
            MouseArea {
                id: closeMa
                anchors.fill: parent
                onClicked: root.requestClose()
            }
        }
    }
}
