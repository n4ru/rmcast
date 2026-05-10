// vncast launcher — chooses between config and session views.
import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent
    signal requestClose()

    Component.onCompleted: console.log("[vncast/launcher.qml] loaded")

    Loader {
        id: stage
        anchors.fill: parent
        source: "qrc:/vnsee/qml/config.qml"
        onStatusChanged: {
            if (status === Loader.Error) {
                console.error("[vncast/launcher] stage Loader error:", sourceComponent ? sourceComponent.errorString() : "?");
            } else if (status === Loader.Ready) {
                console.log("[vncast/launcher] stage Loader ready: " + source);
            }
        }
        onLoaded: {
            if (item) {
                if (item.requestClose)   item.requestClose.connect(root.requestClose);
                if (item.requestConnect) item.requestConnect.connect(function() {
                    stage.source = "qrc:/vnsee/qml/session.qml";
                });
            }
        }
    }
}
