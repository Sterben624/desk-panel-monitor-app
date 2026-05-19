#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QMap>

struct AppConfig {
    QString portName         = "/dev/ttyACM0";
    int     sendIntervalMs   = 20;

    // Енкодер
    QString encoderMode      = "brightness"; // "brightness" | "volume"
    float   encoderSens      = 1.0f;
    bool    autoBrightness   = false;

    // Команди кнопок: "left" | "center" | "right" -> shell-команда
    QMap<QString, QString> buttonBindings;
};

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    void load();
    void save();

    AppConfig &config() { return m_config; }
    const AppConfig &config() const { return m_config; }

private:
    QSettings m_settings;
    AppConfig m_config;
};
