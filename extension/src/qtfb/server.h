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
};

}  // namespace vncast::qtfb
