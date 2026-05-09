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
    // Always paint white background first.
    p->fillRect(boundingRect(), Qt::white);

    // Show a placeholder until a client has actually connected AND sent
    // at least one frame. (We pre-fill the shm with white at server start,
    // so m_img alone isn't a useful "have data" signal.)
    const bool noClient   = !m_server || !m_server->isClientConnected();
    const bool noFrameYet = (m_lastSeq == 0);
    if (noClient || noFrameYet) {
        p->setPen(Qt::black);
        QFont f("Noto Sans");
        f.setPixelSize(22);
        p->setFont(f);
        const QString msg = !m_server
            ? QStringLiteral("(no qtfb server)")
            : noClient
                ? QStringLiteral("Waiting for vnsee to connect…")
                : QStringLiteral("Connected — waiting for frames…");
        p->drawText(boundingRect(), Qt::AlignCenter, msg);
        return;
    }
    if (m_img.isNull()) {
        return;   // shouldn't happen, but be safe
    }
    // The qtfb shm is portrait (1620×2160 on rMPP) and vnsee writes into
    // it as-received from the VNC server. Most desktops are landscape
    // (e.g. 2160×1620), so we paint with a 90° clockwise rotation so the
    // landscape content reads upright when the device is held landscape.
    // (Once the qtfb protocol ships orientation negotiation, this becomes
    //  conditional on the source/panel aspect mismatch.)
    const QRectF dst = boundingRect();
    p->save();
    p->translate(dst.center());
    p->rotate(90.0);
    // After rotating, our drawing space is rotated. The image's aspect
    // is portrait (W=1620, H=2160) but the source content is landscape,
    // so swap width/height when computing the target rect.
    const QRectF rotated(-dst.height() / 2.0, -dst.width() / 2.0,
                          dst.height(),       dst.width());
    p->drawImage(rotated, m_img);
    p->restore();
}

}  // namespace vncast::qtfb
