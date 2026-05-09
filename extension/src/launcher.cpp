#include "launcher.h"
#include "detect.h"
#include <QDebug>
#include <QStringList>
#include <QProcessEnvironment>

namespace {
constexpr auto kVnseeBin   = "/home/root/xovi/exthome/appload/vnsee/vnsee";
constexpr auto kQtfbSocket = "/run/vncast.sock";
}

// ===== overlay routing ===================================================

void VncastLauncher::registerOverlayLoader(QObject *loader) {
    m_overlay = loader;
    qInfo() << "[vncast] registerOverlayLoader:" << loader;
}

void VncastLauncher::openOverlay() {
    if (!m_overlay) {
        qWarning() << "[vncast] openOverlay: no Loader registered yet — sidebar tap ignored";
        return;
    }
    m_overlay->setProperty("active", true);
    setOverlayOpen(true);
    qInfo() << "[vncast] openOverlay";
}

void VncastLauncher::closeOverlay() {
    if (!m_overlay) return;
    m_overlay->setProperty("active", false);
    setOverlayOpen(false);
    qInfo() << "[vncast] closeOverlay";
}

void VncastLauncher::setOverlayOpen(bool v) {
    if (m_overlayOpen == v) return;
    m_overlayOpen = v;
    emit overlayOpenChanged();
}

void VncastLauncher::setStatus(const QString &s) {
    if (m_status == s) return;
    m_status = s;
    qInfo().noquote() << "[vncast/status]" << s;
    emit sessionStatusChanged();
}

// ===== qtfb server lifecycle =============================================

bool VncastLauncher::ensureServerStarted() {
    if (m_server && m_server->shmAddress()) return true;

    if (!m_server) {
        m_server = new vncast::qtfb::Server(this);
        // Mirror server-side connection events into the human-readable
        // status line so session.qml can show progress without having to
        // listen to the Server itself.
        connect(m_server, &vncast::qtfb::Server::clientConnected, this, [this]{
            setStatus(QStringLiteral("vnsee connected — waiting for first frame…"));
        });
        connect(m_server, &vncast::qtfb::Server::clientDisconnected, this, [this]{
            setStatus(QStringLiteral("vnsee disconnected"));
        });
        connect(m_server, &vncast::qtfb::Server::frameReady, this,
                [this](uint32_t seq, int, int, int, int){
            // Only update the status on the very first frame, otherwise
            // we'd flood the QML scene with notify signals.
            if (seq == 0 || seq == 1) {
                setStatus(QStringLiteral("Receiving frames"));
            }
        });
    }
    DeviceInfo info;
    setStatus(QStringLiteral("Allocating shared framebuffer (%1×%2, %3 bpp)…")
                .arg(info.fbWidth()).arg(info.fbHeight()).arg(info.fbBpp()));
    if (!m_server->start(&info)) {
        setStatus(QStringLiteral("Framebuffer server failed to start"));
        qWarning() << "[vncast] qtfb::Server failed to start";
        m_server->deleteLater();
        m_server = nullptr;
        emit qtfbServerChanged();
        return false;
    }
    emit qtfbServerChanged();
    return true;
}

// ===== session lifecycle =================================================

void VncastLauncher::launchVnsee() {
    qint64 pid = 0;
    bool ok = QProcess::startDetached(QString::fromLatin1(kVnseeBin),
                                      QStringList(), QString(), &pid);
    qInfo().noquote() << "[vncast] launchVnsee (legacy): ok=" << ok << "pid=" << pid;
}

void VncastLauncher::startSession(const QVariantMap &cfg) {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        qInfo() << "[vncast] startSession: already running, refusing";
        return;
    }

    setStatus(QStringLiteral("Starting framebuffer server…"));
    if (!ensureServerStarted()) {
        emit sessionEnded(-1);
        return;
    }

    const QString host        = cfg.value("host", "10.11.99.2").toString();
    const int     port        = cfg.value("port", 5900).toInt();
    const int     fps         = cfg.value("fps", 8).toInt();
    const QString encoding    = cfg.value("encoding", "COPYRECT").toString();
    const QString waveform    = cfg.value("waveform", "A2").toString();
    const QString orientation = cfg.value("orientation", "auto").toString();

    QStringList args;
    args << QStringLiteral("%1:%2").arg(host).arg(port);
    args << QStringLiteral("--fps") << QString::number(fps);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VNCAST_QTFB_SOCKET", QString::fromLatin1(kQtfbSocket));
    env.insert("VNSEE_ENCODING",     encoding);
    env.insert("VNCAST_WAVEFORM",    waveform);
    env.insert("VNCAST_ORIENTATION", orientation);

    setStatus(QStringLiteral("Spawning vnsee → %1:%2…").arg(host).arg(port));

    auto *p = new QProcess(this);
    p->setProgram(QString::fromLatin1(kVnseeBin));
    p->setArguments(args);
    p->setProcessEnvironment(env);
    p->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus){
        qInfo() << "[vncast] vnsee exited code=" << code;
        if (code == 0) setStatus(QStringLiteral("vnsee exited cleanly"));
        else           setStatus(QStringLiteral("vnsee exited (code %1)").arg(code));
        emit sessionEnded(code);
        if (m_proc) m_proc->deleteLater();
        m_proc.clear();
    });
    p->start();
    if (!p->waitForStarted(2000)) {
        setStatus(QStringLiteral("vnsee failed to start: %1").arg(p->errorString()));
        qWarning() << "[vncast] vnsee failed to start:" << p->errorString();
        p->deleteLater();
        emit sessionEnded(-1);
        return;
    }
    m_proc = p;
    setStatus(QStringLiteral("vnsee running (pid %1) — waiting for it to connect…")
                .arg(p->processId()));
    qInfo().noquote() << "[vncast] startSession: spawned" << kVnseeBin
                      << "pid=" << p->processId()
                      << "args=" << args.join(' ')
                      << "qtfb=" << kQtfbSocket;
    emit sessionStarted();
}

void VncastLauncher::stopSession() {
    if (!m_proc || m_proc->state() == QProcess::NotRunning) {
        setStatus(QStringLiteral("Disconnected"));
        emit sessionEnded(0);
        return;
    }
    setStatus(QStringLiteral("Stopping vnsee…"));
    qInfo() << "[vncast] stopSession: terminating pid=" << m_proc->processId();
    m_proc->terminate();
    if (!m_proc->waitForFinished(1500)) {
        m_proc->kill();
        m_proc->waitForFinished(1500);
    }
    setStatus(QStringLiteral("Disconnected"));
}
