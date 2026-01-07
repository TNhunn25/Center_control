#ifndef ETHERNET_HANDLER_H
#define ETHERNET_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>

#include "config.h"
// #include "md5.h"
struct NodeWatch
{
    bool active = false;     // đã từng thấy node này
    bool timedOut = false;   // đã báo timeout chưa
    uint32_t lastSeenMs = 0; // lần cuối nhận packet
    IPAddress lastIP;        // ip lần cuối (optional)
    uint16_t lastPort = 0;   // port lần cuối (optional)
};
class EthernetUDPHandler
{
public:
    using UdpReceiveCallback = void (*)(const char *payload, int len, IPAddress rip, uint16_t rport);

    EthernetUDPHandler();

    // Begin đầy đủ chân (khuyến nghị)
    void begin(uint8_t csPin,
               uint8_t rstPin,
               uint8_t sckPin,
               uint8_t misoPin,
               uint8_t mosiPin,
               uint16_t listenPort = 8888);

    // Begin đơn giản: mặc định theo sơ đồ bạn gửi RST=5 CS=6 SCK=7 MISO=8 MOSI=9
    void begin();

    // void update(); // gọi trong loop() để receive UDP

    // void onReceive(UdpReceiveCallback cb);
    // bool sendResponse(IPAddress remoteIp, uint16_t remotePort, const String &payload);

    bool isLinkUp() const;

    bool sendCommand(const MistCommand &cmd);
    // void checkNodeTimeouts();

private:
    static constexpr unsigned long broadcastInterval = 3000; // 3s
    static constexpr size_t RX_BUF_SZ = 512;
    static constexpr size_t TX_BUF_SZ = 512;
    uint32_t RX_TIMEOUT_MS = 30000;
    uint32_t lastRxMs = 0;
    bool timeoutTriggered = false;
    static constexpr uint16_t MAX_NODES = 10;
    NodeWatch nodes[MAX_NODES + 1];
    IPAddress broadcastIP = IPAddress(192, 168, 80, 255);
    EthernetUDP udp;
    uint16_t port = 8888;
    unsigned long lastSend = 0;
    char rxBuf[RX_BUF_SZ];
    UdpReceiveCallback receiveCallback = nullptr;
    void handleReceive();
};

#endif
