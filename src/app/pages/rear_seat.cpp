#include "app/pages/rear_seat.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout> // Required for the split layout
#include <QPainter>
#include <QFont>

/**
 * Constructor
 *
 * Registers page in the app framework
 */
RearSeatPage::RearSeatPage(Arbiter &arbiter, QWidget *parent)
    : QWidget(parent)
    , Page(arbiter, "Rear Seat", "cast_dark", true, this)
{
}

/**
 * Build UI
 *
 * Creates labels, QR placeholder, and toggle button
 */
void RearSeatPage::init()
{
    // Main Vertical Layout (Title top, Content middle, Button bottom)
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);

    // --- Title ---
    titleLabel = new QLabel("Rear Seat Entertainment", this);
    QFont titleFont;
    titleFont.setPointSize(26);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    // --- Content Area ---
    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(40); 

    // 1. LEFT SIDE: Info Labels
    auto *infoLayout = new QVBoxLayout();
    
    statusLabel = new QLabel("WiFi Hotspot: OFF", this);
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: red;");

    ssidLabel = new QLabel(this);
    ssidLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ssidLabel->setStyleSheet("font-size: 28px;");

    passwordLabel = new QLabel(this);
    passwordLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    passwordLabel->setStyleSheet("font-size: 28px;");

    infoLayout->addStretch();
    infoLayout->addWidget(statusLabel);
    infoLayout->addWidget(ssidLabel);
    infoLayout->addWidget(passwordLabel);
    infoLayout->addStretch();

    // 2. MIDDLE: QR Code OR Hint Text
    auto *qrContainerLayout = new QVBoxLayout();
    qrContainerLayout->setAlignment(Qt::AlignCenter);

    // QR Box
    qrPlaceholder = new QFrame(this);
    qrPlaceholder->setFixedSize(350, 350); 
    qrPlaceholder->setFrameShape(QFrame::Box);
    qrPlaceholder->setStyleSheet("background-color: white; border-radius: 15px;"); // Added rounded corners

    auto *qrInnerLayout = new QVBoxLayout(qrPlaceholder);
    qrInnerLayout->setContentsMargins(20, 20, 20, 20);
    qrInnerLayout->setSpacing(0); 

    qrLabel = new QLabel(qrPlaceholder);
    qrLabel->setAlignment(Qt::AlignCenter);
    qrLabel->setScaledContents(false);
    qrInnerLayout->addWidget(qrLabel);

    // Hint Label (Centered when hotspot is off)
    qrHintLabel = new QLabel("Please enable the hotspot\nto connect your device.", this);
    qrHintLabel->setAlignment(Qt::AlignCenter);
    qrHintLabel->setStyleSheet("font-size: 28px;");
    qrHintLabel->setVisible(false);

    qrContainerLayout->addStretch();
    qrContainerLayout->addWidget(qrPlaceholder);
    qrContainerLayout->addWidget(qrHintLabel);
    qrContainerLayout->addStretch();

    // 3. RIGHT SIDE: Instructions (Container for visibility toggle)
    instructionContainer = new QWidget(this);
    auto *instructionLayout = new QVBoxLayout(instructionContainer);
    instructionLayout->setSpacing(15);
    instructionLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *instrTitle = new QLabel("How to Stream", instructionContainer);
    instrTitle->setStyleSheet("font-size: 24px; font-weight: bold;");
    instrTitle->setAlignment(Qt::AlignLeft);

    instrBody = new QLabel(instructionContainer);
    instrBody->setObjectName("instrBody"); // Required for the QSS to find it
    instrBody->setTextFormat(Qt::RichText);
    
    // Maintain your alignment and wrap settings
    instrBody->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    instrBody->setWordWrap(true);

    // Note: If you have a QSS file, it's better to move "font-size" there too.
    // If you keep setStyleSheet() here, it might override QSS properties.
    instrBody->setStyleSheet("font-size: 20px;");

    instructionLayout->addStretch();
    instructionLayout->addWidget(instrTitle);
    instructionLayout->addWidget(instrBody);
    instructionLayout->addStretch();

// --- Assemble Content Layout (1 : 0 : 1 stretch ratio for perfect centering) ---
    contentLayout->addLayout(infoLayout, 1);           // Linke Seite (Stretch 1)
    contentLayout->addLayout(qrContainerLayout, 0);    // Mitte (QR Box / Hint)

    // RECHTE SEITE: Wir erstellen ein Wrapper-Layout für den Platzhalter
    auto *rightSideWrapper = new QVBoxLayout();
    rightSideWrapper->addWidget(instructionContainer);
    
    // Wir fügen das Layout hinzu, nicht das Widget direkt. 
    // Der Stretch von 1 sorgt dafür, dass dieser Bereich immer Platz wegnimmt.
    contentLayout->addLayout(rightSideWrapper, 1);

    // --- Toggle Button ---
    toggleButton = new QPushButton("Enable Hotspot", this);
    toggleButton->setFixedHeight(60); 
    toggleButton->setStyleSheet("font-size: 24px; font-weight: bold;");
    connect(toggleButton, &QPushButton::clicked, this, &RearSeatPage::onToggleHotspotClicked);

    // --- Final Assembly ---
    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(contentLayout); 
    mainLayout->addWidget(toggleButton);

    // --- Initialize UI State ---
    updateWifiLabels();
    updateQrCode();
    updateHotspotUi();
}

/**
 * Toggle button pressed
 * Switches hotspot on/off (UI-only)
 */
void RearSeatPage::onToggleHotspotClicked()
{
    hotspotEnabled = !hotspotEnabled;
    updateHotspotUi();
}

/**
 * Updates instructions with dynamic SSID, Device Name and Theme-based Icon
 */
void RearSeatPage::updateWifiLabels()
{
    // Ensure labels are up to date
    ssidLabel->setText(QString("SSID:\n%1").arg(wifiSsid));
    passwordLabel->setText(QString("Password:\n%1").arg(wifiPassword));

    // Define the single gray icon path
    QString iconPath = ":/icons/cast_dark.svg"; 

    // Build the Rich Text string properly formatted for C++
    instrBody->setText(QString(
        "1. Connect your phone to the WiFi<br>"
        "   network: <b>%1</b><br><br>"
        "2. Open your preferred app (e.g.,<br>"
        "   YouTube, Spotify, Netflix).<br><br>"
        "3. Tap the <img src='%2' width='22' height='22' style='vertical-align: middle;'> "
        "   <b>Cast icon</b><br>"
        "   and select <b>%3</b><br>"
        "   to start streaming."
    ).arg(wifiSsid)
     .arg(iconPath)
     .arg(titleLabel->text()));
}



/**
 * Update all UI elements based on hotspot state
 */
void RearSeatPage::updateHotspotUi()
{
    if (hotspotEnabled)
    {
        statusLabel->setText("WiFi Hotspot: ON");
        statusLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: green;");
        toggleButton->setText("Disable Hotspot");

        // Show connection area
        qrPlaceholder->setVisible(true);
        instructionContainer->setVisible(true); // Now visible
        ssidLabel->setVisible(true);
        passwordLabel->setVisible(true);
        qrHintLabel->setVisible(false);

        updateQrCode();
    }
    else
    {
        statusLabel->setText("WiFi Hotspot: OFF");
        statusLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: red;");
        toggleButton->setText("Enable Hotspot");

        // Hide connection area, show hint
        qrPlaceholder->setVisible(false);
        instructionContainer->setVisible(false); // Now hidden
        ssidLabel->setVisible(false);
        passwordLabel->setVisible(false);
        qrHintLabel->setVisible(true);
    }
}

/**
 * Create WiFi QR payload
 *
 * Format: WIFI:T:<encryption>;S:<SSID>;P:<Password>;;
 */
QString RearSeatPage::createWifiQrPayload() const
{
    // Payload format for WiFi QR code
    return QString("WIFI:T:WPA;S:%1;P:%2;;")
        .arg(wifiSsid)
        .arg(wifiPassword);
}

/**
 * Calculates the required size based on the label, not the container.
 * Accounts for High-DPI displays (Retina).
 */
QSize RearSeatPage::calculateQrSize() const
{
    // Query the label size directly (layout margins are already subtracted).
    // If the label is not yet visible (size is empty), fallback to placeholder size minus margins.
    QSize size = qrLabel->size();
    
    if (size.isEmpty()) {
        int margin = 20 * 2; // Left + right margins from the layout
        size = qrPlaceholder->size() - QSize(margin, margin);
    }

    // High DPI Support: If the screen has scaling (e.g. 2x on Retina),
    // we must generate the image at a higher resolution to keep it sharp.
    qreal dpr = this->devicePixelRatioF(); 
    size *= dpr;

    // Maintain square aspect ratio
    int side = qMin(size.width(), size.height());
    return QSize(side, side);
}

/**
 * Generates the QR code image directly from the payload.
 */
QImage RearSeatPage::generateWifiQrImage(const QString &payload)
{
    QSize qrSize = calculateQrSize();

    // Generate via QZXing
    QImage qrImage = QZXing::encodeData(
        payload,
        QZXing::EncoderFormat_QR_CODE,
        qrSize,
        QZXing::EncodeErrorCorrectionLevel_L,
        false, // No internal border generated by QZXing
        false
    );
    
    // Fallback if generation fails
    if (qrImage.isNull()) {
        return QImage();
    }

    // High DPI Support: Set the device pixel ratio on the image 
    // so Qt knows how to render it correctly on high-res screens.
    qrImage.setDevicePixelRatio(this->devicePixelRatioF());
    
    return qrImage;
}

/**
 * Update QR Code UI
 */
void RearSeatPage::updateQrCode()
{
    // Only generate if visible/needed
    if (!qrPlaceholder->isVisible()) return;

    const QString payload = createWifiQrPayload();

    // 1. Calculate the available space inside the placeholder
    // We take the fixed size (300) and subtract the margins (20 left + 20 right)
    int quietZone = 20; 
    int targetSize = qrPlaceholder->width() - (2 * quietZone);

    // 2. Generate the QR Image
    QImage qrImage = QZXing::encodeData(
        payload,
        QZXing::EncoderFormat_QR_CODE,
        QSize(targetSize, targetSize), 
        QZXing::EncodeErrorCorrectionLevel_L,
        false, 
        false
    );

    if (qrImage.isNull()) {
        qrLabel->clear();
        return;
    }

    // 3. Convert to Pixmap and SCALE it to fit the layout perfectly
    QPixmap pixmap = QPixmap::fromImage(qrImage);

    // Scaling Logic:
    // - Qt::FastTransformation: CRITICAL! Keeps the pixels sharp.
    qrLabel->setPixmap(pixmap.scaled(
        targetSize, 
        targetSize, 
        Qt::KeepAspectRatio, 
        Qt::FastTransformation 
    ));
}