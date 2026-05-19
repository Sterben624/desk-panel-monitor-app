#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QLabel>

#include "systemmonitor.h"
#include "panellink.h"
#include "configmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSystemData(SystemData data);
    void onMkData(MkData data);
    void onConnectionLost();
    void onSerialError(QString message);

    void onConnectToggled(bool checked);
    void onRefreshPorts();
    void onSaveSettings();
    void onOpenBindings();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    Ui::MainWindow *ui;

    SystemMonitor  m_sysMonitor;
    PanelLink      m_panel;
    ConfigManager  m_config;

    QSystemTrayIcon *m_tray;
    QMenu           *m_trayMenu;

    // TX на МК іде 50 Гц незалежно від частоти зчитування системи
    QTimer      m_sendTimer;
    SystemData  m_lastSysData = {};

    // Відстеження яскравості щоб не спамити команди
    int   m_lastAppliedBrightness = -1;
    float m_accEncoderDelta       = 0.0f;

    void setupTray();
    void setupConnections();
    void loadSettingsToUi();
    void setConnectedState(bool connected);
    void setButtonLed(QLabel *led, bool active);

    void executeButtonCommand(const QString &button);
    void handleEncoder(int8_t delta);
    void applyScreenBrightness(int percent);
    void applyVolumeDelta(int steps);

    void updateCo2Color(uint16_t ppm);
    void updateCpuTempColor(float temp);
    void updateAqiColor(uint8_t aqi);
};
