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
    m_layoutValid = false;
    if (!m_server || !m_server->shmAddress() || m_server->w() <= 0 || m_server->h() <= 0) {
        m_img = QImage();
        return;
    }
    const int bpp = m_server->bpp();
    QImage::Format fmt;
    if (bpp == 8)        fmt = QImage::Format_Grayscale8;
    else if (bpp == 16)  fmt = QImage::Format_Grayscale16;
    else                 fmt = QImage::Format_RGBA8888;
    m_img = QImage(const_cast<uchar *>(m_server->shmAddress()),
                   m_server->w(), m_server->h(), m_server->stride(), fmt);
    qInfo().noquote() << "[vncast/frame_view] aliased shm:"
                      << m_img.width() << "x" << m_img.height()
                      << "stride=" << m_server->stride()
                      << "bpp=" << m_server->bpp();
}

void FrameView::onFrameReady(uint32_t seq, int x, int y, int dw, int dh) {
    const bool firstFrame = (m_lastSeq == 0);
    m_lastSeq = seq;
    if (m_server && (seq % 120 == 1)) {
        qInfo().noquote() << "[vncast/frame_view] paint seq=" << seq
                          << "waveform=" << m_server->waveform();
    }

    // Start the latency timer here — paint() will close it. We track only
    // one outstanding paint at a time; if Qt coalesces multiple frames
    // into one paint() call the timer covers the whole gap, which is
    // actually the right answer ("how long from frame-ready to pixels
    // committed to the scene").
    if (!m_pending_paint) {
        m_pending_paint_timer.start();
        m_pending_paint = true;
    }

    // First frame and full-screen sentinels (0,0,0,0): repaint everything.
    // Otherwise only invalidate the mapped subregion so Qt scenegraph
    // re-uploads ~just-the-cursor-area instead of the whole 14 MB texture.
    if (firstFrame || (dw <= 0 || dh <= 0)) {
        update();
        return;
    }
    QRect r = mapSourceRect(x, y, dw, dh);
    // Inflate by 1px to absorb any rounding error.
    r.adjust(-1, -1, 1, 1);
    if (r.isEmpty()) {
        update();
        return;
    }
    update(r);
}

void FrameView::paint(QPainter *p) {
    QElapsedTimer body_timer;
    body_timer.start();

    // Disable smooth (bilinear) scaling and antialiasing — neither buys
    // anything on an e-ink panel and both add measurable per-frame CPU.
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->setRenderHint(QPainter::Antialiasing,         false);
    p->setRenderHint(QPainter::TextAntialiasing,     false);

    if (!m_layoutValid) recalcLayout();

    // fillRect — only when needed. The earlier code unconditionally
    // filled the whole boundingRect on every paint(), which Qt clips
    // to the active update rect but still costs measurable cycles
    // (~10-20ms in the body=51-62ms typing-latency tests on rMPP).
    // Skip the fill when we know drawImage will cover the bounding
    // rect entirely: rotation 0 + offset (0,0) + drawn matches bounds.
    // The rotated path keeps the fill (rotated image has visible
    // letterbox margins that need clearing).
    const bool image_covers_bounds =
        (m_rotation == 0)
        && (m_offset.x() == 0.0 && m_offset.y() == 0.0)
        && (qFuzzyCompare(m_drawn.width(),  boundingRect().width())
         && qFuzzyCompare(m_drawn.height(), boundingRect().height()));
    if (!image_covers_bounds) {
        p->fillRect(boundingRect(), Qt::white);
    }

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

    if (m_rotation == 0) {
        // Pass an explicit source rect = full image. Avoids Qt's
        // "consider transparent border" path some painters take when no
        // src rect is given. Targeted blit only — Qt clips to the update
        // rect we passed.
        const QRectF tgt(m_offset, m_drawn);
        const QRectF src(0, 0, m_img.width(), m_img.height());
        p->drawImage(tgt, m_img, src);
    } else {
        // Rotated path: m_drawn already accounts for w/h swap.
        p->save();
        p->translate(boundingRect().center());
        p->rotate(static_cast<qreal>(m_rotation));
        const QRectF tgt(-m_drawn.width() / 2.0, -m_drawn.height() / 2.0,
                          m_drawn.width(),        m_drawn.height());
        p->drawImage(tgt, m_img);
        p->restore();
    }

    // Close both timers. m_pending_paint_timer measures
    // frameReady→paint-end (Qt scheduling + paint body); body_timer
    // measures just the work inside this function. The diff exposes
    // how much of the 'paint' latency is scenegraph/FBO/scheduling
    // overhead vs actual painting — which determines whether a switch
    // to a custom QSGNode would actually help.
    const quint64 body_us = static_cast<quint64>(body_timer.nsecsElapsed() / 1000);
    m_paint_body_window_us += body_us;
    if (m_pending_paint) {
        m_lat_window_us += static_cast<quint64>(m_pending_paint_timer.nsecsElapsed() / 1000);
        m_lat_window_count++;
        m_pending_paint = false;
        if (m_lat_window_count >= m_lat_log_every) {
            const double avg_total_ms = (double)m_lat_window_us / m_lat_window_count / 1000.0;
            const double avg_body_ms  = (double)m_paint_body_window_us / m_lat_window_count / 1000.0;
            const double avg_sched_ms = avg_total_ms - avg_body_ms;
            qInfo().noquote() << "[vncast/lat] frameReady→paint avg="
                              << QString::number(avg_total_ms, 'f', 2) << "ms"
                              << "(body=" << QString::number(avg_body_ms, 'f', 2) << "ms"
                              << "sched=" << QString::number(avg_sched_ms, 'f', 2) << "ms)"
                              << "over" << m_lat_window_count << "frames";
            m_lat_window_us         = 0;
            m_paint_body_window_us  = 0;
            m_lat_window_count      = 0;
        }
    }
}

void FrameView::recalcLayout() {
    m_layoutValid = true;
    if (m_img.isNull()) {
        m_scale = 1.0; m_offset = QPointF(0, 0); m_drawn = QSizeF(0, 0);
        return;
    }
    const QSizeF dst = boundingRect().size();
    QSizeF src(m_img.size());
    if (m_rotation == 90 || m_rotation == 270) {
        src = QSizeF(m_img.height(), m_img.width());
    }
    const QSizeF fit = src.scaled(dst, Qt::KeepAspectRatio);
    m_scale = (src.width() > 0) ? (fit.width() / src.width()) : 1.0;
    if (m_rotation == 90 || m_rotation == 270) {
        m_drawn = QSizeF(fit.height(), fit.width()); // pre-rotation drawn size
    } else {
        m_drawn = fit;
    }
    m_offset = QPointF((dst.width()  - fit.width())  / 2.0,
                       (dst.height() - fit.height()) / 2.0);
}

QRect FrameView::mapSourceRect(int sx, int sy, int sw, int sh) const {
    // Map a source-image rect into FrameView's local (paint) coords.
    // Must match exactly what paint() draws — otherwise update(QRect)
    // invalidates the wrong area and the actual changed pixels never get
    // repainted, leaving stale "missing chunk" patches.
    if (m_img.isNull() || m_scale <= 0) return QRect();
    const QRectF dst = boundingRect();
    const qreal s = m_scale;

    if (m_rotation == 0) {
        const QPointF tl = m_offset + QPointF(sx * s, sy * s);
        return QRectF(tl, QSizeF(sw * s, sh * s)).toAlignedRect();
    }

    // For rotated paths, the visual cast occupies a post-rotation rect.
    // m_drawn is stored as PRE-rotation size, so swap for 90/270.
    const qreal post_w = (m_rotation == 90 || m_rotation == 270)
                         ? m_drawn.height() : m_drawn.width();
    const qreal post_h = (m_rotation == 90 || m_rotation == 270)
                         ? m_drawn.width()  : m_drawn.height();
    const qreal left = (dst.width()  - post_w) / 2.0;
    const qreal top  = (dst.height() - post_h) / 2.0;

    if (m_rotation == 90) {
        // (sx,sy) → (left + post_w - (sy+sh)*s, top + sx*s); w=sh*s, h=sw*s
        return QRectF(left + post_w - (sy + sh) * s,
                      top + sx * s,
                      sh * s, sw * s).toAlignedRect();
    }
    if (m_rotation == 270) {
        // (sx,sy) → (left + sy*s, top + post_h - (sx+sw)*s); w=sh*s, h=sw*s
        return QRectF(left + sy * s,
                      top + post_h - (sx + sw) * s,
                      sh * s, sw * s).toAlignedRect();
    }
    // 180: (sx,sy) → (left + post_w - (sx+sw)*s, top + post_h - (sy+sh)*s)
    return QRectF(left + post_w - (sx + sw) * s,
                  top + post_h - (sy + sh) * s,
                  sw * s, sh * s).toAlignedRect();
}

void FrameView::setRotation(int deg) {
    int n = ((deg % 360) + 360) % 360;
    if (n == m_rotation) return;
    m_rotation = n;
    m_layoutValid = false;
    update();
    emit rotationChanged();
}

void FrameView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    m_layoutValid = false;
}

}  // namespace vncast::qtfb
