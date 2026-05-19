#pragma once

#include <QObject>
#include <QTimer>

struct SystemData {
    float cpuLoad;   // %
    float ramUsage;  // %
    float cpuTemp;   // °C
};

class SystemMonitor : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitor(QObject *parent = nullptr);

    void start(int intervalMs = 1000);
    void stop();

signals:
    void dataReady(SystemData data);

private slots:
    void poll();

private:
    QTimer  m_timer;

    // CPU load state
    long long m_prevIdle  = 0;
    long long m_prevTotal = 0;

    float readCpuLoad();
    float readRamUsage();
    float readCpuTemp();
};
