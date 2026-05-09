#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QJSEngine>
#include <QVariantMap>
#include <QPointer>
#include <QProcess>
#include "qtfb/server.h"

// QML-callable singleton. Exposed as `Vncast` under net.example.Vncast 1.0.
//
// Overlay routing:
//   MainView.qml's qmldiff inserts a Loader (id _vncastView) and calls
//   Vncast.registerOverlayLoader(this) on Component.onCompleted. The
//   Sidebar item's onClicked calls Vncast.openOverlay() to flip the
//   Loader's `active` to true. Same singleton exposes `overlayOpen` so
//   the SidebarFilterItem highlights while the panel is showing.
//
// Session lifecycle (Connect):
//   startSession(map)
//     → lazily start qtfb::Server (Unix socket + shm sized for the device)
//     → spawn vnsee with VNCAST_QTFB_SOCKET + waveform + fps + encoding env
//     → emit qtfbServerChanged so QML's FrameView can attach
//   stopSession (Disconnect):
//     → terminate vnsee
//     → keep server up briefly to drain final frames; tear down on signal
//
// FrameView attaches via the qtfbServer property and paints whatever the
// shm holds.
class VncastLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool overlayOpen READ overlayOpen NOTIFY overlayOpenChanged)
    Q_PROPERTY(vncast::qtfb::Server* qtfbServer READ qtfbServer NOTIFY qtfbServerChanged)
public:
    explicit VncastLauncher(QObject *parent = nullptr) : QObject(parent) {}

    bool overlayOpen() const                 { return m_overlayOpen; }
    vncast::qtfb::Server *qtfbServer() const { return m_server; }

    Q_INVOKABLE void registerOverlayLoader(QObject *loader);
    Q_INVOKABLE void openOverlay();
    Q_INVOKABLE void closeOverlay();

    Q_INVOKABLE void startSession(const QVariantMap &cfg);
    Q_INVOKABLE void stopSession();

    Q_INVOKABLE void launchVnsee();   // diagnostic plain spawn — kept

    static QObject *qmlSingleton(QQmlEngine *, QJSEngine *) {
        static VncastLauncher *inst = new VncastLauncher();
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }

signals:
    void sessionStarted();
    void sessionEnded(int exitCode);
    void overlayOpenChanged();
    void qtfbServerChanged();

private:
    void setOverlayOpen(bool v);
    bool ensureServerStarted();

    QPointer<QObject>      m_overlay;
    QPointer<QProcess>     m_proc;
    vncast::qtfb::Server  *m_server = nullptr;
    bool                   m_overlayOpen = false;
};
