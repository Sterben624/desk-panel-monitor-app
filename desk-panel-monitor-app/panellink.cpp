#include "panellink.h"

#include <QTimer>
#include <cstring>

PanelLink::PanelLink(QObject *parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead,
            this, &PanelLink::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred,
            this, &PanelLink::onErrorOccurred);
}

bool PanelLink::open(const QString &portName)
{
    if (m_port.isOpen())
        m_port.close();

    m_port.setPortName(portName);
    // Baud rate не важливий для USB CDC, але QSerialPort вимагає значення
    m_port.setBaudRate(QSerialPort::Baud115200);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_port.errorString());
        return false;
    }

    m_buf.clear();
    m_synced = false;
    return true;
}

void PanelLink::close()
{
    if (m_port.isOpen())
        m_port.close();
}

bool PanelLink::isOpen() const
{
    return m_port.isOpen();
}

QString PanelLink::lastError() const
{
    return m_port.errorString();
}

void PanelLink::sendData(float cpuLoad, float ramUsage, float cpuTemp)
{
    if (!m_port.isOpen())
        return;

    PcData d;
    d.cpuLoad  = cpuLoad;
    d.ramUsage = ramUsage;
    d.cpuTemp  = cpuTemp;

    QByteArray packet;
    packet.append(static_cast<char>(SYNC_BYTE));
    packet.append(reinterpret_cast<const char *>(&d), sizeof(PcData));
    m_port.write(packet);
}

void PanelLink::onReadyRead()
{
    m_buf.append(m_port.readAll());

    while (true) {
        if (!m_synced) {
            // Шукаємо sync-байт
            int idx = -1;
            for (int i = 0; i < m_buf.size(); ++i) {
                if (static_cast<uint8_t>(m_buf[i]) == SYNC_BYTE) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0) {
                m_buf.clear();
                return;
            }
            m_buf.remove(0, idx + 1); // відкидаємо все до sync включно
            m_synced = true;
        }

        // Чекаємо повний пакет
        if (m_buf.size() < MK_PACKET_LEN)
            return;

        MkData data;
        std::memcpy(&data, m_buf.constData(), sizeof(MkData));
        m_buf.remove(0, MK_PACKET_LEN);
        m_synced = false;

        emit dataReceived(data);
    }
}

void PanelLink::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    if (error == QSerialPort::ResourceError)
        emit connectionLost();
    else
        emit errorOccurred(m_port.errorString());
}
