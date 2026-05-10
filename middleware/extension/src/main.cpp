// rmpp-vnsee — xovi extension entry point.
//
// Boot:
//   1. Register QML singletons under net.example.Vncast 1.0:
//      - Vncast    → VncastLauncher (overlay open/close, session lifecycle,
//                                    qtfb server property)
//      - Settings  → persisted prefs in ~/.config/vncast.json
//      - Device    → runtime fb geometry + device class
//   2. Register the qtfb FrameView as a QML type so session.qml can
//      instantiate it: `FrameView { server: Vncast.qtfbServer }`
//   3. Register our QRC resources (qrc:/vnsee/...) wrapped in qmldiff
//      slot disable/enable so the resource registration doesn't re-trigger
//      qmldiff slot processing for our own QML.
//   4. The qmldiff itself ships separately as
//      /home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd

#include <QtCore>
#include <QtQml/QQmlEngine>

#include "../resources.cpp"
#include "launcher.h"
#include "settings.h"
#include "detect.h"
#include "qtfb/frame_view.h"
#include "qtfb/server.h"
#include "xovi.h"

bool qRegisterResourceData(int version,
                           const unsigned char *tree,
                           const unsigned char *name,
                           const unsigned char *data);

extern "C" {
    void _xovi_construct() {
        qmlRegisterSingletonType<VncastLauncher>(
            "net.example.Vncast", 1, 0, "Vncast",
            &VncastLauncher::qmlSingleton);
        qmlRegisterSingletonType<Settings>(
            "net.example.Vncast", 1, 0, "Settings",
            &Settings::qmlSingleton);
        qmlRegisterSingletonType<DeviceInfo>(
            "net.example.Vncast", 1, 0, "Device",
            &DeviceInfo::qmlSingleton);

        // FrameView is a value-typed QML item, not a singleton.
        qmlRegisterType<vncast::qtfb::FrameView>(
            "net.example.Vncast", 1, 0, "FrameView");
        // Server is uncreatable from QML; only exposed as a property of
        // the Vncast singleton. Register the metatype so QML can hold it.
        qmlRegisterUncreatableType<vncast::qtfb::Server>(
            "net.example.Vncast", 1, 0, "QtfbServer",
            QStringLiteral("Owned by VncastLauncher; not user-creatable"));

        qt_resource_rebuilder$qmldiff_disable_slots_while_processing();
        qRegisterResourceData(3, qt_resource_struct, qt_resource_name, qt_resource_data);
        qt_resource_rebuilder$qmldiff_enable_slots_while_processing();

        qInfo() << "[vncast] _xovi_construct: registered singletons + types + QRC";
    }
}
