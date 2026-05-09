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
// Properties exposed to QML:
//   overlayOpen     — bool, drives sidebar highlight + Loader.active
//   qtfbServer      — Server*, FrameView attaches to it for paint
//   sessionStatus   — QString, human-readable status the session view
//                     shows under the header ("Spawning vnsee…",
//                     "Connected — receiving frames", "vnsee exited (1)")
class VncastLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool overlayOpen        READ overlayOpen    NOTIFY overlayOpenChanged)
    Q_PROPERTY(vncast::qtfb::Server* qtfbServer READ qtfbServer NOTIFY qtfbServerChanged)
    Q_PROPERTY(QString sessionStatus   READ sessionStatus  NOTIFY sessionStatusChanged)
public:
    explicit VncastLauncher(QObject *parent = nullptr) : QObject(parent) {}

    bool overlayOpen() const                 { return m_overlayOpen; }
    vncast::qtfb::Server *qtfbServer() const { return m_server; }
    QString sessionStatus() const            { return m_status; }

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
    void sessionStatusChanged();

private:
    void setOverlayOpen(bool v);
    bool ensureServerStarted();
    void setStatus(const QString &s);

    QPointer<QObject>      m_overlay;
    QPointer<QProcess>     m_proc;
    vncast::qtfb::Server  *m_server = nullptr;
    bool                   m_overlayOpen = false;
    QString                m_status;
};
