#include "systemmonitor.h"

#include <QFile>
#include <QDir>
#include <QTextStream>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::poll);
}

void SystemMonitor::start(int intervalMs)
{
    m_timer.start(intervalMs);
}

void SystemMonitor::stop()
{
    m_timer.stop();
}

void SystemMonitor::poll()
{
    SystemData d;
    d.cpuLoad  = readCpuLoad();
    d.ramUsage = readRamUsage();
    d.cpuTemp  = readCpuTemp();
    emit dataReady(d);
}

float SystemMonitor::readCpuLoad()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly))
        return -1.0f;

    QTextStream in(&f);
    QString line = in.readLine(); // "cpu  user nice system idle iowait irq softirq ..."
    f.close();

    QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 5)
        return -1.0f;

    long long user    = parts[1].toLongLong();
    long long nice    = parts[2].toLongLong();
    long long system  = parts[3].toLongLong();
    long long idle    = parts[4].toLongLong();
    long long iowait  = parts.size() > 5 ? parts[5].toLongLong() : 0;
    long long irq     = parts.size() > 6 ? parts[6].toLongLong() : 0;
    long long softirq = parts.size() > 7 ? parts[7].toLongLong() : 0;

    long long totalIdle = idle + iowait;
    long long total     = user + nice + system + idle + iowait + irq + softirq;

    long long diffIdle  = totalIdle - m_prevIdle;
    long long diffTotal = total     - m_prevTotal;

    m_prevIdle  = totalIdle;
    m_prevTotal = total;

    if (diffTotal == 0)
        return 0.0f;

    return static_cast<float>(diffTotal - diffIdle) / diffTotal * 100.0f;
}

float SystemMonitor::readRamUsage()
{
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly))
        return -1.0f;

    const QByteArray content = f.readAll();
    f.close();

    long long memTotal = -1, memAvailable = -1;
    for (const QByteArray &line : content.split('\n')) {
        if (line.startsWith("MemTotal:"))
            memTotal = line.mid(line.indexOf(':') + 1).trimmed().split(' ').first().toLongLong();
        else if (line.startsWith("MemAvailable:"))
            memAvailable = line.mid(line.indexOf(':') + 1).trimmed().split(' ').first().toLongLong();
        if (memTotal != -1 && memAvailable != -1)
            break;
    }

    if (memTotal <= 0 || memAvailable < 0)
        return -1.0f;

    return static_cast<float>(memTotal - memAvailable) / memTotal * 100.0f;
}

float SystemMonitor::readCpuTemp()
{
    // Перебираємо thermal_zone* і беремо першу з типом x86_pkg_temp або просто першу
    QDir dir("/sys/class/thermal");
    QStringList zones = dir.entryList(QStringList() << "thermal_zone*", QDir::Dirs);

    QString bestPath;
    for (const QString &zone : zones) {
        QString typePath = QString("/sys/class/thermal/%1/type").arg(zone);
        QFile typeFile(typePath);
        if (typeFile.open(QIODevice::ReadOnly)) {
            QString type = typeFile.readAll().trimmed();
            typeFile.close();
            if (type == "x86_pkg_temp") {
                bestPath = QString("/sys/class/thermal/%1/temp").arg(zone);
                break;
            }
            if (bestPath.isEmpty())
                bestPath = QString("/sys/class/thermal/%1/temp").arg(zone);
        }
    }

    if (bestPath.isEmpty())
        return -1.0f;

    QFile f(bestPath);
    if (!f.open(QIODevice::ReadOnly))
        return -1.0f;

    bool ok;
    float milli = f.readAll().trimmed().toFloat(&ok);
    f.close();

    return ok ? milli / 1000.0f : -1.0f;
}
