#pragma once
#include <QObject>
#include <QString>
#include <QPointer>
#include <QLocalServer>
#include <QLocalSocket>
#include "protocol.h"
#include "../detect.h"

namespace vncast::qtfb {

// vncast-side qtfb server. Owns the Unix socket, the shm region, and the
// frame counter. Emits frameReady() on a Qt::QueuedConnection so QML/UI
// repaints happen on the GUI thread.
//
// Lifecycle:
//   start(deviceInfo)   → create shm sized for fb, listen on /run/vncast.sock
//   stop()              → close socket, unlink shm
//   shmAddress() / shmSize() / w()/h()/stride() — for FBController consumer
class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server() override;

    bool   start(const DeviceInfo *info);
    void   stop();

    bool       isClientConnected() const { return m_socket; }
    const uchar *shmAddress()      const { return m_shm; }
    size_t      shmSize()          const { return m_shmSize; }
    int         w()                const { return m_w; }
    int         h()                const { return m_h; }
    int         stride()           const { return m_stride; }
    uint32_t    bpp()              const { return m_bpp; }

    /** Waveform hint set by VncastLauncher from Settings.waveform.
     *  Strings: "A2", "DU", "GC16". FrameView reads this when painting
     *  to decide which EPDC waveform to request. (TODO: actually request
     *  it once we hook EPFramebuffer.sendUpdate via xovi.) */
    QString    waveform()          const { return m_waveform; }
    void       setWaveform(const QString &w) { m_waveform = w; }

signals:
    // Fires on every client FRAME message. dirty=(0,0,0,0) means full screen.
    void frameReady(uint32_t seq, int x, int y, int dw, int dh);
    void clientConnected();
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    bool   allocShm(size_t bytes);
    void   freeShm();
    void   handleHello(const HelloC2S &);

    QLocalServer       *m_server  = nullptr;
    QPointer<QLocalSocket> m_socket;
    int                 m_shmFd   = -1;
    uchar              *m_shm     = nullptr;
    size_t              m_shmSize = 0;
    QString             m_shmName;

    int      m_w = 0, m_h = 0, m_stride = 0;
    uint32_t m_bpp = 0, m_format = 0;

    // Server-side fps cap negotiated in HELLO. 0 = no cap.
    uint32_t m_fps_cap = 0;
    qint64   m_min_period_ms = 0;
    qint64   m_last_emit_ms  = 0;

    // Waveform hint from VncastLauncher (Settings.waveform).
    QString  m_waveform = QStringLiteral("A2");
};

}  // namespace vncast::qtfb
