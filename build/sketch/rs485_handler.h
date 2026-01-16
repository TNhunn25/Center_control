#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\phunsuong\\master\\master\\rs485_handler.h"
// File: rs485_handler.h
// Header cho lớp Rs485Handler - Master gửi lệnh xuống node qua RS485
// Chạy song song với EthernetUDPHandler

#ifndef RS485_HANDLER_H
#define RS485_HANDLER_H

#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUDP.h>
#include <ArduinoJson.h>
#include "config.h"

class Rs485Handler
{
public:
    Rs485Handler(HardwareSerial &serial,
                 uint8_t rxPin,
                 uint8_t txPin,
                 uint32_t baudRate);

    void begin();  // Khởi tạo RS485 

    // Gửi lệnh xuống node qua RS485 (giao thức giống Ethernet)
    bool sendCommand(const MistCommand &cmd);

private:
    HardwareSerial &rs485;
    uint8_t rxPin;
    uint8_t txPin;
    uint32_t baudRate;

    static const uint16_t RX_BUF_SZ = 512;
    static const uint16_t TX_BUF_SZ = 512;
    char rxBuf[RX_BUF_SZ];

};

#endif // RS485_HANDLER_H