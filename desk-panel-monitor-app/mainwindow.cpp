#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "bindingdialog.h"

#include <QSerialPortInfo>
#include <QCloseEvent>
#include <QFile>
#include <QProcess>
#include <QLabel>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tray(new QSystemTrayIcon(this))
    , m_trayMenu(new QMenu(this))
{
    ui->setupUi(this);

    m_config.load();

    setupTray();
    setupConnections();
    loadSettingsToUi();
    onRefreshPorts();

    // Зчитуємо систему раз на секунду — значення стабільні
    m_sysMonitor.start(1000);

    // TX на МК — 50 Гц, шлємо останні відомі дані
    m_sendTimer.setInterval(20);
    connect(&m_sendTimer, &QTimer::timeout, this, [this]() {
        m_panel.sendData(m_lastSysData.cpuLoad,
                         m_lastSysData.ramUsage,
                         m_lastSysData.cpuTemp);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ─────────────────────────────────────────────
//  Ініціалізація
// ─────────────────────────────────────────────

void MainWindow::setupTray()
{
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    m_tray->setIcon(icon);
    m_tray->setToolTip("Панель моніторингу");

    QAction *actionShow = m_trayMenu->addAction("Показати");
    m_trayMenu->addSeparator();
    QAction *actionQuit = m_trayMenu->addAction("Вийти");

    connect(actionShow, &QAction::triggered, this, &QWidget::show);
    connect(actionQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();
}

void MainWindow::setupConnections()
{
    connect(&m_sysMonitor, &SystemMonitor::dataReady, this, &MainWindow::onSystemData);

    connect(&m_panel, &PanelLink::dataReceived,   this, &MainWindow::onMkData);
    connect(&m_panel, &PanelLink::connectionLost, this, &MainWindow::onConnectionLost);
    connect(&m_panel, &PanelLink::errorOccurred,  this, &MainWindow::onSerialError);

    connect(ui->btnConnect,      &QPushButton::toggled, this, &MainWindow::onConnectToggled);
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(ui->btnSaveSettings, &QPushButton::clicked, this, &MainWindow::onSaveSettings);
    connect(ui->btnBindings,     &QPushButton::clicked, this, &MainWindow::onOpenBindings);
}

void MainWindow::loadSettingsToUi()
{
    const AppConfig &cfg = m_config.config();

    int idx = ui->portCombo->findText(cfg.portName);
    if (idx >= 0) ui->portCombo->setCurrentIndex(idx);
    else          ui->portCombo->setCurrentText(cfg.portName);

    ui->comboEncoderMode->setCurrentIndex(cfg.encoderMode == "volume" ? 1 : 0);
    ui->spinEncoderSens->setValue(static_cast<double>(cfg.encoderSens));
    ui->checkAutoBrightness->setChecked(cfg.autoBrightness);
}

// ─────────────────────────────────────────────
//  Дані системи
// ─────────────────────────────────────────────

void MainWindow::onSystemData(SystemData data)
{
    m_lastSysData = data;

    ui->progressCpu->setValue(static_cast<int>(data.cpuLoad));
    ui->progressRam->setValue(static_cast<int>(data.ramUsage));

    if (data.cpuTemp > 0.0f) {
        ui->labelCpuTemp->setText(QString("%1 °C").arg(data.cpuTemp, 0, 'f', 1));
        updateCpuTempColor(data.cpuTemp);
    } else {
        ui->labelCpuTemp->setText("— °C");
    }
}

void MainWindow::updateCpuTempColor(float temp)
{
    QString color;
    if      (temp >= 85.0f) color = "#c62828";
    else if (temp >= 70.0f) color = "#ef6c00";
    else                    color = "#2e7d32";
    ui->labelCpuTemp->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13pt; font-weight: bold; }").arg(color));
}

// ─────────────────────────────────────────────
//  Дані з МК
// ─────────────────────────────────────────────

void MainWindow::onMkData(MkData data)
{
    // Середовище
    ui->labelMkTemp->setText(    QString("%1 °C" ).arg(data.temperature, 0, 'f', 1));
    ui->labelMkHumidity->setText(QString("%1 %"  ).arg(data.humidity,    0, 'f', 1));
    ui->labelMkEco2->setText(    QString("%1 ppm").arg(data.eco2));
    ui->labelMkTvoc->setText(    QString("%1 ppb").arg(data.tvoc));
    ui->labelMkAqi->setText(     QString::number(data.aqi));

    updateCo2Color(data.eco2);
    updateAqiColor(data.aqi);

    // Стан панелі
    ui->progressBrightness->setValue(data.brightness);
    ui->labelBrightness->setText(QString::number(data.brightness));
    ui->progressLight->setValue(data.lightRaw);
    ui->labelLight->setText(QString::number(data.lightRaw));

    // Кнопки — спалах на 200 мс + виконання команди
    const uint8_t btns = data.buttons;
    if (btns & 0x04) { setButtonLed(ui->ledBtnLeft,   true); executeButtonCommand("left");   }
    if (btns & 0x02) { setButtonLed(ui->ledBtnCenter, true); executeButtonCommand("center"); }
    if (btns & 0x01) { setButtonLed(ui->ledBtnRight,  true); executeButtonCommand("right");  }
    if (btns) {
        QTimer::singleShot(200, this, [this, btns]() {
            if (btns & 0x04) setButtonLed(ui->ledBtnLeft,   false);
            if (btns & 0x02) setButtonLed(ui->ledBtnCenter, false);
            if (btns & 0x01) setButtonLed(ui->ledBtnRight,  false);
        });
    }

    // Яскравість / гучність
    if (data.encoderDelta != 0) {
        ui->labelEncoder->setText(
            (data.encoderDelta > 0 ? "+" : "") + QString::number(data.encoderDelta));
        handleEncoder(data.encoderDelta);
    } else {
        ui->labelEncoder->setText("0");
    }

    // Автояскравість від фоторезистора
    if (ui->checkAutoBrightness->isChecked()) {
        int pct = qBound(5, static_cast<int>(data.lightRaw / 4095.0f * 100.0f), 100);
        if (qAbs(pct - m_lastAppliedBrightness) >= 2) {
            applyScreenBrightness(pct);
            m_lastAppliedBrightness = pct;
        }
    }
}

void MainWindow::updateCo2Color(uint16_t ppm)
{
    QString color;
    if      (ppm >= 2000) color = "#c62828";
    else if (ppm >= 1000) color = "#ef6c00";
    else                  color = "#212121";
    ui->labelMkEco2->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13pt; font-weight: bold; }").arg(color));
}

void MainWindow::updateAqiColor(uint8_t aqi)
{
    static const char *colors[] = {
        "#212121",  // 0 — невалідний
        "#43a047",  // 1 — відмінно
        "#7cb342",  // 2 — добре
        "#f9a825",  // 3 — помірно
        "#ef6c00",  // 4 — погано
        "#c62828",  // 5 — дуже погано
    };
    const char *color = (aqi <= 5) ? colors[aqi] : colors[0];
    ui->labelMkAqi->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13pt; font-weight: bold; }").arg(color));
}

void MainWindow::setButtonLed(QLabel *led, bool active)
{
    led->setStyleSheet(active
        ? "QLabel { font-size: 14px; color: #1976d2; }"
        : "QLabel { font-size: 14px; color: #bdbdbd; }");
}

// ─────────────────────────────────────────────
//  Команди кнопок
// ─────────────────────────────────────────────

void MainWindow::executeButtonCommand(const QString &button)
{
    QString cmd = m_config.config().buttonBindings.value(button).trimmed();
    if (cmd.isEmpty()) return;
    QProcess::startDetached("bash", {"-c", cmd});
}

// ─────────────────────────────────────────────
//  Яскравість і гучність від енкодера
// ─────────────────────────────────────────────

void MainWindow::handleEncoder(int8_t delta)
{
    float sens = m_config.config().encoderSens;
    m_accEncoderDelta += delta * sens;

    int steps = static_cast<int>(m_accEncoderDelta);
    if (steps == 0) return;
    m_accEncoderDelta -= steps;

    if (ui->checkAutoBrightness->isChecked())
        return; // В авто-режимі енкодер не керує яскравістю екрана

    const QString &mode = m_config.config().encoderMode;
    if (mode == "brightness")
        applyScreenBrightness(qBound(0, m_lastAppliedBrightness + steps, 100));
    else if (mode == "volume")
        applyVolumeDelta(steps);
}

void MainWindow::applyScreenBrightness(int percent)
{
    percent = qBound(0, percent, 100);
    m_lastAppliedBrightness = percent;

    static const QString path = "/sys/class/backlight/intel_backlight/brightness";
    static const QString maxPath = "/sys/class/backlight/intel_backlight/max_brightness";

    static int maxVal = 0;
    if (maxVal == 0) {
        QFile f(maxPath);
        if (f.open(QIODevice::ReadOnly))
            maxVal = f.readAll().trimmed().toInt();
        if (maxVal <= 0) maxVal = 7500;
    }

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        int raw = qRound(percent / 100.0 * maxVal);
        f.write(QByteArray::number(raw));
    }
}

void MainWindow::applyVolumeDelta(int steps)
{
    // pactl з PulseAudio / PipeWire
    QString arg = (steps > 0)
        ? QString("+%1%").arg(qAbs(steps))
        : QString("-%1%").arg(qAbs(steps));
    QProcess::startDetached("pactl", {"set-sink-volume", "@DEFAULT_SINK@", arg});
}

// ─────────────────────────────────────────────
//  Підключення / відключення
// ─────────────────────────────────────────────

void MainWindow::onConnectToggled(bool checked)
{
    if (checked) {
        QString port = ui->portCombo->currentText().trimmed();
        if (port.isEmpty()) { ui->btnConnect->setChecked(false); return; }
        if (m_panel.open(port)) {
            setConnectedState(true);
            m_sendTimer.start();
        } else {
            ui->btnConnect->setChecked(false);
            statusBar()->showMessage("Помилка: " + m_panel.lastError(), 5000);
        }
    } else {
        m_sendTimer.stop();
        m_panel.close();
        setConnectedState(false);
    }
}

void MainWindow::onConnectionLost()
{
    m_sendTimer.stop();
    m_panel.close();
    ui->btnConnect->setChecked(false);
    setConnectedState(false);
    statusBar()->showMessage("З'єднання з панеллю втрачено", 5000);
}

void MainWindow::onSerialError(QString message)
{
    statusBar()->showMessage("Serial: " + message, 4000);
}

void MainWindow::setConnectedState(bool connected)
{
    if (connected) {
        ui->labelConnStatus->setText("● Підключено");
        ui->labelConnStatus->setStyleSheet("QLabel { font-weight: bold; color: #2e7d32; }");
        ui->btnConnect->setText("Відключити");
    } else {
        ui->labelConnStatus->setText("● Не підключено");
        ui->labelConnStatus->setStyleSheet("QLabel { font-weight: bold; color: #c62828; }");
        ui->btnConnect->setText("Підключити");
    }
}

// ─────────────────────────────────────────────
//  Налаштування
// ─────────────────────────────────────────────

void MainWindow::onRefreshPorts()
{
    QString current = ui->portCombo->currentText();
    ui->portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->portCombo->addItem(info.portName());
    int idx = ui->portCombo->findText(current);
    if (idx >= 0) ui->portCombo->setCurrentIndex(idx);
    else if (!current.isEmpty()) ui->portCombo->setCurrentText(current);
}

void MainWindow::onSaveSettings()
{
    AppConfig &cfg = m_config.config();
    cfg.portName       = ui->portCombo->currentText().trimmed();
    cfg.encoderMode    = (ui->comboEncoderMode->currentIndex() == 1) ? "volume" : "brightness";
    cfg.encoderSens    = static_cast<float>(ui->spinEncoderSens->value());
    cfg.autoBrightness = ui->checkAutoBrightness->isChecked();
    m_config.save();
    statusBar()->showMessage("Налаштування збережено", 3000);
}

void MainWindow::onOpenBindings()
{
    BindingDialog dlg(this);
    dlg.setBindings(m_config.config().buttonBindings);
    if (dlg.exec() == QDialog::Accepted) {
        m_config.config().buttonBindings = dlg.bindings();
        m_config.save();
    }
}

// ─────────────────────────────────────────────
//  Tray / закриття
// ─────────────────────────────────────────────

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        setVisible(!isVisible());
        if (isVisible()) raise();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray->isVisible()) {
        hide();
        event->ignore();
    }
}
