#ifndef RS485_HANDLER_H
#define RS485_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"

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

#endif