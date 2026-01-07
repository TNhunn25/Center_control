#ifndef RS485_HANDLER_H
#define RS485_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"

extern const String SECRET_KEY;

class RS485Handler
{
public:
    RS485Handler();

    // Khởi tạo cổng RS485 với chân RX/TX và DE/RE điều khiển driver.
    void begin(HardwareSerial &serial,
               uint32_t baud,
               int8_t rxPin,
               int8_t txPin,
               int8_t dePin = -1,
               int8_t rePin = -1);

    // Đọc dữ liệu nhận từ RS485 (gọi trong loop).
    void update();
    // Gửi lệnh MistCommand theo định dạng JSON + auth.
    bool sendCommand(const MistCommand &cmd);
    // Kiểm tra đã được begin hay chưa.
    bool isReady() const;

private:
    static constexpr size_t RX_BUF_SZ = 512;
    static constexpr size_t TX_BUF_SZ = 512;

    HardwareSerial *serialPort = nullptr;
    int8_t dePin = -1;
    int8_t rePin = -1;

    char rxBuf[RX_BUF_SZ];
    size_t rxLen = 0;

    // Bật/tắt chế độ truyền cho driver RS485.
    void setTransmit(bool enable);
    // Xử lý một dòng dữ liệu nhận được.
    void handleLine(const char *line);
};

inline RS485Handler::RS485Handler()
{
    memset(rxBuf, 0, sizeof(rxBuf));
}

inline void RS485Handler::begin(HardwareSerial &serial,
                                uint32_t baud,
                                int8_t rxPin,
                                int8_t txPin,
                                int8_t driverEnablePin,
                                int8_t receiverEnablePin)
{
    // Lưu cấu hình phần cứng RS485.
    serialPort = &serial;
    dePin = driverEnablePin;
    rePin = receiverEnablePin;

    // Nếu dùng chân DE/RE thì đặt mặc định ở chế độ nhận.
    if (dePin >= 0)
    {
        pinMode(dePin, OUTPUT);
        digitalWrite(dePin, LOW);
    }
    if (rePin >= 0)
    {
        pinMode(rePin, OUTPUT);
        digitalWrite(rePin, LOW);
    }

    // Khởi tạo UART với chân RX/TX tùy chỉnh.
    serialPort->begin(baud, SERIAL_8N1, rxPin, txPin);
}

inline bool RS485Handler::isReady() const
{
    return serialPort != nullptr;
}

inline void RS485Handler::setTransmit(bool enable)
{
    // Điều khiển hướng truyền/nhận của driver RS485.
    if (dePin >= 0)
        digitalWrite(dePin, enable ? HIGH : LOW);
    if (rePin >= 0)
        digitalWrite(rePin, enable ? HIGH : LOW);
}

inline void RS485Handler::update()
{
    if (!serialPort)
        return;

    // Thu thập dữ liệu theo dòng, kết thúc bằng '\n'.
    while (serialPort->available() > 0)
    {
        char c = static_cast<char>(serialPort->read());
        if (c == '\n')
        {
            rxBuf[rxLen] = '\0';
            handleLine(rxBuf);
            rxLen = 0;
        }
        else if (rxLen + 1 < sizeof(rxBuf))
        {
            rxBuf[rxLen++] = c;
        }
        else
        {
            rxLen = 0;
        }
    }
}

inline void RS485Handler::handleLine(const char *line)
{
    if (!line || line[0] == '\0')
        return;

    // TODO: parse JSON phản hồi từ node nếu cần.
    Serial.print(F("RS485 Rx: "));
    Serial.println(line);
}

inline bool RS485Handler::sendCommand(const MistCommand &cmd)
{
    if (!serialPort)
        return false;

    // Tạo payload dữ liệu theo opcode.
    StaticJsonDocument<256> dataDoc;

    swith (cmd.opcode)
    // if (cmd.opcode == 1)
    case MIST_COMMAND:
    {
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    break;
    // else if (cmd.opcode == 2)
    case IO_COMMAND:
    
        dataDoc["out1"] = cmd.out1 ? 1 : 0;
        dataDoc["out2"] = cmd.out2 ? 1 : 0;
        dataDoc["out3"] = cmd.out3 ? 1 : 0;
        dataDoc["out4"] = cmd.out4 ? 1 : 0;

    
    break;
    default:
    // else
    // {
        return false;
    // }

    String dataJson;
    dataJson.reserve(256);
    serializeJson(dataDoc, dataJson);

    String combined;
    combined.reserve(64 + dataJson.length());
    combined = String(cmd.id_des) + String(cmd.opcode) + dataJson + String(cmd.unix) + SECRET_KEY;

    String auth = calculateMD5(combined);

    // Tạo gói JSON hoàn chỉnh.
    StaticJsonDocument<512> doc;
    doc["id_des"] = cmd.id_des;
    doc["opcode"] = cmd.opcode;
    doc["data"] = dataDoc;
    doc["time"] = cmd.unix;
    doc["auth"] = auth;

    char msg[TX_BUF_SZ];
    size_t n = serializeJson(doc, msg, sizeof(msg));
    if (n == 0 || n >= sizeof(msg))
        return false;

    // Chuyển sang chế độ truyền, gửi gói, rồi quay về nhận.
    setTransmit(true);
    serialPort->write(reinterpret_cast<const uint8_t *>(msg), n);
    serialPort->write('\n');
    serialPort->flush();
    setTransmit(false);

    return true;
}

#endif
