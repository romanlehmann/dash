#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QImage>
#include <QShowEvent>
#include <QTimer>
#include "app/pages/page.hpp"

#include <QString>
#include <vector>

#define ENABLE_ENCODER_GENERIC
#include <QZXing.h>  // Include the QZXing library
#include "app/services/hostapd_config.hpp"
#include "app/services/hotspot_client.hpp"

#ifndef ENABLE_ENCODER_GENERIC
#error "ENABLE_ENCODER_GENERIC is NOT defined"
#endif

/**
 * RearSeatPage
 *
 * UI page for rear seat WiFi hotspot.
 * Generates a WiFi QR code that can be scanned by smartphones.
 * Fully embedded QR code generation, no external libraries required.
 */
class RearSeatPage : public QWidget, public Page
{
    Q_OBJECT

public:
    explicit RearSeatPage(Arbiter &arbiter, QWidget *parent = nullptr);
    void init() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    // ===== UI Elements =====
    QLabel *titleLabel;       // Page title
    QLabel *statusLabel;      // Hotspot status (ON/OFF)
    QLabel *ssidLabel;        // WiFi SSID label
    QLabel *passwordLabel;    // WiFi password label
    QFrame *qrPlaceholder;    // Placeholder frame for QR code
    QLabel *qrLabel;          // QR code image
    QPushButton *toggleButton;// Button to enable/disable hotspot
    QLabel *qrHintLabel;
    QLabel *instrBody;
    QWidget *instructionContainer; // We group the instructions to hide/show them easily
    QTimer *hotspotRefreshTimer = nullptr;

    bool hotspotEnabled = false; // Hotspot state
    bool hotspotStatusOk = false;
    QString hotspotError;

    // ===== WiFi Credentials =====
    QString wifiSsid;
    QString wifiPassword;
    QString wifiEncryption;
    QString hostapdConfigPath = "/etc/hostapd.conf";

    // ===== UI & QR Code Methods =====
    void updateHotspotUi();
    void updateWifiLabels();
    void updateQrCode();
    QImage prepareQrForDisplay(const QImage &qr);
    QSize calculateQrSize() const;
    void loadHostapdConfig();
    void refreshHotspotStatus();
    bool hasValidHotspotConfig() const;
    bool desiredHotspotEnabled() const;
    void setDesiredHotspotEnabled(bool enabled);
    void applyDesiredHotspotState();

private slots:
    void onToggleHotspotClicked();
    QString createWifiQrPayload() const;
    QImage generateWifiQrImage(const QString &payload);

private:
    HostapdConfig hostapdConfig;
    HotspotClient hotspotClient;
};

/**
 * Minimal QR code generator for WiFi payload
 *
 * Generates a module matrix (true=black, false=white)
 * Only supports alphanumeric and simple WiFi payloads
 */
class QrCodeGenerator
{
public:
    QrCodeGenerator() = default;

    /**
     * Encode a string into a QR code module matrix
     * @param text Payload string (e.g. WIFI:T:WPA;S:SSID;P:Password;;)
     * @return 2D boolean matrix (true=black, false=white)
     */
    std::vector<std::vector<bool>> encode(const QString &text);

private:
    int version = 2; // QR version 2 => 25x25 modules
    int size = 25;   // number of modules
};
