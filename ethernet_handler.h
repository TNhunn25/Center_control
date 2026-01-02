#ifndef ETHERNET_HANDLER_H
#define ETHERNET_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"

class EthernetUDPHandler
{
public:
    EthernetUDPHandler();
    using ReceiveCallback = void (*)(const char *payload, int len, IPAddress remoteIp, uint16_t remotePort);

    // Begin đầy đủ chân (khuyến nghị)
    void begin(uint8_t csPin,
               uint8_t rstPin,
               uint8_t sckPin,
               uint8_t misoPin,
               uint8_t mosiPin,
               uint16_t listenPort = 8888);

    // Begin đơn giản: mặc định theo sơ đồ bạn gửi RST=5 CS=6 SCK=7 MISO=8 MOSI=9
    void begin();

    void update(); // gọi trong loop() để receive UDP

    bool sendCommand(const MistCommand &cmd);
    void onReceive(ReceiveCallback cb);
    bool isLinkUp() const;

private:
    static constexpr unsigned long broadcastInterval = 3000; // 3s
    static constexpr size_t RX_BUF_SZ = 512;
    static constexpr size_t TX_BUF_SZ = 512;

    IPAddress broadcastIP = IPAddress(192, 168, 1, 255);

    EthernetUDP udp;
    uint16_t port = 8888;

    unsigned long lastSend = 0;

    char rxBuf[RX_BUF_SZ];
    ReceiveCallback receiveCallback = nullptr;

    void handleReceive();
};

#endif
