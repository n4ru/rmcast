#include "launcher.h"
#include "detect.h"
#include "epdc_hook.h"
#include <QDebug>
#include <QStringList>
#include <QProcessEnvironment>

namespace {
// Map our user-facing waveform names → EPScreenMode integers. Stub for
// now; we'll fill in the real numbers once the [vncast/epdc] log shows
// what xochitl uses. Returning -1 means "pass through, don't force".
int waveform_to_mode(const QString &w) {
    Q_UNUSED(w);
    return -1;
}
}

namespace {
constexpr auto kVnseeBin   = "/home/root/xovi/exthome/appload/vnsee/vnsee";
constexpr auto kQtfbSocket = "/run/vncast.sock";
}

// ===== ctor — install EPDC hook at QML singleton init =====================
//
// We do this here (not in startSession) so logging captures xochitl's OWN
// swapBuffers calls — which is the cheapest way to map the EPScreenMode
// integers used by the panel for known UI states (status bar = GLR16 area,
// notebook page turn = GC16, etc.). Without an active session the values
// xochitl picks during normal use are exactly what we need to decode
// before we can sensibly call setForceMode().

VncastLauncher::VncastLauncher(QObject *parent) : QObject(parent) {
    // The libqsgepaper.so EPFramebuffer hooks don't fire on rMPP — xochitl
    // appears to drive the panel via the DRM platform plugin, not the
    // scenegraph plugin. Keep the install path opt-in for future RE work.
    if (qEnvironmentVariableIsSet("VNCAST_ENABLE_EPDC_HOOK")) {
        vncast::epdc::installHook();
        vncast::epdc::setLogging(true);
    }
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

bool VncastLauncher::ensureServerStarted(bool grayscale) {
    // If the server is already up but the requested shm format differs
    // (Fast B&W toggled mid-session), tear it down so we can reallocate
    // with the right format. The vnsee process should already have been
    // killed by stopSession() before we get here.
    if (m_server && m_server->shmAddress()) {
        if (m_server->usingGrayscaleShm() == grayscale) return true;
        qInfo() << "[vncast] server format mismatch (gray="
                << m_server->usingGrayscaleShm() << "→" << grayscale
                << ") — recreating";
        // Order matters here: emit nullptr FIRST so QML re-binds and
        // FrameView clears m_img (which aliases the old shm). Only then
        // schedule destruction. The shm itself stays mapped until the
        // destructor runs in the next event loop tick — so any in-flight
        // paint on the render thread completes safely against the old
        // bytes before the unmap happens.
        auto *old = m_server;
        m_server = nullptr;
        emit qtfbServerChanged();   // FrameView::setServer(nullptr) → m_img cleared
        old->stop();                // close socket; shm still mapped
        old->deleteLater();         // destructor (which freeShm) runs later
    }

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
            vncast::epdc::setActive(false);
        });
        connect(m_server, &vncast::qtfb::Server::frameReady, this,
                [this](uint32_t seq, int, int, int, int){
            // Only update the status on the very first frame, otherwise
            // we'd flood the QML scene with notify signals.
            if (seq == 0 || seq == 1) {
                setStatus(QStringLiteral("Receiving frames"));
                vncast::epdc::setActive(true);
                vncast::epdc::setForceMode(waveform_to_mode(m_server->waveform()));
            }
        });
    }
    DeviceInfo info;
    m_server->setUseGrayscaleShm(grayscale);
    setStatus(QStringLiteral("Allocating shared framebuffer (%1×%2, %3)…")
                .arg(info.fbWidth()).arg(info.fbHeight())
                .arg(grayscale ? "8 bpp gray" : "32 bpp RGBA"));
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

    const QString host        = cfg.value("host", "10.11.99.2").toString();
    const int     port        = cfg.value("port", 5900).toInt();
    const int     fps         = cfg.value("fps", 0).toInt();
    const QString encoding    = cfg.value("encoding", "COPYRECT").toString();
    const QString orientation = cfg.value("orientation", "auto").toString();
    const bool    grayscale   = cfg.value("grayscale",   true).toBool();
    const bool    mono1       = cfg.value("mono1",       false).toBool();
    const bool    lowLatency  = cfg.value("lowLatency",  false).toBool();
    // Low-latency mode forces the DU waveform regardless of what the
    // user (or default) had saved. DU is ~30ms faster than A2 on B&W
    // content — text, cursors, pen strokes — at the cost of grayscale
    // ghosting if the source isn't truly bilevel. Pairs with the
    // Monochrome color mode for the cleanest fast-typing experience.
    const QString waveform    = lowLatency ? QStringLiteral("DU")
                                           : cfg.value("waveform", "A2").toString();

    // Pre-create the server (if needed) so we can configure it BEFORE
    // start() decides on the shm format. Grayscale mode allocates the shm
    // as 1-byte/pixel grayscale, saving 75% of the conversion bandwidth.
    if (!m_server) {
        // Construct now without start()ing — ensureServerStarted does start.
    }
    setStatus(QStringLiteral("Starting framebuffer server…"));
    if (!ensureServerStarted(grayscale)) {
        emit sessionEnded(-1);
        return;
    }

    // EPDC hook is opt-in (see ctor) and currently a no-op on rMPP.

    // Push the waveform hint into the qtfb server so FrameView can pick
    // up the requested EPDC mode at paint time. (Actual EPDC override
    // requires hooking xochitl's EPFramebuffer.sendUpdate via xovi,
    // which lands in a follow-up; for now this just propagates the
    // setting end-to-end.)
    if (m_server) m_server->setWaveform(waveform);

    // vnsee parses positional arg as host[:display] where display is an
    // offset from base port 5900. Passing "host:5900" double-appends the
    // port; we pass host alone for default 5900, or "host:N" where N is
    // the display offset (port-5900) when caller picked a custom port.
    QStringList args;
    if (port == 5900) {
        args << host;
    } else {
        args << QStringLiteral("%1:%2").arg(host).arg(port - 5900);
    }
    // (No --fps yet: vnsee upstream doesn't accept it. Add to vnsee fork
    // and re-introduce here; for now fps is enforced server-side via the
    // VNCAST_FPS env var that the future vnsee fork will honor.)

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VNCAST_QTFB_SOCKET", QString::fromLatin1(kQtfbSocket));
    env.insert("VNCAST_FPS",         QString::number(fps));
    // Don't pin the encoding — let vnsee use its default
    // ("rcastmono1 copyrect tight zrle hextile raw"). Pinning to a
    // single legacy encoding here used to silently drop the rcastmono1
    // pseudo-encoding from the SetEncodings list, blocking rcast-host
    // from ever picking MONO1.
    if (encoding != "COPYRECT")
        env.insert("VNSEE_ENCODING",     encoding);
    env.insert("VNCAST_WAVEFORM",    waveform);
    env.insert("VNCAST_ORIENTATION", orientation);
    env.insert("VNCAST_FORCE_GRAYSCALE", grayscale ? "1" : "0");
    env.insert("VNCAST_PREFER_MONO1",    mono1 ? "1" : "0");
    env.insert("VNCAST_COMPRESS_LEVEL",
               QString::number(cfg.value("compressLevel", 0).toInt()));

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
