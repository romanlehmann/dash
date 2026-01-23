#pragma once

#include <QString>

struct HostapdConfig {
    QString ssid;
    QString passphrase;
    QString encryption;

    bool isOpen() const;
    bool hasCredentials() const;
};

class HostapdConfigReader {
public:
    static HostapdConfig load(const QString &path, QString *error = nullptr);
};

QString escapeWifiQrField(const QString &value);
