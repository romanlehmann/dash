#include <QDebug>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

#include "bike.hpp"
//#include "app/widgets/vehicle.hpp"
#include <QPixmap>
#include <QResizeEvent>

namespace {
class BikeImageLabel : public QLabel
{
public:
    explicit BikeImageLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setSourcePixmap(const QPixmap &pix)
    {
        m_pix = pix;
        updateScaled();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateScaled();
    }

private:
    QPixmap m_pix;

    void updateScaled()
    {
        if (m_pix.isNull())
            return;

        setPixmap(m_pix.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
};

} // namespace


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
    // Bike image visualization
    //
    BikeImageLabel *bikeImage = new BikeImageLabel(this);
    bikeImage->setObjectName("BikeImage");

    QPixmap pix(":/graphics/vehicle/Yamaha_bg.png");
    if (pix.isNull()) {
        qWarning() << "Failed to load :/graphics/vehicle/Yamaha_bg.png";
    } else {
        bikeImage->setSourcePixmap(pix);
    }

    root->addWidget(bikeImage, /*stretch*/ 2);



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
            labelFont.setPointSize(24);          // Label-Schriftgröße
            labelWidget->setAlignment(
                Qt::AlignLeft | Qt::AlignVCenter);

            QLabel *valueWidget = new QLabel(value, infoPanel);
            QFont valueFont = valueWidget->font();
            valueFont.setBold(true);
            valueWidget->setFont(valueFont);
            valueFont.setPointSize(24);          // Wert-Schriftgröße
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
        addRow(row++, tr("Lighting"),     tr("Low beam, tail"));
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
