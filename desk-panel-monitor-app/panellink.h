#pragma once

#include <QObject>
#include <QSerialPort>
#include <QByteArray>

static constexpr uint8_t SYNC_BYTE       = 0xA5;
static constexpr int     MK_PACKET_LEN   = 20;  // байт після sync
static constexpr int     PC_PACKET_LEN   = 12;  // байт після sync

#pragma pack(push, 1)
struct MkData {
    float    temperature;
    float    humidity;
    uint16_t eco2;
    uint16_t tvoc;
    uint8_t  aqi;
    uint8_t  _pad;
    uint16_t lightRaw;
    uint8_t  brightness;
    uint8_t  buttons;
    int8_t   encoderDelta;
    uint8_t  _pad2;
};

struct PcData {
    float cpuLoad;
    float ramUsage;
    float cpuTemp;
};
#pragma pack(pop)

static_assert(sizeof(MkData) == MK_PACKET_LEN, "MkData size mismatch");
static_assert(sizeof(PcData) == PC_PACKET_LEN, "PcData size mismatch");

class PanelLink : public QObject
{
    Q_OBJECT

public:
    explicit PanelLink(QObject *parent = nullptr);

    bool open(const QString &portName);
    void close();
    bool isOpen() const;

    QString lastError() const;

public slots:
    void sendData(float cpuLoad, float ramUsage, float cpuTemp);

signals:
    void dataReceived(MkData data);
    void connectionLost();
    void errorOccurred(QString message);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort m_port;
    QByteArray  m_buf;
    bool        m_synced = false;
};
