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
public:
    explicit Settings(QObject *parent = nullptr);

    QString host() const         { return m_host; }
    int     port() const         { return m_port; }
    int     fps() const          { return m_fps; }
    QString waveform() const     { return m_waveform; }
    QString orientation() const  { return m_orientation; }
    QString encoding() const     { return m_encoding; }

    void setHost(const QString &v);
    void setPort(int v);
    void setFps(int v);
    void setWaveform(const QString &v);
    void setOrientation(const QString &v);
    void setEncoding(const QString &v);

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
    int     m_fps         = 8;
    QString m_waveform    = QStringLiteral("A2");
    QString m_orientation = QStringLiteral("auto");
    QString m_encoding    = QStringLiteral("COPYRECT");
};
