#include "frame_view.h"
#include <QPainter>
#include <QDebug>

namespace vncast::qtfb {

FrameView::FrameView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
}

void FrameView::setServer(Server *s) {
    if (m_server.data() == s) return;
    if (m_server) {
        disconnect(m_server.data(), &Server::frameReady, this, &FrameView::onFrameReady);
        disconnect(m_server.data(), &Server::clientConnected, this, &FrameView::rebuildImage);
        disconnect(m_server.data(), &Server::clientDisconnected, this, &FrameView::rebuildImage);
    }
    m_server = s;
    if (m_server) {
        connect(m_server.data(), &Server::frameReady, this, &FrameView::onFrameReady);
        connect(m_server.data(), &Server::clientConnected, this, &FrameView::rebuildImage);
        connect(m_server.data(), &Server::clientDisconnected, this, &FrameView::rebuildImage);
        rebuildImage();
    } else {
        m_img = QImage();
    }
    update();
    emit serverChanged();
}

void FrameView::rebuildImage() {
    if (!m_server || !m_server->shmAddress() || m_server->w() <= 0 || m_server->h() <= 0) {
        m_img = QImage();
        return;
    }
    const QImage::Format fmt = (m_server->bpp() == 16)
        ? QImage::Format_Grayscale16
        : QImage::Format_RGBA8888;
    m_img = QImage(const_cast<uchar *>(m_server->shmAddress()),
                   m_server->w(), m_server->h(), m_server->stride(), fmt);
    qInfo().noquote() << "[vncast/frame_view] aliased shm:"
                      << m_img.width() << "x" << m_img.height()
                      << "stride=" << m_server->stride()
                      << "bpp=" << m_server->bpp();
}

void FrameView::onFrameReady(uint32_t seq, int /*x*/, int /*y*/, int /*dw*/, int /*dh*/) {
    m_lastSeq = seq;
    update();
}

void FrameView::paint(QPainter *p) {
    if (m_img.isNull()) {
        // Waiting state — placeholder centered text.
        p->setPen(Qt::black);
        QFont f("Noto Sans");
        f.setPixelSize(22);
        p->setFont(f);
        const QString msg = m_server
            ? (m_server->isClientConnected()
                ? QStringLiteral("Connected — waiting for frames…")
                : QStringLiteral("Waiting for vnsee to connect…"))
            : QStringLiteral("(no qtfb server)");
        p->drawText(boundingRect(), Qt::AlignCenter, msg);
        return;
    }
    // Scale-to-fit while preserving aspect (most VNC sessions match the
    // panel exactly, but be safe).
    const QRectF dst = boundingRect();
    p->drawImage(dst, m_img);
}

}  // namespace vncast::qtfb
