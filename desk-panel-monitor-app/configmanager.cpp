#include "configmanager.h"

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings("desk-panel", "desk-panel-monitor-app")
{
}

void ConfigManager::load()
{
    m_config.portName       = m_settings.value("serial/portName",       "/dev/ttyACM0").toString();
    m_config.sendIntervalMs = m_settings.value("serial/sendIntervalMs", 20).toInt();

    m_config.encoderMode    = m_settings.value("encoder/mode",          "brightness").toString();
    m_config.encoderSens    = m_settings.value("encoder/sensitivity",   1.0f).toFloat();
    m_config.autoBrightness = m_settings.value("display/autoBrightness",false).toBool();

    m_settings.beginGroup("buttonBindings");
    m_config.buttonBindings["left"]   = m_settings.value("left",   "").toString();
    m_config.buttonBindings["center"] = m_settings.value("center", "").toString();
    m_config.buttonBindings["right"]  = m_settings.value("right",  "").toString();
    m_settings.endGroup();
}

void ConfigManager::save()
{
    m_settings.setValue("serial/portName",       m_config.portName);
    m_settings.setValue("serial/sendIntervalMs", m_config.sendIntervalMs);

    m_settings.setValue("encoder/mode",          m_config.encoderMode);
    m_settings.setValue("encoder/sensitivity",   m_config.encoderSens);
    m_settings.setValue("display/autoBrightness",m_config.autoBrightness);

    m_settings.beginGroup("buttonBindings");
    m_settings.setValue("left",   m_config.buttonBindings.value("left"));
    m_settings.setValue("center", m_config.buttonBindings.value("center"));
    m_settings.setValue("right",  m_config.buttonBindings.value("right"));
    m_settings.endGroup();
}
