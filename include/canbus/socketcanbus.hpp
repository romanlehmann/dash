#pragma once

#include <QObject>
#include <QCanBus>
#include <QCanBusDevice>
#include <QVector>
#include <QByteArray>
#include <QVariant>
#include <QList>
#include <QTimer>

#include <map>
#include <vector>
#include <functional>

#include "DashLog.hpp"
#include "canbus/ICANBus.hpp"
#include "app/config.hpp"

class SocketCANBus : public ICANBus
{
    Q_OBJECT
public:
    explicit SocketCANBus(QString canInterface = QStringLiteral("can0"));
    ~SocketCANBus() override;

    static SocketCANBus* get_instance();

    // ICANBus API
    void registerFrameHandler(int id, std::function<void(QByteArray)> callback) override;
    bool writeFrame(QCanBusFrame frame) override;

private:
    bool socketCANAvailable = false;
    QCanBusDevice* bus = nullptr;

    std::map<int, std::vector<std::function<void(QByteArray)>>> callbacks;
    QList<QCanBusDevice::Filter> filterList;

    // --- Simulation state ---
    bool    simActive   = false;
    QTimer* simTimer    = nullptr;
    int     simDir      = 1;     // 1 rauf, -1 runter (Speed)
    int     simSpeed    = 0;     // km/h
    int     simRPM      = 800;   // rpm
    int     simTick     = 0;     // langsame Trends (Temp)
    int     simCoolantC = 25;    // °C
    int     simLoadPct  = 20;    // %

    // Helpers
    QVector<QCanBusFrame> readAllFrames(int numFrames);
    void applyFiltersIfConnected();

    // Simulation intern
    void startSimulation();
    void stopSimulation();
    void emitSimulatedFrames();

private slots:
    void framesAvailable();
};
