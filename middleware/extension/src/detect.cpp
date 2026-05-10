#include "detect.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace {
struct Entry { const char *machine; const char *cls; int w, h, bpl, bpp; bool needsQtfb; };
constexpr Entry kKnown[] = {
    // machine string                cls         w     h     bpl       bpp  needsQtfb
    { "ferrari",                     "rMPP",     1620, 2160, 1620 * 4, 32,  true  },
    { "reMarkable Paper Pro",        "rMPP",     1620, 2160, 1620 * 4, 32,  true  },
    { "reMarkable 2.0",              "rM2",      1404, 1872, 1404 * 2, 16,  false },
    { "reMarkable 1.0",              "rM1",      1404, 1872, 1404 * 2, 16,  false },
    // rMPPM placeholder — verify dims on hardware before trusting.
    { "reMarkable Paper Pro Move",   "rMPPM",    1620, 2160, 1620 * 4, 32,  true  },
};
}

DeviceInfo::DeviceInfo(QObject *parent) : QObject(parent) { detect(); }

void DeviceInfo::detect() {
    // Fallback path 3: device-class lookup. Prefer /sys/devices/soc0/machine
    // when present; otherwise read /etc/version (only its first line is the
    // device name on rM stock images).
    QString machine;
    QFile m("/sys/devices/soc0/machine");
    if (m.open(QIODevice::ReadOnly)) {
        machine = QString::fromLocal8Bit(m.readLine()).trimmed();
    }
    if (machine.isEmpty()) {
        QFile v("/etc/device.conf");
        if (v.open(QIODevice::ReadOnly)) {
            machine = QString::fromLocal8Bit(v.readLine()).trimmed();
        }
    }

    for (const auto &e : kKnown) {
        if (machine.contains(QString::fromLatin1(e.machine), Qt::CaseInsensitive)) {
            m_class     = QString::fromLatin1(e.cls);
            m_w         = e.w;
            m_h         = e.h;
            m_bpl       = e.bpl;
            m_bpp       = e.bpp;
            m_needsQtfb = e.needsQtfb;
            qInfo().noquote() << "[vncast/detect] machine=" << machine
                              << "→ class=" << m_class
                              << "fb=" << m_w << "x" << m_h
                              << "bpp=" << m_bpp
                              << "needsQtfb=" << m_needsQtfb;
            return;
        }
    }

    // Fallback: rMPP defaults already set in header. If we end up here on
    // rM2 we'll allocate too much shm — better than the alternative.
    qWarning().noquote() << "[vncast/detect] unknown machine='" << machine
                         << "' — defaulting to rMPP geometry";
}
