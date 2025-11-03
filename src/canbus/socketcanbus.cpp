#include "canbus/socketcanbus.hpp"
#include <QDebug>

SocketCANBus::SocketCANBus(QString canInterface)
{
    QCanBus* inst = QCanBus::instance();
    if (!inst) {
        qWarning() << "RLE: [SocketCANBus] QCanBus::instance() == nullptr -> start SIM";
        startSimulation();
        return;
    }

    if (!inst->plugins().contains(QStringLiteral("socketcan"))) {
        qWarning() << "RLE: [SocketCANBus] 'socketcan' plugin NOT available -> start SIM";
        startSimulation();
        return;
    }

    DASH_LOG(info) << "[SocketCANBus] 'socketcan' Available";
    socketCANAvailable = true;

    QString errorString;
    bus = inst->createDevice(QStringLiteral("socketcan"), canInterface, &errorString);
    if (!bus) {
        DASH_LOG(error) << "[SocketCANBus] Error creating CAN device, "
                        << errorString.toStdString();
        startSimulation();
        return;
    }

    DASH_LOG(info) << "[SocketCANBus] Connecting CAN interface "
                   << canInterface.toStdString();
    if (!bus->connectDevice()) {
        startSimulation();
        return;
    }

    QObject::connect(bus, &QCanBusDevice::framesReceived,
                     this, &SocketCANBus::framesAvailable);

    QObject::connect(bus, &QCanBusDevice::stateChanged, this,
        [this](QCanBusDevice::CanBusDeviceState s){
            if (s == QCanBusDevice::ConnectedState) {
                stopSimulation();
                applyFiltersIfConnected();
            } else {
                startSimulation();
            }
        });
}

SocketCANBus::~SocketCANBus()
{
    stopSimulation();

    DASH_LOG(info) << "[SocketCANBus] Disconnecting and deleting bus";
    if (bus) {
        if (bus->state() == QCanBusDevice::ConnectedState)
            bus->disconnectDevice();
        delete bus;
        bus = nullptr;
    }
}

bool SocketCANBus::writeFrame(QCanBusFrame frame)
{
    if (!bus || bus->state() != QCanBusDevice::ConnectedState)
        return false;
    return bus->writeFrame(frame);
}

SocketCANBus *SocketCANBus::get_instance()
{
    static SocketCANBus bus(Config::get_instance()->get_vehicle_interface());
    return &bus;
}

QVector<QCanBusFrame> SocketCANBus::readAllFrames(int numFrames)
{
    QVector<QCanBusFrame> frames;
    if (!bus) return frames;

    frames.reserve(numFrames);
    for (int i = 0; i < numFrames; ++i)
        frames.append(bus->readFrame());
    return frames;
}

void SocketCANBus::framesAvailable()
{
    if (!bus) return;

    const int numFrames = bus->framesAvailable();
    if (numFrames <= 0) return;

    const QVector<QCanBusFrame> frames = readAllFrames(numFrames);
    for (const auto& frame : frames) {
        const int fid = static_cast<int>(frame.frameId());
        auto it = callbacks.find(fid);
        if (it != callbacks.end()) {
            for (auto& cb : it->second)
                cb(frame.payload());
        }
    }
}

void SocketCANBus::applyFiltersIfConnected()
{
    if (!bus || bus->state() != QCanBusDevice::ConnectedState || filterList.isEmpty())
        return;

    bus->setConfigurationParameter(QCanBusDevice::RawFilterKey,
                                   QVariant::fromValue(filterList));
}

void SocketCANBus::registerFrameHandler(int id, std::function<void(QByteArray)> callback)
{

    callbacks[id].push_back(std::move(callback));

    QCanBusDevice::Filter filter;
    filter.frameId = static_cast<quint32>(id);
    filter.frameIdMask = (id > 0x7FF) ? 0x1FFFFFFF : 0x7FF;
    filter.format = QCanBusDevice::Filter::MatchBaseAndExtendedFormat;
    filter.type   = QCanBusFrame::DataFrame;
    filterList.append(filter);

    if (bus && bus->state() == QCanBusDevice::ConnectedState) {
        applyFiltersIfConnected();
    } else {
        startSimulation();
    }
}

/* =========================
 *  Simulation
 * =========================
 * Wir simulieren OBD-II Responses auf RLE_ECU_RESP_ID:
 *  - 0x0D Speed   : 0..RLE_SPEED_MAX_KMH km/h
 *  - 0x0C RPM     : RLE_RPM_MIN..RLE_RPM_MAX (proportional zu Speed)
 *  - 0x05 Coolant : RLE_COOLANT_MIN_C..RLE_COOLANT_MAX_C (langsamer Trend)
 *  - 0x04 Load    : 0..100 % (auf Basis Speed + Beschleunigung)
 * Payload: 8 Byte (gepadded).
 */
void SocketCANBus::startSimulation()
{
/* =========================
 *  Tuning-Defines (Simulation)
 * ========================= */
#define RLE_SIM_TICK_INTERVAL_MS   100     // Update-Intervall (10 Hz)
#define RLE_SPEED_MAX_KMH          180     // max. km/h
#define RLE_SPEED_STEP_KMH         2       // Schrittweite km/h pro Tick

#define RLE_RPM_MIN                800     // Leerlaufdrehzahl
#define RLE_RPM_MAX                6000    // max. Drehzahl für Demo

#define RLE_COOLANT_MIN_C          25      // Start-Temp
#define RLE_COOLANT_MAX_C          95      // Ziel-Temp (Betriebstemp)

#define RLE_LOAD_BASE_MIN          15      // Basislast minimal (%)
#define RLE_LOAD_BASE_SPAN         70      // ergibt 15..85 % über Speed
#define RLE_LOAD_ACCEL_BOOST       10      // Zuschlag beim Beschleunigen
#define RLE_LOAD_DECEL_PENALTY     8       // Abzug beim Verzögern

#define RLE_ECU_RESP_ID            0x7E8   // typische OBD-II ECU Response-ID

// OBD-II PIDs (nur Doku/Lesbarkeit)
#define RLE_PID_ENGINE_LOAD        0x04
#define RLE_PID_COOLANT_TEMP       0x05
#define RLE_PID_RPM                0x0C
#define RLE_PID_SPEED              0x0D


    if (simActive) return;
    simActive = true;

    if (!simTimer) {
        simTimer = new QTimer(this);
        simTimer->setInterval(RLE_SIM_TICK_INTERVAL_MS);
        QObject::connect(simTimer, &QTimer::timeout,
                         this, &SocketCANBus::emitSimulatedFrames);
    }

    simDir      = 1;
    simSpeed    = 0;
    simRPM      = RLE_RPM_MIN;
    simTick     = 0;
    simCoolantC = RLE_COOLANT_MIN_C;
    simLoadPct  = RLE_LOAD_BASE_MIN;

    qWarning() << "RLE: [SocketCANBus] SIMULATION STARTED (no CAN)";
    simTimer->start();
}

void SocketCANBus::stopSimulation()
{
    if (!simActive) return;
    simActive = false;
    if (simTimer) simTimer->stop();
    qDebug() << "RLE: [SocketCANBus] simulation stopped";
}

void SocketCANBus::emitSimulatedFrames()
{
    if (bus && bus->state() == QCanBusDevice::ConnectedState) {
        stopSimulation();
        return;
    }

    // --- Speed ---
    if (simSpeed >= RLE_SPEED_MAX_KMH) simDir = -1;
    if (simSpeed <= 0)                  simDir =  1;
    simSpeed += simDir * RLE_SPEED_STEP_KMH;

    // --- RPM proportional zu Speed ---
    const double ratio = double(simSpeed) / double(RLE_SPEED_MAX_KMH); // 0..1
    simRPM = RLE_RPM_MIN + int((RLE_RPM_MAX - RLE_RPM_MIN) * ratio);

    // --- Coolant langsam richten ---
    simTick++;
    if (simTick % (1000 / RLE_SIM_TICK_INTERVAL_MS) == 0) { // ~1s
        if (simSpeed > 0 && simCoolantC < RLE_COOLANT_MAX_C) simCoolantC++;
        if (simSpeed == 0 && simCoolantC > RLE_COOLANT_MIN_C) simCoolantC--;
    }

    // --- Load (%)
    int baseLoad = RLE_LOAD_BASE_MIN + int(ratio * RLE_LOAD_BASE_SPAN); // 15..85
    if (simDir > 0 && simSpeed > 0) baseLoad += RLE_LOAD_ACCEL_BOOST;   // Beschleunigen
    if (simDir < 0 && simSpeed > 0) baseLoad -= RLE_LOAD_DECEL_PENALTY; // Verzögern
    simLoadPct = qBound(0, baseLoad, 100);

    // ---------- Payloads (8 Byte) ----------
    // SPEED (0x0D)
    QByteArray speedPayload(8, 0);
    speedPayload[0] = char(0x04);
    speedPayload[1] = char(0x41);
    speedPayload[2] = char(RLE_PID_SPEED);
    speedPayload[3] = char(qBound(0, simSpeed, 255));

    // RPM (0x0C) -> rpm = ((A<<8)|B)/4
    const int rpm4x = simRPM * 4;
    QByteArray rpmPayload(8, 0);
    rpmPayload[0] = char(0x05);
    rpmPayload[1] = char(0x41);
    rpmPayload[2] = char(RLE_PID_RPM);
    rpmPayload[3] = char((rpm4x >> 8) & 0xFF);
    rpmPayload[4] = char(rpm4x & 0xFF);

    // COOLANT (0x05): A = °C + 40
    QByteArray ectPayload(8, 0);
    ectPayload[0] = char(0x04);
    ectPayload[1] = char(0x41);
    ectPayload[2] = char(RLE_PID_COOLANT_TEMP);
    ectPayload[3] = char(qBound(0, simCoolantC + 40, 255));

    // LOAD (0x04): A = % * 255 / 100
    QByteArray loadPayload(8, 0);
    loadPayload[0] = char(0x04);
    loadPayload[1] = char(0x41);
    loadPayload[2] = char(RLE_PID_ENGINE_LOAD);
    loadPayload[3] = char(qBound(0, int(simLoadPct * 255.0 / 100.0 + 0.5), 255));

    // Ausliefern an Handler der ECU-Response-ID
    auto it = callbacks.find(RLE_ECU_RESP_ID);
    if (it != callbacks.end()) {
        for (auto& cb : it->second) {
            cb(speedPayload);
            cb(rpmPayload);
            cb(ectPayload);
            cb(loadPayload);
        }
    }
}
