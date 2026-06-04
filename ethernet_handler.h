#ifndef ETHERNET_HANDLER_H
#define ETHERNET_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"
#include "net_config.h"

struct NodeWatch
{
    bool active = false;     // Da tung thay node nay
    bool timedOut = false;   // Da bao timeout hay chua
    uint32_t lastSeenMs = 0; // Lan cuoi nhan packet
    IPAddress lastIP;        // IP lan cuoi (optional)
    uint16_t lastPort = 0;   // Port lan cuoi (optional)
};

class EthernetUDPHandler
{
public:
    EthernetUDPHandler();

    // Khoi tao Ethernet/UDP voi day du chan SPI va cong lang nghe chi dinh.
    void begin(uint8_t csPin,
               uint8_t rstPin,
               uint8_t sckPin,
               uint8_t misoPin,
               uint8_t mosiPin,
               uint16_t listenPort = 8888);

    // Khoi tao nhanh voi mapping chan mac dinh cua phan cung hien tai.
    void begin();

    // Poll UDP de nhan packet moi trong loop().
    void update();

    // Dong goi va broadcast lenh dieu khien xuong cac node qua UDP.
    bool sendCommand(const IoCommand &cmd);

    // Kiem tra node nao da qua thoi gian khong gui du lieu de bao timeout.
    void checkNodeTimeouts();

    // Ap dung lai cau hinh IP tinh moi cho chip Ethernet ngay luc runtime.
    bool applyStaticConfig(const EthStaticConfig &cfg);

private:
    static constexpr unsigned long broadcastInterval = 3000; // 3s
    static constexpr size_t RX_BUF_SZ = 512;
    static constexpr size_t TX_BUF_SZ = 512;
    uint32_t RX_TIMEOUT_MS = 5UL * 60UL * 1000UL;
    uint32_t lastRxMs = 0;
    bool timeoutTriggered = false;
    static constexpr uint16_t MAX_NODES = 10;
    NodeWatch nodes[MAX_NODES + 1];
    IPAddress broadcastIP = IPAddress(192, 168, 1, 255);
    EthernetUDP udp;
    uint16_t port = 8888;
    unsigned long lastSend = 0;
    char rxBuf[RX_BUF_SZ];

    // Nhan, parse va cap nhat trang thai node tu mot goi UDP den.
    void handleReceive();

    // Khoi dong lai Ethernet va UDP bang cau hinh IP tinh hien tai.
    bool startEthernet(const EthStaticConfig &cfg);

    // Tinh dia chi broadcast tu IP va subnet mask.
    IPAddress calcBroadcast(const IPAddress &ip, const IPAddress &mask);

    uint8_t csPin_ = 0;
    uint8_t rstPin_ = 0;
    uint8_t sckPin_ = 0;
    uint8_t misoPin_ = 0;
    uint8_t mosiPin_ = 0;
    bool pinsReady_ = false;
    uint8_t mac_[6] = {0};
    bool macReady_ = false;
    bool noLinkLogged_ = false;
};

#endif
