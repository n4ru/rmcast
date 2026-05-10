#pragma once
#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QJSEngine>

// Runtime device + framebuffer detection.
//
// rM1, rM2:    1404x1872 mono16, /dev/fb0 works → no qtfb needed.
// rMPP:        1620x2160 RGBA,   xochitl owns DRM master → qtfb required.
// rMPPM:       Gallery (smaller, dims unverified) → same as rMPP.
//
// Detection strategy, in order of trust:
//   1. EPFramebuffer constructor hook (filled in once xochitl runs) —
//      authoritative w/h/bpl/format from xochitl itself. (TBD: hook the
//      same symbol qt-resource-rebuilder/framebuffer-spy hooks.)
//   2. DRM ioctl drmModeGetResources/Connector — works on rMPP/rMPPM.
//   3. Device-class lookup table by /etc/version + /sys/devices/soc0/machine.
//
// For now this is a stub that returns the rMPP defaults. Replaced once we
// hook EPFramebuffer in qt-resource-rebuilder-style.
class DeviceInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deviceClass READ deviceClass CONSTANT)
    Q_PROPERTY(int     fbWidth     READ fbWidth     CONSTANT)
    Q_PROPERTY(int     fbHeight    READ fbHeight    CONSTANT)
    Q_PROPERTY(int     fbStride    READ fbStride    CONSTANT)
    Q_PROPERTY(int     fbBpp       READ fbBpp       CONSTANT)
    Q_PROPERTY(bool    needsQtfb   READ needsQtfb   CONSTANT)
public:
    explicit DeviceInfo(QObject *parent = nullptr);

    QString deviceClass() const { return m_class; }
    int     fbWidth()     const { return m_w; }
    int     fbHeight()    const { return m_h; }
    int     fbStride()    const { return m_bpl; }
    int     fbBpp()       const { return m_bpp; }
    bool    needsQtfb()   const { return m_needsQtfb; }

    static QObject *qmlSingleton(QQmlEngine *, QJSEngine *) {
        static DeviceInfo *inst = new DeviceInfo();
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }

private:
    void detect();

    QString m_class      = QStringLiteral("unknown");
    int     m_w          = 1620;
    int     m_h          = 2160;
    int     m_bpl        = 1620 * 4;
    int     m_bpp        = 32;
    bool    m_needsQtfb  = true;
};
