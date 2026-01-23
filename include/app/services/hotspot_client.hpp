#pragma once

#include <QObject>
#include <QString>

class HotspotClient : public QObject
{
    Q_OBJECT

public:
    struct Status {
        bool ok = false;
        QString systemdState;
        QString hostapdState;
        QString error;

        bool isActive() const { return systemdState == "active"; }
    };

    explicit HotspotClient(QObject *parent = nullptr);

    Status status();
    Status start();
    Status stop();
    Status restart();

    void setSocketPath(const QString &path);
    QString socketPath() const;

private:
    Status sendCommand(const QString &command);

    QString socketPath_;
    int timeoutMs_ = 3000;
};
