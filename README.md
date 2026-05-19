# desk-panel-monitor-fw

Прошивка STM32WB55RG для панели мониторинга рабочего места.
Подключается к Linux-машине по USB CDC ACM — дополнительные драйверы не нужны.

---

## Железо

| Компонент     | Интерфейс | Пин(ы)          | Примечание                              |
|---------------|-----------|------------------|-----------------------------------------|
| ENS160        | I2C1      | —                | eCO2 (ppm), TVOC (ppb), AQI            |
| AHT21         | I2C1      | —                | Температура (°C), влажность (%RH)      |
| SSD1306       | I2C3      | —                | OLED 128×64, монохромный               |
| LDR GL5528    | ADC1      | —                | Освещённость, 12-бит (0–4095)          |
| KY-040 CLK    | EXTI      | PA5              | Энкодер, falling edge, pull-up         |
| KY-040 DT     | GPIO_IN   | PA6              | Направление энкодера, pull-up          |
| Кнопка левая  | EXTI      | PA15             | Falling edge, pull-up, bit2 в buttons  |
| Кнопка центр  | EXTI      | PA8              | Falling edge, pull-up, bit1 в buttons  |
| Кнопка правая | EXTI      | PC13             | Falling edge, pull-up, bit0 в buttons  |
| LED красный   | GPIO_OUT  | PA4              | Активный LOW (LOW = горит)             |
| LED жёлтый    | GPIO_OUT  | PA9              | Активный LOW                           |
| LED зелёный   | GPIO_OUT  | PC12             | Активный LOW                           |
| USB CDC       | USB FS    | —                | Internal Phy, 12 Mbit/s                |

---

## Подключение к Ubuntu

Устройство появляется как `/dev/ttyACM0` (или `ttyACM1`, если занят).

```bash
# Добавить пользователя в группу (один раз, потом перелогиниться)
sudo usermod -a -G dialout $USER

# Или временно без перелогина
sudo chmod 666 /dev/ttyACM0

# Проверить что устройство есть
ls -la /dev/ttyACM*

# Быстрый просмотр сырых данных
python3 listen.py /dev/ttyACM0
```

Скорость порта не имеет значения — USB CDC игнорирует baud rate.

---

## USB-протокол

Оба направления используют одну схему: **sync-байт + бинарная структура**.

```
[0xA5] [N байт структуры]
```

Синхронизация побайтовая: ждём `0xA5`, затем читаем ровно `sizeof(struct)` байт.
После приёма полного пакета — сбрасываем состояние и ждём следующий `0xA5`.

### MCU → PC (50 Гц, каждые 20 мс)

Структура `MK_Data_t`, **21 байт** (1 sync + 20 данных):

```
Offset  Size  Type      Field           Описание
──────────────────────────────────────────────────────────────────
  0      1    uint8_t   sync            всегда 0xA5
  1      4    float     temperature     температура, °C (AHT21)
  5      4    float     humidity        влажность, %RH (AHT21)
  9      2    uint16_t  eco2            eCO2, ppm (ENS160)
 11      2    uint16_t  tvoc            TVOC, ppb (ENS160)
 13      1    uint8_t   aqi             AQI, 1–5 (ENS160)
 14      1    —         (padding)       выравнивание uint16_t
 15      2    uint16_t  light_raw       АЦП освещённости, 0–4095
 17      1    uint8_t   brightness      яркость дисплея, 0–255
 18      1    uint8_t   buttons         битмаска кнопок (см. ниже)
 19      1    int8_t    encoder_delta   приращение энкодера с прошлого пакета
 20      1    —         (padding)       выравнивание структуры до 4 байт
```

**buttons** (битмаска, сбрасывается после каждой отправки):
```
bit2 = левая кнопка  (PA15)
bit1 = центр         (PA8)
bit0 = правая кнопка (PC13)
```

**encoder_delta** — сырое приращение, сбрасывается после каждой отправки.
Чувствительность применяется на стороне утилиты.

### PC → MCU

Структура `PC_Data_t`, **13 байт** (1 sync + 12 данных):

```
Offset  Size  Type      Field           Описание
──────────────────────────────────────────────────────────────────
  0      1    uint8_t   sync            всегда 0xA5
  1      4    float     cpu_load        загрузка CPU, %
  5      4    float     ram_usage       использование RAM, %
  9      4    float     cpu_temp        температура CPU, °C
```

Частота отправки — на усмотрение утилиты, рекомендуется ~20 мс (50 Гц).
Таймаут на стороне МК: **2000 мс** — если пакетов нет дольше, считается потеря связи.

### Парсинг на Python (пример)

```python
import struct
import serial

MK_SYNC       = 0xA5
MK_PACKET_LEN = 20  # байт после sync

fmt = '<ffHHBxHBBb'  # x = padding байт
# f  temperature
# f  humidity
# H  eco2
# H  tvoc
# B  aqi
# x  padding
# H  light_raw
# B  brightness
# B  buttons
# b  encoder_delta

fields = ('temperature', 'humidity', 'eco2', 'tvoc', 'aqi',
          'light_raw', 'brightness', 'buttons', 'encoder_delta')

def read_mk_packet(ser):
    while True:
        b = ser.read(1)
        if b and b[0] == MK_SYNC:
            data = ser.read(MK_PACKET_LEN)
            if len(data) == MK_PACKET_LEN:
                values = struct.unpack(fmt, data)
                return dict(zip(fields, values))

PC_PACKET = struct.Struct('<fff')  # cpu_load, ram_usage, cpu_temp

def send_pc_packet(ser, cpu_load, ram_usage, cpu_temp):
    payload = PC_PACKET.pack(cpu_load, ram_usage, cpu_temp)
    ser.write(bytes([MK_SYNC]) + payload)
```

### Парсинг на Qt / C++ (пример)

```cpp
// Та же структура что в прошивке — просто memcpy после sync
#pragma pack(push, 1)
struct MK_Data {
    float    temperature;
    float    humidity;
    uint16_t eco2;
    uint16_t tvoc;
    uint8_t  aqi;
    uint8_t  _pad;
    uint16_t light_raw;
    uint8_t  brightness;
    uint8_t  buttons;
    int8_t   encoder_delta;
    uint8_t  _pad2;
};
#pragma pack(pop)
// static_assert(sizeof(MK_Data) == 20);
```

---

## Индикация LED

| Состояние                       | Красный | Жёлтый     | Зелёный |
|---------------------------------|---------|------------|---------|
| Норма                           | —       | —          | горит   |
| Нет связи с утилитой (>2 с)     | —       | горит      | —       |
| Warning: cpu_temp≥70°C или CO2≥1000 ppm | — | мигает 1 Гц | —  |
| Критично: cpu_temp≥85°C или CO2≥2000 ppm | мигает 2 Гц | — | — |
| Краш МК (HardFault / stack overflow) | горит | —       | —       |

При краше МК LED устанавливается прямо из обработчика прерывания/хука и остаётся постоянно.

---

## RTOS-задачи

| Задача        | Приоритет   | Период  | Что делает                                          |
|---------------|-------------|---------|-----------------------------------------------------|
| Task_USB_RX   | Highest     | —       | Блокируется на очереди, парсит пакеты PC→MCU        |
| Task_USB_TX   | High        | 20 мс   | Сериализует MK_Data, шлёт по USB                   |
| Task_Display  | Medium      | 500 мс  | Обновляет SSD1306                                   |
| Task_Light    | Medium-low  | 300 мс  | АЦП, энкодер, яркость дисплея                      |
| Task_LED      | Low         | 50 мс   | Управляет тремя LED по состоянию системы            |
| Task_Sensor   | Lowest      | ~2000 мс| Читает AHT21 → ENS160, пишет в MK_Data             |

---

## Зависимости утилиты (Ubuntu)

```bash
# Python (для тестов)
pip install pyserial

# Qt (для основной утилиты)
sudo apt install qt6-base-dev qt6-serialport-dev
# или Qt5:
sudo apt install qtbase5-dev libqt5serialport5-dev
```

CMakeLists.txt для Qt-утилиты должен включать `Qt::SerialPort`.
