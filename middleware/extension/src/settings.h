#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QJSEngine>
#include <QVariantMap>

// Persisted vncast preferences. Singleton exposed to QML as
// `Settings` under net.example.Vncast 1.0. Backed by ~/.config/vncast.json
// so we don't co-tenant AppLoad's settings store.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString host         READ host         WRITE setHost         NOTIFY changed)
    Q_PROPERTY(int     port         READ port         WRITE setPort         NOTIFY changed)
    Q_PROPERTY(int     fps          READ fps          WRITE setFps          NOTIFY changed)
    Q_PROPERTY(QString waveform     READ waveform     WRITE setWaveform     NOTIFY changed)
    Q_PROPERTY(QString orientation  READ orientation  WRITE setOrientation  NOTIFY changed)
    Q_PROPERTY(QString encoding     READ encoding     WRITE setEncoding     NOTIFY changed)
    Q_PROPERTY(bool    grayscale    READ grayscale    WRITE setGrayscale    NOTIFY changed)
    // Separate from grayscale: controls whether vnsee advertises the
    // rcast-host MONO1 (1-bit packed) pseudo-encoding. The two often
    // pair (B&W content tagged + delivered as 1-bit) but a user might
    // want grayscale source without 1-bit encoding (preserve the few
    // gray levels) or 1-bit encoding without grayscale (color content
    // dithered to mono).
    Q_PROPERTY(bool    mono1        READ mono1        WRITE setMono1        NOTIFY changed)
    Q_PROPERTY(int     compressLevel READ compressLevel WRITE setCompressLevel NOTIFY changed)
public:
    explicit Settings(QObject *parent = nullptr);

    QString host() const         { return m_host; }
    int     port() const         { return m_port; }
    int     fps() const          { return m_fps; }
    QString waveform() const     { return m_waveform; }
    QString orientation() const  { return m_orientation; }
    QString encoding() const     { return m_encoding; }
    bool    grayscale() const    { return m_grayscale; }
    bool    mono1() const        { return m_mono1; }
    int     compressLevel() const { return m_compressLevel; }

    void setHost(const QString &v);
    void setPort(int v);
    void setFps(int v);
    void setWaveform(const QString &v);
    void setOrientation(const QString &v);
    void setEncoding(const QString &v);
    void setGrayscale(bool v);
    void setMono1(bool v);
    void setCompressLevel(int v);

    Q_INVOKABLE void save();
    Q_INVOKABLE QVariantMap asMap() const;

    static QObject *qmlSingleton(QQmlEngine *, QJSEngine *) {
        static Settings *inst = new Settings();
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }

signals:
    void changed();

private:
    void load();
    QString configPath() const;

    // Defaults match the most-common rMPP/USB-tether scenario.
    QString m_host        = QStringLiteral("10.11.99.2");
    int     m_port        = 5900;
    int     m_fps         = 0;     // 0 = "free" (no cap)
    // Vestigial — Pen mode looked terrible on full-screen content; we
    // only ever use A2 now. Kept in settings.json for back-compat with
    // existing configs but UI no longer exposes it.
    QString m_waveform    = QStringLiteral("A2");
    QString m_orientation = QStringLiteral("auto");
    QString m_encoding    = QStringLiteral("COPYRECT");
    // Force pixels to luma in vnsee before they hit shm. Combined with a
    // GC16/GLR16 panel waveform this still yields A2-ish appearance because
    // the source content has no color to drive ghosting/transition cells.
    bool    m_grayscale   = true;
    // Default false — only useful when paired with a server that knows
    // the rcastmono1 pseudo-encoding (rcast-host). Standard VNC servers
    // ignore unknown encoding IDs harmlessly.
    bool    m_mono1       = false;
    // Hidden — no UI knob. Used for benchmarking. Edit
    // /home/root/.config/vncast.json to override (0..9, default 0).
    int     m_compressLevel = 0;
};
