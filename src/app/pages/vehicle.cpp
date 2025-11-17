#include <QPalette>
#include <QSerialPortInfo>

#include "app/config.hpp"
#include "app/pages/vehicle.hpp"
#include "app/window.hpp"
#include "obd/conversions.hpp"
#include "canbus/elm327.hpp"
#include "plugins/vehicle_plugin.hpp"



Gauge::Gauge(units_t units, QFont value_font, QFont unit_font, Gauge::Orientation orientation, int rate,
             std::vector<Command> cmds, int precision, obd_decoder_t decoder, QWidget *parent)
: QWidget(parent)
{
    Config *config = Config::get_instance();
    ICANBus *bus;
    switch(config->get_vehicle_can_bus()){
        //ELM327 USB
        case ICANBus::VehicleBusType::ELM327USB:
            bus = (ICANBus *)elm327::get_usb_instance();
            break;
        //ELM327 Bluetooth
        case ICANBus::VehicleBusType::ELM327BT:
            bus = (ICANBus *)elm327::get_bt_instance();
            break;
        //SocketCAN
        case ICANBus::VehicleBusType::SocketCAN:
        default:
            bus = (ICANBus *)SocketCANBus::get_instance();
            break;
    }

    using namespace std::placeholders;
    std::function<void(QByteArray)> callback = std::bind(&Gauge::can_callback, this, std::placeholders::_1);

    bus->registerFrameHandler(cmds[0].frame.frameId()+0x9, callback);
    DASH_LOG(info)<<"[Gauges] Registered frame handler for id "<<(cmds[0].frame.frameId()+0x9);

    this->si = config->get_si_units();

    this->rate = rate;
    this->precision = precision;

    this->cmds = cmds;
    this->decoder = decoder;

    QBoxLayout *layout;
    if (orientation == BOTTOM)
        layout = new QVBoxLayout(this);
    else
        layout = new QHBoxLayout(this);

    value_label = new QLabel(this->null_value(), this);
    value_label->setFont(value_font);
    value_label->setAlignment(Qt::AlignCenter);

    QLabel *unit_label = new QLabel(this->si ? units.second : units.first, this);
    unit_label->setFont(unit_font);
    unit_label->setAlignment(Qt::AlignCenter);

    this->timer = new QTimer(this);
    connect(this->timer, &QTimer::timeout, [this, bus, cmds]() {
        for (auto cmd : cmds) {
            bus->writeFrame(cmd.frame);
        }
    });

    connect(config, &Config::si_units_changed, [this, units, unit_label](bool si) {
        this->si = si;
        unit_label->setText(this->si ? units.second : units.first);
        value_label->setText(this->null_value());
    });

    layout->addStretch(6);
    layout->addWidget(value_label);
    layout->addStretch(1);
    layout->addWidget(unit_label);
    layout->addStretch(4);
}

void Gauge::can_callback(QByteArray payload){
    Response resp = Response(payload);
    for(auto cmd : cmds){
        if(cmd.frame.payload().at(2) == resp.PID){
            value_label->setText(this->format_value(this->decoder(cmd.decoder(resp), this->si)));
        }
    }
}

QString Gauge::format_value(double value)
{
    if (this->precision == 0)
        return QString::number((int)value);
    else
        return QString::number(value, 'f', this->precision);
}

QString Gauge::null_value()
{
    QString null_str = "-";
    if (this->precision > 0)
        null_str += ".-";
    else
        null_str += '-';

    return null_str;
}

VehiclePage::VehiclePage(Arbiter &arbiter, QWidget *parent)
    : QTabWidget(parent)
    , Page(arbiter, "Vehicle", "motocycle", true, this)
{
    // Accept touch events for gesture recognition
    this->setAttribute(Qt::WA_AcceptTouchEvents, true);
    // Register swipe gesture on this tab widget
    this->grabGesture(Qt::SwipeGesture);

}

void VehiclePage::init()
{
    auto *dataTab = new DataTab(this->arbiter, this);
    dataTab->installEventFilter(this);
    this->addTab(dataTab, "Data");    
    this->config = Config::get_instance();
 
    for (auto device : QCanBus::instance()->availableDevices("socketcan"))
        this->can_devices.append(device.name());

    for (auto port : QSerialPortInfo::availablePorts())
        this->serial_devices.append(port.systemLocation());

    connect(&this->arbiter.system().bluetooth, &Bluetooth::init, [this]{
        for (auto device: this->arbiter.system().bluetooth.get_devices())
        {
            if(device->isPaired()){
                this->paired_bt_devices.insert(device->name(), device->address());
            }
        }
    });

    this->get_plugins();
    this->active_plugin = new QPluginLoader(this);
    Dialog *dialog = new Dialog(this->arbiter, true, this->window());
    dialog->set_body(this->dialog_body());
    QPushButton *load_button = new QPushButton("load");
    connect(load_button, &QPushButton::clicked, [this]() { this->load_plugin(); });
    dialog->set_button(load_button);

    QPushButton *settings_button = new QPushButton(this);
    settings_button->setFlat(true);
    this->arbiter.forge().iconize("settings", settings_button, 24);
    connect(settings_button, &QPushButton::clicked, [dialog]() { dialog->open(); });
    this->setCornerWidget(settings_button);

    this->load_plugin();
}

QWidget *VehiclePage::dialog_body()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    QStringList plugins = this->plugins.keys();
    this->plugin_selector = new Selector(plugins, this->config->get_vehicle_plugin(), this->arbiter.forge().font(14), this->arbiter, widget, "unloader");

    layout->addWidget(this->si_units_row_widget(), 1);
    layout->addWidget(Session::Forge::br(), 1);
    layout->addWidget(this->can_bus_toggle_row(), 1);

    QStringList devices;
    switch(config->get_vehicle_can_bus()){
        //ELM327 USB
        case ICANBus::VehicleBusType::ELM327USB:
            devices = this->serial_devices;
            break;
        //ELM327 Bluetooth
        case ICANBus::VehicleBusType::ELM327BT:
            
            break;
        //SocketCAN
        case ICANBus::VehicleBusType::SocketCAN:
        default:
            devices = this->can_devices;
            break;
    }
   
    Selector *interface_selector = new Selector(devices, this->config->get_vehicle_interface(), this->arbiter.forge().font(14), this->arbiter, widget, "disabled");
    interface_selector->setVisible((this->can_devices.size() > 0) || (this->serial_devices.size() > 0) || (this->paired_bt_devices.size() > 0));
    connect(interface_selector, &Selector::item_changed, [this](QString item){
        if(this->config->get_vehicle_can_bus()==ICANBus::VehicleBusType::ELM327BT && item != QString("disabled"))
        {
            this->config->set_vehicle_interface(this->paired_bt_devices[item]);
        }
        else
        {
            this->config->set_vehicle_interface(item);
        }
    });
    connect(this->config, &Config::vehicle_can_bus_changed, [this, interface_selector](int state){
        switch(state){
            //ELM327 USB
            case ICANBus::VehicleBusType::ELM327USB:
                interface_selector->set_options(this->serial_devices);
                break;
            //ELM327 Bluetooth
            case ICANBus::VehicleBusType::ELM327BT:
                interface_selector->set_options(this->paired_bt_devices.keys());
                break;
            //SocketCAN
            case ICANBus::VehicleBusType::SocketCAN:
            default:
                interface_selector->set_options(this->can_devices);
                break;
        }
    });
    connect(&this->arbiter.system().bluetooth, &Bluetooth::init, [this, interface_selector]{
        interface_selector->setVisible((this->can_devices.size() > 0) || (this->serial_devices.size() > 0) || (this->paired_bt_devices.size() > 0));
        if(this->config->get_vehicle_can_bus()==ICANBus::VehicleBusType::ELM327BT){
            QString current = this->config->get_vehicle_interface();
            interface_selector->set_options(this->paired_bt_devices.keys());
            if(current != "disabled")
                interface_selector->set_current(this->paired_bt_devices.key(current));

        }
    });
    layout->addWidget(interface_selector, 1);

    layout->addWidget(Session::Forge::br(), 1);
    layout->addWidget(this->plugin_selector, 1);

    return widget;
}

QWidget *VehiclePage::can_bus_toggle_row()
{
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);

    QLabel *label = new QLabel("Interface", widget);
    layout->addWidget(label, 1);

    QGroupBox *group = new QGroupBox();
    QVBoxLayout *group_layout = new QVBoxLayout(group);

    ICANBus::VehicleBusType can_bus_selected = this->config->get_vehicle_can_bus();
    QRadioButton *socketcan_button = new QRadioButton("SocketCAN", group);
    socketcan_button->setChecked(can_bus_selected==ICANBus::VehicleBusType::SocketCAN);
    socketcan_button->setEnabled(this->can_devices.size() > 0);
    connect(socketcan_button, &QRadioButton::clicked, [config = this->config]{
        config->set_vehicle_can_bus(ICANBus::VehicleBusType::SocketCAN);
    });
    group_layout->addWidget(socketcan_button);

    QRadioButton *elm_usb_button = new QRadioButton("ELM327 (USB)", group);
    elm_usb_button->setChecked(can_bus_selected==ICANBus::VehicleBusType::ELM327USB);
    elm_usb_button->setEnabled(this->serial_devices.size() > 0);
    connect(elm_usb_button, &QRadioButton::clicked, [config = this->config]{
        config->set_vehicle_can_bus(ICANBus::VehicleBusType::ELM327USB);
    });
    group_layout->addWidget(elm_usb_button);

    QRadioButton *elm_bt_button = new QRadioButton("ELM327 (Bluetooth)", group);
    elm_bt_button->setChecked(can_bus_selected==ICANBus::VehicleBusType::ELM327BT);
    elm_bt_button->setEnabled(false);
    connect(elm_bt_button, &QRadioButton::clicked, [config = this->config]{
        config->set_vehicle_can_bus(ICANBus::VehicleBusType::ELM327BT);
    });
    connect(&this->arbiter.system().bluetooth, &Bluetooth::init, [this, elm_bt_button]{
            elm_bt_button->setEnabled(this->paired_bt_devices.size() > 0);
    });
    group_layout->addWidget(elm_bt_button);

    layout->addWidget(group, 1, Qt::AlignHCenter);

    return widget;
}

QWidget *VehiclePage::si_units_row_widget()
{
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);

    QLabel *label = new QLabel("SI Units", widget);
    layout->addWidget(label, 1);

    Switch *toggle = new Switch(widget);
    toggle->scale(this->arbiter.layout().scale);
    toggle->setChecked(this->config->get_si_units());
    connect(toggle, &Switch::stateChanged, [config = this->config](bool state) { config->set_si_units(state); });
    layout->addWidget(toggle, 1, Qt::AlignHCenter);

    return widget;
}

void VehiclePage::get_plugins()
{
    for (const QFileInfo &plugin : Session::plugin_dir("vehicle").entryInfoList(QDir::Files)) {
        if (QLibrary::isLibrary(plugin.absoluteFilePath()))
            this->plugins[Session::fmt_plugin(plugin.baseName())] = plugin;
    }
}

void VehiclePage::load_plugin()
{
    if (this->active_plugin->isLoaded())
        this->active_plugin->unload();

    QString key = this->plugin_selector->get_current();
    if (!key.isNull()) {
        this->active_plugin->setFileName(this->plugins[key].absoluteFilePath());

        if (VehiclePlugin *plugin = qobject_cast<VehiclePlugin *>(this->active_plugin->instance())) {
            plugin->dashize(&this->arbiter);
            switch(config->get_vehicle_can_bus()){
                //ELM327 USB
                case ICANBus::VehicleBusType::ELM327USB:
                    plugin->init((ICANBus *)elm327::get_usb_instance());
                    break;
                //ELM327 Bluetooth
                case ICANBus::VehicleBusType::ELM327BT:
                    plugin->init((ICANBus *)elm327::get_bt_instance());
                    break;
                //SocketCAN
                case ICANBus::VehicleBusType::SocketCAN:
                default:
                    plugin->init((ICANBus *)SocketCANBus::get_instance());
                    break;
            }
            for (QWidget *tab : plugin->widgets()) {
                tab->installEventFilter(this);           // swipe detection
                this->addTab(tab, tab->objectName());
            }
        }
    }
    this->config->set_vehicle_plugin(key);
}


DataTab::DataTab(Arbiter &arbiter, QWidget *parent)
    : QWidget(parent)
    , arbiter(arbiter)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    QWidget *vehicleData = this->vehicle_data_widget();
    layout->addWidget(vehicleData);

    // Socket anlegen
    this->vehicleSocket = new QLocalSocket(this);

    // Reconnect-Timer anlegen
    this->vehicleReconnectTimer = new QTimer(this);
    this->vehicleReconnectTimer->setInterval(2000);     // alle 2 Sekunden
    this->vehicleReconnectTimer->setSingleShot(false);

    // Wenn der Timer feuert und wir sind nicht verbunden -> neu versuchen
    connect(this->vehicleReconnectTimer, &QTimer::timeout, this, [this]() {
        if (!this->vehicleSocket)
            return;

        if (this->vehicleSocket->state()
                == QLocalSocket::UnconnectedState) {
            this->vehicleSocket->connectToServer(
                QStringLiteral("vehicle_data"));
        }
        // Wenn wir schon Connected/Connecting sind, macht der Timer nichts
    });

    // Wenn Daten ankommen
    connect(this->vehicleSocket, &QLocalSocket::readyRead,
            this, [this]() {
        this->vehicleBuffer.append(this->vehicleSocket->readAll());

        int index = -1;
        while ((index = this->vehicleBuffer.indexOf('\n')) != -1) {
            QByteArray line = this->vehicleBuffer.left(index);
            this->vehicleBuffer.remove(0, index + 1);

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(line, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject())
                continue;

            QJsonObject obj = doc.object();

            double speedKmh = obj.value("kmh").toDouble();
            double rpm      = obj.value("rpm").toDouble();
            int    gear     = obj.value("gear").toInt();
            double temp     = obj.value("temperature").toDouble();
            double odometer = obj.value("odometer").toDouble();
            double fuel     = obj.value("fuelLevel").toDouble();

            if (this->speedLabel)
                this->speedLabel->setText(
                    QString::number(speedKmh, 'f', 0));
            if (this->rpmLabel)
                this->rpmLabel->setText(
                    QString::number(rpm, 'f', 0));
            if (this->gearLabel) 
                this->gearLabel->setText(gear == 0 ? "N" : QString::number(gear));
            if (this->coolantLabel)
                this->coolantLabel->setText(
                    QString::number(temp, 'f', 1));
            if (this->odoLabel)
                this->odoLabel->setText(
                    QString::number(odometer, 'f', 1));
            if (this->fuelLabel)
                this->fuelLabel->setText(
                    QString::number(fuel * 100.0, 'f', 0) + "%");
        }
    });

    // Verbunden -> Reconnect-Timer stoppen
    connect(this->vehicleSocket, &QLocalSocket::connected,
            this, [this]() {
        if (this->vehicleReconnectTimer)
            this->vehicleReconnectTimer->stop();
    });

    // Getrennt -> Reconnect-Timer starten
    connect(this->vehicleSocket, &QLocalSocket::disconnected,
            this, [this]() {
        if (this->vehicleReconnectTimer)
            this->vehicleReconnectTimer->start();
    });

    // Fehler -> Reconnect-Timer starten
    connect(this->vehicleSocket,
            qOverload<QLocalSocket::LocalSocketError>(
                &QLocalSocket::errorOccurred),
            this, [this](QLocalSocket::LocalSocketError) {
        if (this->vehicleReconnectTimer)
            this->vehicleReconnectTimer->start();
    });

    // Erster Verbindungsversuch
    this->vehicleSocket->connectToServer(
        QStringLiteral("vehicle_data"));
    // Wenn der Server noch nicht läuft, gibt es einen Fehler und
    // der Fehler-Handler startet den Reconnect-Timer
}

QWidget *DataTab::speedo_tach_widget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch(3);

    QFont speed_value_font(this->arbiter.forge().font(36, true));

    QFont speed_unit_font(this->arbiter.forge().font(16));
    speed_unit_font.setWeight(QFont::Light);
    speed_unit_font.setItalic(true);

    Gauge *speed = new Gauge({"mph", "km/h"}, speed_value_font, speed_unit_font,
                             Gauge::BOTTOM, 100, {cmds.SPEED}, 0,
                             [](double x, bool si) { return si ? x : kph_to_mph(x); }, widget);
    layout->addWidget(speed);
    this->gauges.push_back(speed);

    layout->addStretch(2);

    QFont tach_value_font(this->arbiter.forge().font(24, true));

    QFont tach_unit_font(this->arbiter.forge().font(12));
    tach_unit_font.setWeight(QFont::Light);
    tach_unit_font.setItalic(true);

    Gauge *rpm = new Gauge({"x1000rpm", "x1000rpm"}, tach_value_font,
                           tach_unit_font, Gauge::BOTTOM, 100, {cmds.RPM}, 1,
                           [](double x, bool _) { return x / 1000.0; }, widget);
    layout->addWidget(rpm);
    this->gauges.push_back(rpm);

    layout->addStretch(1);

    return widget;
}

QWidget *DataTab::engine_data_widget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addStretch();
    layout->addWidget(this->coolant_temp_widget());
    layout->addStretch();
    layout->addWidget(Session::Forge::br());
    layout->addStretch();
    layout->addWidget(this->engine_load_widget());
    layout->addStretch();

    return widget;
}

QWidget *DataTab::coolant_temp_widget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QFont value_font(this->arbiter.forge().font(16, true));

    QFont unit_font(this->arbiter.forge().font(12));
    unit_font.setWeight(QFont::Light);
    unit_font.setItalic(true);

    Gauge *coolant_temp = new Gauge(
        {"°F", "°C"}, value_font, unit_font, Gauge::RIGHT, 5000,
        {cmds.COOLANT_TEMP}, 1, [](double x, bool si) { return si ? x : c_to_f(x); }, widget);
    layout->addWidget(coolant_temp);
    this->gauges.push_back(coolant_temp);

    QFont label_font(this->arbiter.forge().font(10));
    label_font.setWeight(QFont::Light);

    QLabel *coolant_temp_label = new QLabel("coolant", widget);
    coolant_temp_label->setFont(label_font);
    coolant_temp_label->setAlignment(Qt::AlignHCenter);
    layout->addWidget(coolant_temp_label);

    return widget;
}

QWidget *DataTab::engine_load_widget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QFont value_font(this->arbiter.forge().font(16, true));

    QFont unit_font(this->arbiter.forge().font(12));
    unit_font.setWeight(QFont::Light);
    unit_font.setItalic(true);

    Gauge *engine_load =
        new Gauge({"%", "%"}, value_font, unit_font, Gauge::RIGHT,
                  500, {cmds.LOAD}, 1, [](double x, bool _) { return x; }, widget);
    layout->addWidget(engine_load);
    this->gauges.push_back(engine_load);

    QFont label_font(this->arbiter.forge().font(10));
    label_font.setWeight(QFont::Light);

    QLabel *engine_load_label = new QLabel("load", widget);
    engine_load_label->setFont(label_font);
    engine_load_label->setAlignment(Qt::AlignHCenter);
    layout->addWidget(engine_load_label);
    return widget;
}

QWidget *DataTab::vehicle_data_widget()
{
    QWidget *widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setHorizontalSpacing(20);
    layout->setVerticalSpacing(8);

    QFont valueFont = this->arbiter.forge().font(24, true);
    QFont labelFont = this->arbiter.forge().font(10);
    labelFont.setWeight(QFont::Light);

    int row = 0;

    auto makeRow = [&](const QString &labelText,
                       QLabel **valueLabelPtr,
                       const QString &unitText) {
        QLabel *label = new QLabel(labelText, widget);
        label->setFont(labelFont);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QLabel *value = new QLabel("-", widget);
        value->setFont(valueFont);
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *unit = nullptr;
        if (!unitText.isEmpty()) {
            unit = new QLabel(unitText, widget);
            unit->setFont(labelFont);
            unit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }

        layout->addWidget(label, row, 0);
        layout->addWidget(value, row, 1);
        if (unit)
            layout->addWidget(unit, row, 2);

        *valueLabelPtr = value;
        ++row;
    };

    makeRow("Speed",   &this->speedLabel,   "km/h");
    makeRow("RPM",     &this->rpmLabel,     "");
    makeRow("Gear",    &this->gearLabel,    "");
    makeRow("Coolant", &this->coolantLabel, "°C");
    makeRow("Odo",     &this->odoLabel,     "km");
    makeRow("Fuel",    &this->fuelLabel,    "%");

    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);

    return widget;
}

bool VehiclePage::eventFilter(QObject *watched, QEvent *event)
{
    // Only care about events on tab pages (QWidget children of this)
    QWidget *w = qobject_cast<QWidget *>(watched);
    if (!w)
        return QTabWidget::eventFilter(watched, event);

    // Mouse press: remember start position and start timer
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            swipeActive = true;
            swipeStartPos = me->pos();
            swipeTimer.start();
            // qDebug() << "[VehiclePage] swipe start at" << swipeStartPos;
        }
        return false; // do not eat the event
    }

    // Mouse move: optional, we just keep state
    if (event->type() == QEvent::MouseMove) {
        // could update last position if needed
        return false;
    }

    // Mouse release: check if this looks like a swipe
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (!swipeActive) {
            return false;
        }

        swipeActive = false;

        QPoint endPos = me->pos();
        int dt = swipeTimer.elapsed();
        QPoint delta = endPos - swipeStartPos;

        // qDebug() << "[VehiclePage] swipe end at" << endPos
        //          << "delta =" << delta << "dt =" << dt;

        int dx = delta.x();
        int dy = delta.y();

        // Thresholds: adjust if needed
        const int minDistance = 100;      // minimum horizontal movement in pixels
        const int maxOffAxis = 100;       // max vertical movement in pixels
        const int maxDurationMs = 700;   // must be relatively quick

        if (dt > maxDurationMs)
            return false;

        if (std::abs(dx) < minDistance)
            return false;

        if (std::abs(dy) > maxOffAxis)
            return false;

        int idx = this->currentIndex();
        int count = this->count();
        if (count <= 1)
            return false;

        if (dx < 0) {
            // Swipe left: go to next tab
            int newIndex = (idx + 1) % count;
            // qDebug() << "[VehiclePage] swipe left -> tab" << newIndex;
            this->setCurrentIndex(newIndex);
            return true;    // consume event
        } else {
            // Swipe right: go to previous tab
            int newIndex = (idx - 1 + count) % count;
            // qDebug() << "[VehiclePage] swipe right -> tab" << newIndex;
            this->setCurrentIndex(newIndex);
            return true;    // consume event
        }
    }

    return QTabWidget::eventFilter(watched, event);
}
