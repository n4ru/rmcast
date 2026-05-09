#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QPointer>
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
public:
    explicit FrameView(QQuickItem *parent = nullptr);

    Server* server() const { return m_server.data(); }
    void    setServer(Server *s);

    void paint(QPainter *painter) override;

signals:
    void serverChanged();

private slots:
    void onFrameReady(uint32_t seq, int x, int y, int dw, int dh);
    void rebuildImage();

private:
    QPointer<Server> m_server;
    QImage           m_img;     // aliases shm bytes; lifetime tied to server
    uint32_t         m_lastSeq = 0;
};

}  // namespace vncast::qtfb
