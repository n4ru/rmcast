#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QPointer>
#include <QElapsedTimer>
#include "server.h"

namespace vncast::qtfb {

// QML-instantiable item that paints whatever the qtfb shm currently
// contains. Aliases the shm bytes into a QImage (no per-frame copy);
// repaints on every frameReady from the Server.
//
// QML usage:
//   FrameView {
//       anchors.fill: parent
//       server: Vncast.qtfbServer
//   }
class FrameView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Server* server READ server WRITE setServer NOTIFY serverChanged)
    // 0 = none, 90/180/270 = clockwise rotation applied at paint time.
    // (Tried scenegraph-level rotation via QQuickItem::rotation; broke
    // the layout when combined with anchors.centerIn + width/height
    // swap on a QQuickPaintedItem. In-paint rotation it is.)
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
public:
    explicit FrameView(QQuickItem *parent = nullptr);

    Server* server() const { return m_server.data(); }
    void    setServer(Server *s);

    int  rotation() const { return m_rotation; }
    void setRotation(int deg);

    void paint(QPainter *painter) override;

signals:
    void serverChanged();
    void rotationChanged();

private slots:
    void onFrameReady(uint32_t seq, int x, int y, int dw, int dh);
    void rebuildImage();

private:
    // Recompute the cached layout (scale, offset, draw rect) from the
    // current bounding rect, source size, and rotation. Cheap; called
    // only on geometry/source changes, not per-frame.
    void recalcLayout();

    // Map a source-image rect to the FrameView's local coordinate system,
    // accounting for current rotation, scale, and aspect-fit offset.
    // Used to ask Qt scenegraph to only re-upload that subregion to the
    // backing texture instead of the whole frame.
    QRect mapSourceRect(int sx, int sy, int sw, int sh) const;

    QPointer<Server> m_server;
    QImage           m_img;     // aliases shm bytes; lifetime tied to server
    uint32_t         m_lastSeq = 0;
    int              m_rotation = 0;

    // Cached layout — invalidated on geometryChange / rebuildImage / setRotation.
    qreal   m_scale    = 1.0;
    QPointF m_offset   = QPointF(0, 0);
    QSizeF  m_drawn    = QSizeF(0, 0);   // post-aspect-fit, pre-rotation size
    bool    m_layoutValid = false;

    // Client-side latency tracking. m_pending_paint_timer is started in
    // onFrameReady (frame arrived via shm) and stopped at the end of
    // paint() — measures the QML/scenegraph half of the pipeline only.
    // Logs an average every m_lat_log_every frames so we have ground
    // truth alongside the server's FBUR→FBU stats.
    QElapsedTimer m_pending_paint_timer;
    bool          m_pending_paint = false;
    quint64       m_lat_window_us    = 0;
    quint64       m_lat_window_count = 0;
    static constexpr quint64 m_lat_log_every = 30;

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
};

}  // namespace vncast::qtfb
