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
    // Log waveform request once per ~120 frames so we can see the setting
    // propagating without flooding the journal. TODO once we've hooked
    // EPFramebuffer.sendUpdate via xovi, replace this log with a direct
    // call into xochitl's EPDC path so A2 / DU / GC16 actually change
    // the panel refresh waveform.
    if (m_server && (seq % 120 == 1)) {
        qInfo().noquote() << "[vncast/frame_view] paint seq=" << seq
                          << "waveform=" << m_server->waveform();
    }
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

    const QRectF dst = boundingRect();

    // No rotation: scale-to-fit with aspect preserved (letterbox if needed).
    if (m_rotation == 0) {
        const QSize  src   = m_img.size();
        const QSizeF fit   = QSizeF(src).scaled(dst.size(), Qt::KeepAspectRatio);
        const QRectF tgt(dst.center() - QPointF(fit.width() / 2.0, fit.height() / 2.0),
                         fit);
        p->drawImage(tgt, m_img);
        return;
    }

    // Rotated: paint into the bounding rect with the source rotated by
    // m_rotation degrees clockwise around its centre, then aspect-fit.
    p->save();
    p->translate(dst.center());
    p->rotate(static_cast<qreal>(m_rotation));
    const QSize src = m_img.size();
    QSizeF logical(src);
    if (m_rotation == 90 || m_rotation == 270) {
        logical = QSizeF(src.height(), src.width());
    }
    const QSizeF fit = logical.scaled(dst.size(), Qt::KeepAspectRatio);
    QSizeF drawn = fit;
    if (m_rotation == 90 || m_rotation == 270) {
        drawn = QSizeF(fit.height(), fit.width());
    }
    const QRectF tgt(-drawn.width() / 2.0, -drawn.height() / 2.0,
                      drawn.width(),        drawn.height());
    p->drawImage(tgt, m_img);
    p->restore();
}

void FrameView::setRotation(int deg) {
    int n = ((deg % 360) + 360) % 360;
    if (n == m_rotation) return;
    m_rotation = n;
    update();
    emit rotationChanged();
}

}  // namespace vncast::qtfb
