#include <QDebug>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

#include "bike.hpp"
#include "app/widgets/vehicle.hpp"

// Simple tab widget that shows a bike-style vehicle view
// with hard-coded status values.
class BikeTab : public QWidget {
public:
    explicit BikeTab(Arbiter &arbiter, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("Bike");

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(18);

        //
        // Vehicle visualization
        //
        Vehicle *vehicle = new Vehicle(arbiter, this);

        // Configure the vehicle widget to loosely represent a bike.
        // The Vehicle widget is car-oriented (four wheels), so we map
        // the motorcycle's two tires onto both left/right positions.
        vehicle->disable_sensors();                 // keep the view clean

        // Use PSI for consistency with the test plugin
        vehicle->pressure_init("psi", 34);         // unit + warning threshold

        // Hard-coded motorcycle tire pressures (example values)
        const uint8_t frontPsi = 36;               // front tire ~2.5 bar
        const uint8_t rearPsi  = 42;               // rear tire ~2.9 bar

        // Front wheel -> both front positions
        vehicle->pressure(Position::FRONT_LEFT,  frontPsi);
        vehicle->pressure(Position::FRONT_RIGHT, frontPsi);

        // Rear wheel -> both rear positions
        vehicle->pressure(Position::BACK_LEFT,   rearPsi);
        vehicle->pressure(Position::BACK_RIGHT,  rearPsi);

        // Lights and basic appearance
        vehicle->headlights(true);
        vehicle->taillights(true);
        vehicle->indicators(Position::LEFT, false);
        vehicle->indicators(Position::RIGHT, false);
        vehicle->hazards(false);
        vehicle->wheel_steer(0);

        root->addWidget(vehicle, /*stretch*/ 2);

        //
        // Info panel below the vehicle
        //
        QFrame *infoPanel = new QFrame(this);
        infoPanel->setFrameShape(QFrame::NoFrame);
        QGridLayout *grid = new QGridLayout(infoPanel);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(6);

        auto addRow = [infoPanel, grid](int row,
                                        const QString &label,
                                        const QString &value) {
            QLabel *labelWidget = new QLabel(label, infoPanel);
            QFont labelFont = labelWidget->font();
            labelFont.setWeight(QFont::Light);
            labelWidget->setFont(labelFont);
            labelWidget->setAlignment(
                Qt::AlignLeft | Qt::AlignVCenter);

            QLabel *valueWidget = new QLabel(value, infoPanel);
            QFont valueFont = valueWidget->font();
            valueFont.setBold(true);
            valueWidget->setFont(valueFont);
            valueWidget->setAlignment(
                Qt::AlignRight | Qt::AlignVCenter);

            grid->addWidget(labelWidget, row, 0);
            grid->addWidget(valueWidget, row, 1);
        };

        // Hard-coded bike status values
        int row = 0;
        addRow(row++, tr("Front tire"),   tr("36 psi / 32 °C"));
        addRow(row++, tr("Rear tire"),    tr("42 psi / 34 °C"));
        addRow(row++, tr("Battery"),      tr("12.6 V  /  92 %"));
        addRow(row++, tr("Lighting"),     tr("Low beam, tail, DRL"));
        addRow(row++, tr("Indicators"),   tr("Off"));
        addRow(row++, tr("Riding mode"),  tr("Sport"));

        root->addWidget(infoPanel, /*stretch*/ 1);
    }
};


// ------------------- Bike plugin implementation -------------------

Bike::~Bike() = default;

QList<QWidget *> Bike::widgets()
{
    QList<QWidget *> tabs;

    if (this->bike_tab)
        tabs.append(this->bike_tab);

    return tabs;
}

bool Bike::init(ICANBus *canbus)
{
    Q_UNUSED(canbus);

    if (!this->arbiter) {
        qWarning() << "[BikePlugin] Arbiter is not set, init failed";
        return false;
    }

    if (!this->bike_tab) {
        this->bike_tab = new BikeTab(*this->arbiter);
    }

    return true;
}
