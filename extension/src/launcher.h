#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QJSEngine>
#include <QVariantMap>
#include <QPointer>
#include <QProcess>

// QML-callable singleton. Exposed as `Vncast` under net.example.Vncast 1.0.
//
// Overlay routing:
//   MainView.qml's qmldiff inserts a Loader (id _vncastView) and calls
//   Vncast.registerOverlayLoader(this) on Component.onCompleted. The
//   Sidebar item's onClicked calls Vncast.openOverlay() to flip the
//   Loader's `active` to true. The same singleton exposes `overlayOpen`
//   so the SidebarFilterItem can bind its `active` (selected) state to it
//   — the icon highlights while the panel is open, matching how Settings'
//   sidebar items show selection.
//
// Session lifecycle:
//   startSession(map) → spawn vnsee with config args. qtfb wiring lands
//   in step 2; today vnsee just exits because there's no shm consumer.
class VncastLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool overlayOpen READ overlayOpen NOTIFY overlayOpenChanged)
public:
    explicit VncastLauncher(QObject *parent = nullptr) : QObject(parent) {}

    bool overlayOpen() const { return m_overlayOpen; }

    Q_INVOKABLE void registerOverlayLoader(QObject *loader);
    Q_INVOKABLE void openOverlay();
    Q_INVOKABLE void closeOverlay();

    Q_INVOKABLE void startSession(const QVariantMap &cfg);
    Q_INVOKABLE void stopSession();

    // Diagnostic-only direct spawn — left in for log parity with earlier work.
    Q_INVOKABLE void launchVnsee();

    static QObject *qmlSingleton(QQmlEngine *, QJSEngine *) {
        static VncastLauncher *inst = new VncastLauncher();
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }

signals:
    void sessionStarted();
    void sessionEnded(int exitCode);
    void overlayOpenChanged();

private:
    void setOverlayOpen(bool v);

    QPointer<QObject>  m_overlay;
    QPointer<QProcess> m_proc;
    bool               m_overlayOpen = false;
};
