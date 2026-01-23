#include "app/services/hotspot_client.hpp"

#include <QByteArray>
#include <QLocalSocket>
#include <QStringList>
#include <QtGlobal>

namespace {
const char *kDefaultSocketPath = "/run/opendash/hotspotd.sock";
const char *kSocketEnvVar = "OPENDASH_HOTSPOTD_SOCKET";
}

HotspotClient::HotspotClient(QObject *parent)
    : QObject(parent)
    , socketPath_(qEnvironmentVariableIsEmpty(kSocketEnvVar)
                      ? QString::fromLatin1(kDefaultSocketPath)
                      : QString::fromLocal8Bit(qgetenv(kSocketEnvVar)))
{
}

HotspotClient::Status HotspotClient::status()
{
    return this->sendCommand("status");
}

HotspotClient::Status HotspotClient::start()
{
    return this->sendCommand("start");
}

HotspotClient::Status HotspotClient::stop()
{
    return this->sendCommand("stop");
}

HotspotClient::Status HotspotClient::restart()
{
    return this->sendCommand("restart");
}

void HotspotClient::setSocketPath(const QString &path)
{
    this->socketPath_ = path;
}

QString HotspotClient::socketPath() const
{
    return this->socketPath_;
}

HotspotClient::Status HotspotClient::sendCommand(const QString &command)
{
    Status status;
    QLocalSocket socket;
    socket.connectToServer(this->socketPath_);
    if (!socket.waitForConnected(this->timeoutMs_)) {
        status.error = socket.errorString();
        return status;
    }

    QByteArray payload = command.toUtf8();
    payload.append('\n');
    if (socket.write(payload) == -1 || !socket.waitForBytesWritten(this->timeoutMs_)) {
        status.error = socket.errorString();
        return status;
    }

    if (!socket.waitForReadyRead(this->timeoutMs_)) {
        status.error = "No response from hotspot service";
        return status;
    }

    QByteArray response = socket.readAll();
    while (socket.waitForReadyRead(10))
        response.append(socket.readAll());

    const QString line = QString::fromUtf8(response).trimmed();
    if (line.startsWith("ok")) {
        status.ok = true;
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        for (int i = 1; i < parts.size(); ++i) {
            const QString &token = parts.at(i);
            const int equalsIndex = token.indexOf('=');
            if (equalsIndex <= 0)
                continue;

            const QString key = token.left(equalsIndex);
            const QString value = token.mid(equalsIndex + 1);
            if (key == "state")
                status.systemdState = value;
            else if (key == "hostapd_state")
                status.hostapdState = value;
        }

        if (status.systemdState.isEmpty())
            status.systemdState = "unknown";
        if (status.hostapdState.isEmpty())
            status.hostapdState = "unknown";
    } else if (line.startsWith("error")) {
        status.ok = false;
        status.error = line.mid(QString("error").size()).trimmed();
    } else {
        status.ok = false;
        status.error = "Invalid hotspot service response";
    }

    return status;
}
