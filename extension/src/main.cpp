// rmpp-vnsee — xovi extension entry point.
//
// Boot:
//   1. Register QML singletons under net.example.Vncast 1.0:
//      - Vncast    → VncastLauncher (overlay open/close, session lifecycle)
//      - Settings  → persisted prefs in ~/.config/vncast.json
//      - Device    → runtime fb geometry + device class (rM2/rMPP/rMPPM)
//   2. Register our QRC resources (qrc:/vnsee/...) wrapped in qmldiff
//      slot disable/enable so the resource registration itself doesn't
//      re-trigger qmldiff slot processing for our own QML.
//   3. The qmldiff itself ships separately as
//      /home/root/xovi/exthome/qt-resource-rebuilder/vncast-menu-icon.qmd
//      so the .so isn't version-coupled to the diff.
//
// qtfb server is built in but not yet started — wiring it into
// VncastLauncher::startSession() lands in the next iteration.

#include <QtCore>
#include <QtQml/QQmlEngine>

#include "../resources.cpp"
#include "launcher.h"
#include "settings.h"
#include "detect.h"
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

        qt_resource_rebuilder$qmldiff_disable_slots_while_processing();
        qRegisterResourceData(3, qt_resource_struct, qt_resource_name, qt_resource_data);
        qt_resource_rebuilder$qmldiff_enable_slots_while_processing();

        qInfo() << "[vncast] _xovi_construct: registered singletons + QRC";
    }
}
