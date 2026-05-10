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
    Q_PROPERTY(uint lastFrameMode READ lastFrameMode NOTIFY lastFrameModeChanged)
    Q_PROPERTY(int  cursorX       READ cursorX       NOTIFY cursorChanged)
    Q_PROPERTY(int  cursorY       READ cursorY       NOTIFY cursorChanged)
    Q_PROPERTY(int  cursorW       READ cursorW       NOTIFY cursorChanged)
    Q_PROPERTY(int  cursorH       READ cursorH       NOTIFY cursorChanged)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY cursorChanged)
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

    /** When true, allocate the shm as 1-byte/pixel grayscale (Format=2)
     *  so vnsee can blit RGB565→8-bit luma directly without expanding to
     *  RGBA8888. ~75% less memcpy bandwidth on the conversion path.
     *  Decided at start() time; runtime toggling requires reconnect. */
    void       setUseGrayscaleShm(bool v) { m_use_grayscale_shm = v; }
    bool       usingGrayscaleShm() const  { return m_use_grayscale_shm; }

    /** Per-frame mode hint from the most recent FRAME (FrameMode enum).
     *  0xFFFFFFFF if the client never set one. QML binds against this via
     *  FrameView so an Epaper.ScreenModeItem can re-tag the area per frame. */
    uint32_t lastFrameMode() const { return m_last_frame_mode; }

    /** Cursor hotspot reported by the VNC server, in source-image coords.
     *  QML positions a small fast-mode ScreenModeItem at this point so
     *  the cursor area always refreshes quickly even when surrounding
     *  content needs slow GC16. (-1, -1) until first update. */
    int  cursorX() const       { return m_cursor_x; }
    int  cursorY() const       { return m_cursor_y; }
    int  cursorW() const       { return m_cursor_w; }
    int  cursorH() const       { return m_cursor_h; }
    bool cursorVisible() const { return m_cursor_visible; }

signals:
    // Fires on every client FRAME message. dirty=(0,0,0,0) means full screen.
    void frameReady(uint32_t seq, int x, int y, int dw, int dh);
    // Fires when the per-frame mode hint changed value (so QML doesn't
    // resubscribe / re-bind on every frame, only on actual transitions).
    void lastFrameModeChanged();
    void cursorChanged();
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

    // When true, shm is allocated as 1-byte/pixel grayscale (format=2).
    bool     m_use_grayscale_shm = false;

    // Per-frame mode hint from the most recent FRAME (FrameMode enum int).
    uint32_t m_last_frame_mode = 0xFFFFFFFFu;

    // Latest cursor hotspot from CursorC2S messages.
    int  m_cursor_x = -1, m_cursor_y = -1;
    int  m_cursor_w = 0,  m_cursor_h = 0;
    bool m_cursor_visible = false;
};

}  // namespace vncast::qtfb
