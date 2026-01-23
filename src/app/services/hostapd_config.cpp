#include "app/services/hostapd_config.hpp"

#include <QFile>
#include <QTextStream>

bool HostapdConfig::isOpen() const
{
    return this->encryption.compare("nopass", Qt::CaseInsensitive) == 0;
}

bool HostapdConfig::hasCredentials() const
{
    return !this->ssid.isEmpty() && (this->isOpen() || !this->passphrase.isEmpty());
}

HostapdConfig HostapdConfigReader::load(const QString &path, QString *error)
{
    HostapdConfig config;
    QString wpa;
    QString wpaKeyMgmt;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return config;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        int commentIndex = line.indexOf('#');
        if (commentIndex >= 0)
            line = line.left(commentIndex).trimmed();

        int equalsIndex = line.indexOf('=');
        if (equalsIndex <= 0)
            continue;

        const QString key = line.left(equalsIndex).trimmed();
        const QString value = line.mid(equalsIndex + 1).trimmed();

        if (key == "ssid")
            config.ssid = value;
        else if (key == "wpa_passphrase")
            config.passphrase = value;
        else if (key == "wpa")
            wpa = value;
        else if (key == "wpa_key_mgmt")
            wpaKeyMgmt = value;
    }

    if (wpa == "0") {
        config.encryption = "nopass";
    } else if (!wpaKeyMgmt.isEmpty() && wpaKeyMgmt.contains("WPA-PSK", Qt::CaseInsensitive)) {
        config.encryption = "WPA";
    } else {
        config.encryption = "WPA";
    }

    return config;
}

QString escapeWifiQrField(const QString &value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace(";", "\\;");
    escaped.replace(",", "\\,");
    escaped.replace(":", "\\:");
    return escaped;
}
