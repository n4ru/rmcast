#include "server.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

namespace vncast::qtfb {

Server::Server(QObject *parent) : QObject(parent) {}
Server::~Server() { stop(); }

bool Server::start(const DeviceInfo *info) {
    if (m_server) return true;
    m_w      = info->fbWidth();
    m_h      = info->fbHeight();
    m_stride = info->fbStride();
    m_bpp    = info->fbBpp();
    m_format = (m_bpp == 16) ? 0 : 1;

    const size_t bytes = static_cast<size_t>(m_stride) * m_h;
    if (!allocShm(bytes)) {
        qWarning() << "[vncast/qtfb] allocShm failed for" << bytes << "bytes";
        return false;
    }

    QFile::remove(QString::fromLatin1(SOCKET));
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);
    if (!m_server->listen(QString::fromLatin1(SOCKET))) {
        qWarning() << "[vncast/qtfb] listen failed:" << m_server->errorString();
        freeShm();
        delete m_server; m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &Server::onNewConnection);
    qInfo().noquote() << "[vncast/qtfb] listening on" << SOCKET
                      << "fb=" << m_w << "x" << m_h
                      << "stride=" << m_stride << "bpp=" << m_bpp
                      << "shm=" << m_shmName << "(" << m_shmSize << "B)";
    return true;
}

void Server::stop() {
    if (m_socket) { m_socket->disconnectFromServer(); m_socket->deleteLater(); m_socket.clear(); }
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
    freeShm();
}

bool Server::allocShm(size_t bytes) {
    // Unique shm name per process+timestamp so we don't collide if a stale
    // mapping survives a vncast crash.
    m_shmName = QStringLiteral("/vncast-%1-%2")
                    .arg(::getpid())
                    .arg(QDateTime::currentMSecsSinceEpoch(), 0, 36);
    const QByteArray name = m_shmName.toLatin1();
    m_shmFd = ::shm_open(name.constData(), O_CREAT | O_RDWR | O_EXCL, 0600);
    if (m_shmFd < 0) return false;
    if (::ftruncate(m_shmFd, bytes) != 0) { ::close(m_shmFd); m_shmFd = -1; return false; }
    void *p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);
    if (p == MAP_FAILED) { ::close(m_shmFd); m_shmFd = -1; return false; }
    m_shm     = static_cast<uchar *>(p);
    m_shmSize = bytes;
    ::memset(m_shm, 0xFF, bytes); // pre-fill white so first paint isn't garbage
    return true;
}

void Server::freeShm() {
    if (m_shm)   { ::munmap(m_shm, m_shmSize); m_shm = nullptr; m_shmSize = 0; }
    if (m_shmFd >= 0) { ::close(m_shmFd); m_shmFd = -1; }
    if (!m_shmName.isEmpty()) { ::shm_unlink(m_shmName.toLatin1().constData()); m_shmName.clear(); }
}

void Server::onNewConnection() {
    auto *s = m_server->nextPendingConnection();
    if (m_socket) {
        // Already serving someone — refuse extras.
        s->disconnectFromServer();
        s->deleteLater();
        qInfo() << "[vncast/qtfb] rejecting second client; one already connected";
        return;
    }
    m_socket = s;
    connect(s, &QLocalSocket::readyRead,    this, &Server::onSocketReadyRead);
    connect(s, &QLocalSocket::disconnected, this, &Server::onSocketDisconnected);
    qInfo() << "[vncast/qtfb] client connected";
    emit clientConnected();
}

void Server::onSocketDisconnected() {
    qInfo() << "[vncast/qtfb] client disconnected";
    if (m_socket) m_socket->deleteLater();
    m_socket.clear();
    emit clientDisconnected();
}

void Server::onSocketReadyRead() {
    while (m_socket && m_socket->bytesAvailable() >= (qint64)sizeof(Header)) {
        Header h{};
        m_socket->peek(reinterpret_cast<char *>(&h), sizeof(h));
        if (h.magic != MAGIC) {
            qWarning() << "[vncast/qtfb] bad magic, closing";
            m_socket->disconnectFromServer();
            return;
        }
        switch (static_cast<Tag>(h.tag)) {
        case Tag::HelloC2S: {
            if (m_socket->bytesAvailable() < (qint64)sizeof(HelloC2S)) return;
            HelloC2S msg{};
            m_socket->read(reinterpret_cast<char *>(&msg), sizeof(msg));
            handleHello(msg);
            break;
        }
        case Tag::Frame: {
            if (m_socket->bytesAvailable() < (qint64)sizeof(Frame)) return;
            Frame msg{};
            m_socket->read(reinterpret_cast<char *>(&msg), sizeof(msg));
            // Apply the negotiated fps cap. We always swallow the message
            // (so the shm gets the new pixels), but skip the QML repaint
            // signal when the previous emit is too recent.
            if (m_fps_cap > 0 && m_min_period_ms > 0) {
                const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
                if (m_last_emit_ms != 0 && now_ms - m_last_emit_ms < m_min_period_ms) {
                    break;   // drop this frame for paint
                }
                m_last_emit_ms = now_ms;
            }
            emit frameReady(msg.seq, msg.x, msg.y, msg.w, msg.h);
            break;
        }
        case Tag::Bye: {
            if (m_socket->bytesAvailable() < (qint64)sizeof(Bye)) return;
            Bye msg{};
            m_socket->read(reinterpret_cast<char *>(&msg), sizeof(msg));
            qInfo() << "[vncast/qtfb] client bye, reason=" << msg.reason;
            m_socket->disconnectFromServer();
            return;
        }
        default:
            qWarning() << "[vncast/qtfb] unexpected tag" << h.tag << "— closing";
            m_socket->disconnectFromServer();
            return;
        }
    }
}

void Server::handleHello(const HelloC2S &msg) {
    // Server-side fps enforcement. Client requests its desired cap in
    // HELLO; 0 = unlimited.
    m_fps_cap       = msg.requested_fps;
    m_min_period_ms = (m_fps_cap == 0) ? 0 : (1000 / m_fps_cap);
    m_last_emit_ms  = 0;

    HelloAckS2C ack{};
    ack.header.magic = MAGIC;
    ack.header.tag   = static_cast<uint32_t>(Tag::HelloAckS2C);
    ack.accepted = (msg.client_version == VERSION) ? 1 : 0;
    ack.w        = m_w;
    ack.h        = m_h;
    ack.stride   = m_stride;
    ack.bpp      = m_bpp;
    ack.format   = m_format;
    QByteArray name = m_shmName.toLatin1();
    ack.shm_name_len = qMin<int>(name.size(), sizeof(ack.shm_name));
    std::memset(ack.shm_name, 0, sizeof(ack.shm_name));
    std::memcpy(ack.shm_name, name.constData(), ack.shm_name_len);
    m_socket->write(reinterpret_cast<const char *>(&ack), sizeof(ack));
    qInfo().noquote() << "[vncast/qtfb] hello: client_v=" << msg.client_version
                      << "accepted=" << ack.accepted
                      << "shm=" << m_shmName;
    if (!ack.accepted) m_socket->disconnectFromServer();
}

}  // namespace vncast::qtfb
