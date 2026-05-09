#include "launcher.h"
#include <QDebug>
#include <QStringList>
#include <QProcessEnvironment>

namespace {
constexpr auto kVnseeBin = "/home/root/xovi/exthome/appload/vnsee/vnsee";
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
    env.insert("VNSEE_ENCODING",    encoding);
    env.insert("VNCAST_WAVEFORM",   waveform);
    env.insert("VNCAST_ORIENTATION", orientation);

    auto *p = new QProcess(this);
    p->setProgram(QString::fromLatin1(kVnseeBin));
    p->setArguments(args);
    p->setProcessEnvironment(env);
    p->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus){
        qInfo() << "[vncast] vnsee exited code=" << code;
        emit sessionEnded(code);
        if (m_proc) m_proc->deleteLater();
        m_proc.clear();
    });
    p->start();
    if (!p->waitForStarted(2000)) {
        qWarning() << "[vncast] vnsee failed to start:" << p->errorString();
        p->deleteLater();
        emit sessionEnded(-1);
        return;
    }
    m_proc = p;
    qInfo().noquote() << "[vncast] startSession: spawned" << kVnseeBin
                      << "pid=" << p->processId() << "args=" << args.join(' ');
    emit sessionStarted();
}

void VncastLauncher::stopSession() {
    if (!m_proc || m_proc->state() == QProcess::NotRunning) {
        emit sessionEnded(0);
        return;
    }
    qInfo() << "[vncast] stopSession: terminating pid=" << m_proc->processId();
    m_proc->terminate();
    if (!m_proc->waitForFinished(1500)) {
        m_proc->kill();
        m_proc->waitForFinished(1500);
    }
}
