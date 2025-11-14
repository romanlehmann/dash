#pragma once

#include <QObject>
#include <QList>
#include <QWidget>

#include "plugins/vehicle_plugin.hpp"
#include "app/widgets/vehicle.hpp"
#include "canbus/socketcanbus.hpp"

class Bike : public QObject, VehiclePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID VehiclePlugin_iid FILE "bike.json")
    Q_INTERFACES(VehiclePlugin)

public:
    Bike() = default;
    ~Bike() override;

    QList<QWidget *> widgets() override;
    bool init(ICANBus *canbus) override;

private:
    QWidget *bike_tab = nullptr;
};
