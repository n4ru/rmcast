#include "settings.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

Settings::Settings(QObject *parent) : QObject(parent) { load(); }

QString Settings::configPath() const {
    // ~/.config/vncast.json — XDG-ish. xochitl runs as root on rMPP so HOME=/home/root.
    const QString dir = qgetenv("HOME").isEmpty()
        ? QStringLiteral("/home/root/.config")
        : QString::fromLocal8Bit(qgetenv("HOME")) + QStringLiteral("/.config");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/vncast.json");
}

void Settings::load() {
    QFile f(configPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const auto o = doc.object();
    if (o.contains("host"))        m_host        = o.value("host").toString(m_host);
    if (o.contains("port"))        m_port        = o.value("port").toInt(m_port);
    if (o.contains("fps"))         m_fps         = o.value("fps").toInt(m_fps);
    if (o.contains("waveform"))    m_waveform    = o.value("waveform").toString(m_waveform);
    if (o.contains("orientation")) m_orientation = o.value("orientation").toString(m_orientation);
    if (o.contains("encoding"))    m_encoding    = o.value("encoding").toString(m_encoding);
    if (o.contains("grayscale"))   m_grayscale   = o.value("grayscale").toBool(m_grayscale);
    if (o.contains("mono1"))       m_mono1       = o.value("mono1").toBool(m_mono1);
    if (o.contains("lowLatency"))  m_lowLatency  = o.value("lowLatency").toBool(m_lowLatency);
    if (o.contains("compressLevel")) m_compressLevel = o.value("compressLevel").toInt(m_compressLevel);
    qInfo() << "[vncast/settings] loaded" << configPath();
}

void Settings::save() {
    QJsonObject o;
    o["host"]        = m_host;
    o["port"]        = m_port;
    o["fps"]         = m_fps;
    o["waveform"]    = m_waveform;
    o["orientation"] = m_orientation;
    o["encoding"]    = m_encoding;
    o["grayscale"]   = m_grayscale;
    o["mono1"]       = m_mono1;
    o["lowLatency"]  = m_lowLatency;
    o["compressLevel"] = m_compressLevel;
    QFile f(configPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[vncast/settings] cannot write" << configPath();
        return;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    qInfo() << "[vncast/settings] saved" << configPath();
}

QVariantMap Settings::asMap() const {
    return {
        {"host",        m_host},
        {"port",        m_port},
        {"fps",         m_fps},
        {"waveform",    m_waveform},
        {"orientation", m_orientation},
        {"encoding",    m_encoding},
        {"grayscale",   m_grayscale},
        {"mono1",       m_mono1},
        {"lowLatency",  m_lowLatency},
        {"compressLevel", m_compressLevel},
    };
}

void Settings::setHost(const QString &v)        { if (v == m_host) return;        m_host = v;        emit changed(); }
void Settings::setPort(int v)                   { if (v == m_port) return;        m_port = v;        emit changed(); }
void Settings::setFps(int v)                    { if (v == m_fps) return;         m_fps = v;         emit changed(); }
void Settings::setWaveform(const QString &v)    { if (v == m_waveform) return;    m_waveform = v;    emit changed(); }
void Settings::setOrientation(const QString &v) { if (v == m_orientation) return; m_orientation = v; emit changed(); }
void Settings::setEncoding(const QString &v)    { if (v == m_encoding) return;    m_encoding = v;    emit changed(); }
void Settings::setGrayscale(bool v)             { if (v == m_grayscale) return;   m_grayscale = v;   emit changed(); }
void Settings::setMono1(bool v)                 { if (v == m_mono1) return;       m_mono1 = v;       emit changed(); }
void Settings::setLowLatency(bool v)            { if (v == m_lowLatency) return;  m_lowLatency = v;  emit changed(); }
void Settings::setCompressLevel(int v)          { if (v == m_compressLevel) return; m_compressLevel = qBound(0, v, 9); emit changed(); }
