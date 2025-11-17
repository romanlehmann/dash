#pragma once

#include <QPair>
#include <QtWidgets>
#include <QPluginLoader>
#include <QMap>

#include "canbus/socketcanbus.hpp"
#include "obd/message.hpp"
#include "obd/command.hpp"
#include "app/widgets/selector.hpp"
#include "app/widgets/dialog.hpp"

#include "app/pages/page.hpp"

#include <QLocalSocket>
#include <QByteArray>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QElapsedTimer>



class Arbiter;

typedef std::function<double(double, bool)> obd_decoder_t;
typedef QPair<QString, QString> units_t;

class Gauge : public QWidget {
    Q_OBJECT

   public:
    enum Orientation { BOTTOM, RIGHT };

    Gauge(units_t units, QFont value_font, QFont unit_font, Orientation orientation, int rate,
          std::vector<Command> cmds, int precision, obd_decoder_t decoder, QWidget *parent = nullptr);

    inline void start() { this->timer->start(this->rate); }
    inline void stop() { this->timer->stop(); }
    void can_callback(QByteArray payload);

   private:
    QString format_value(double value);
    QString null_value();
    QLabel *value_label;

    obd_decoder_t decoder;
    std::vector<Command> cmds;

    bool si;
    int rate;
    int precision;
    QTimer *timer;

   signals:
    void toggle_unit(bool si);
};

class VehiclePage : public QTabWidget, public Page {
    Q_OBJECT

   public:
    VehiclePage(Arbiter &arbiter, QWidget *parent = nullptr);
  
   protected:
     // New: swipe detection via event filter
    bool eventFilter(QObject *watched, QEvent *event) override;

    void init() override;

   private:
    void get_plugins();
    void load_plugin();
    QWidget *dialog_body();
    QWidget *can_bus_toggle_row();
    QWidget *si_units_row_widget();

    QMap<QString, int> capabilities;
    QMap<QString, QFileInfo> plugins;
    QStringList can_devices;
    QStringList serial_devices;
    QMap<QString, QString> paired_bt_devices;
    QPluginLoader *active_plugin;
    Selector *plugin_selector;
    Config *config;
    // Swipe state
    QPoint swipeStartPos;
    bool swipeActive = false;
    QElapsedTimer swipeTimer;

};

class DataTab : public QWidget {
    Q_OBJECT

   public:
    DataTab(Arbiter &arbiter, QWidget *parent = nullptr);

   private:
    Arbiter &arbiter;
    QWidget *speedo_tach_widget();
    // QWidget *mileage_data_widget();
    QWidget *engine_data_widget();
    QWidget *coolant_temp_widget();
    QWidget *engine_load_widget();

    std::vector<Gauge *> gauges;

    // NEU: Widget, das die Vehicle-Data vom Socket anzeigt
    QWidget *vehicle_data_widget();

    // NEU: Client-State für vehicle_data
    QLocalSocket *vehicleSocket = nullptr;
    QByteArray vehicleBuffer;
    // NEU: Timer für Reconnect
    QTimer *vehicleReconnectTimer = nullptr;

    // NEU: Labels für die einzelnen Werte
    QLabel *speedLabel = nullptr;
    QLabel *rpmLabel = nullptr;
    QLabel *gearLabel = nullptr;
    QLabel *coolantLabel = nullptr;
    QLabel *odoLabel = nullptr;
    QLabel *fuelLabel = nullptr;
};

