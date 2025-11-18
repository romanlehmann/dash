#include <QDebug>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

#include "bike.hpp"
//#include "app/widgets/vehicle.hpp"
#include <QPixmap>
#include <QResizeEvent>

#include <QMouseEvent>
#include <QStringList>


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

    // NEU: Liste der Bildnamen setzen
    void setImageList(const QStringList &images)
    {
        m_images = images;
        m_currentIndex = 0;
        loadCurrentImage();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateScaled();
    }

    // NEU: Klick-Handler
    void mousePressEvent(QMouseEvent *event) override
    {
        QLabel::mousePressEvent(event);

        if (m_images.isEmpty())
            return;

        m_currentIndex = (m_currentIndex + 1) % m_images.size();
        loadCurrentImage();
    }

private:
    QPixmap m_pix;
    QStringList m_images;
    int m_currentIndex = 0;

    void updateScaled()
    {
        if (m_pix.isNull())
            return;

        setPixmap(m_pix.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void loadCurrentImage()
    {
        if (m_images.isEmpty())
            return;

        // Pfad ggf. anpassen, falls deine Ressourcen anders heißen
        QString path = QString(":/graphics/vehicle/%1").arg(m_images[m_currentIndex]);
        QPixmap pix(path);
        if (pix.isNull()) {
            qWarning() << "Failed to load" << path;
            return;
        }
        setSourcePixmap(pix);
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

        // Statt QVBoxLayout -> QHBoxLayout
        QHBoxLayout *root = new QHBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(18);

        //
        // Bike image visualization (links)
        //
        BikeImageLabel *bikeImage = new BikeImageLabel(this);
        bikeImage->setObjectName("BikeImage");

        // Die Bilder-Liste setzen (s. Abschnitt 2)
       //bikeImage->setImageList(QStringList()
       //                        << "Yamaha_3.png"
       //                        << "Yamaha_2.png"
       //                        << "Yamaha_3.png");

       bikeImage->setImageList(QStringList() << "Yamaha_3.png");


        // Bild bekommt mehr Platz
        root->addWidget(bikeImage, /*stretch*/ 2);

           //
    // Info panel (rechts)
    //
    QFrame *infoPanel = new QFrame(this);
    infoPanel->setObjectName("infoBox");
    infoPanel->setFrameShape(QFrame::NoFrame);
    infoPanel->setContentsMargins(16, 8, 16, 16);
                  //              ^   ^   ^   ^
                  //              |   |   |   |
                  //              |   |   |   └─ bottom margin: 16 px Abstand nach unten
                  //              |   |   └──── right margin: 16 px Abstand nach rechts
                  //              |   └──────── top margin: 8 px Abstand nach oben
                  //              └──────────── left margin: 16 px Abstand nach links


    // Use a vertical layout: title on top, grid with rows below
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(12);

    // Title label: "Vehicle status"
    QLabel *title = new QLabel(tr("Vehicle status"), infoPanel);
    QFont titleFont = title->font();
    titleFont.setPointSize(26);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setProperty("infoTitle", true);  // For optional styling in the stylesheet
    infoLayout->addWidget(title);

    // Grid layout for label/value rows
    QGridLayout *grid = new QGridLayout();
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    infoLayout->addLayout(grid);

    auto addRow = [infoPanel, grid](int row,
                                    const QString &label,
                                    const QString &value) {
        QLabel *labelWidget = new QLabel(label, infoPanel);
        QFont labelFont = labelWidget->font();
        labelFont.setWeight(QFont::Light);
        labelFont.setPointSize(22);
        labelWidget->setFont(labelFont);
        labelWidget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        labelWidget->setProperty("infoRow", true);   // Mark row labels for table-like styling

        QLabel *valueWidget = new QLabel(value, infoPanel);
        QFont valueFont = valueWidget->font();
        valueFont.setBold(true);
        valueFont.setPointSize(22);
        valueWidget->setFont(valueFont);
        valueWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueWidget->setProperty("infoRow", true);   // Mark row values for table-like styling

        grid->addWidget(labelWidget, row, 0);
        grid->addWidget(valueWidget, row, 1);
    };

    int row = 0;
    addRow(row++, tr("Front tire"),   tr("40 psi  /  32 °C"));
    addRow(row++, tr("Rear tire"),    tr("42 psi  /  34 °C"));
    addRow(row++, tr("Battery"),      tr("12.6 V  /  92 %"));
    addRow(row++, tr("Fuel Range"),   tr("182 km"));
    addRow(row++, tr("Engine hours"), tr("23.8 h"));
    addRow(row++, tr("Next service"), tr("1500 km"));
    addRow(row++, tr("Riding mode"),  tr("cruise"));

    // InfoPanel rechts im Layout
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
