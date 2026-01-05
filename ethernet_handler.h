#ifndef ETHERNET_HANDLER_H
#define ETHERNET_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>

#include "config.h"
// #include "md5.h"

extern const String SECRET_KEY;

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

inline EthernetUDPHandler::EthernetUDPHandler()
{
    memset(rxBuf, 0, sizeof(rxBuf));
}

inline void EthernetUDPHandler::begin()
{
    // Mặc định theo sơ đồ:
    // RST=5, CS=6, SCK=7, MISO=8, MOSI=9, port=8888
    begin(6, 5, 7, 8, 9, 8888);
}

inline void EthernetUDPHandler::begin(uint8_t csPin,
                                      uint8_t rstPin,
                                      uint8_t sckPin,
                                      uint8_t misoPin,
                                      uint8_t mosiPin,
                                      uint16_t listenPort)
{
    port = listenPort;

    // Reset W5500 (RSTn active-low)
    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, LOW);
    delay(100);
    digitalWrite(rstPin, HIGH);
    delay(300);

    // SPI theo chân custom
    SPI.begin(sckPin, misoPin, mosiPin, csPin);

    Ethernet.init(csPin);

    // MAC từ efuse
    uint64_t chipid = ESP.getEfuseMac();
    uint8_t mac[6];
    for (int i = 0; i < 6; i++)
    {
        mac[i] = (chipid >> (40 - i * 8)) & 0xFF;
    }

    // IP tĩnh (giữ như bạn)
    IPAddress ip(192, 168, 1, 100);
    IPAddress dns(8, 8, 8, 8);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

    Ethernet.begin(mac, ip, dns, gateway, subnet);
    delay(200);

    if (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println(F("{\"error\":\"No link ethernet\"}"));
    }

    if (udp.begin(port))
    {
        Serial.print(F("{\"status\":\"ready\",\"ip\":\""));
        Serial.print(Ethernet.localIP());
        Serial.print(F("\",\"port\":"));
        Serial.print(port);
        Serial.println(F(",\"role\":\"w5500_master\"}"));
    }
    else
    {
        Serial.println(F("{\"error\":\"UDP begin failed\"}"));
    }
}

inline void EthernetUDPHandler::update()
{
    handleReceive();
}

inline void EthernetUDPHandler::onReceive(ReceiveCallback cb)
{
    receiveCallback = cb;
}

inline bool EthernetUDPHandler::isLinkUp() const
{
    return Ethernet.linkStatus() != LinkOFF;
}

// Nhận gói UDP từ node
inline void EthernetUDPHandler::handleReceive()
{
    int packetSize = udp.parsePacket();
    if (packetSize <= 0)
        return;

    memset(rxBuf, 0, sizeof(rxBuf));

    int len = udp.read((uint8_t *)rxBuf, (int)sizeof(rxBuf) - 1);
    if (len <= 0)
        return;

    // bỏ \r nếu có
    if (len > 0 && rxBuf[len - 1] == '\r')
        rxBuf[len - 1] = '\0';
    else
        rxBuf[len] = '\0';

    Serial.print(F("Received from Node ("));
    Serial.print(udp.remoteIP());
    Serial.print(F(":"));
    Serial.print(udp.remotePort());
    Serial.print(F("): "));
    Serial.println(rxBuf);

    if (receiveCallback)
    {
        receiveCallback(rxBuf, len, udp.remoteIP(), udp.remotePort());
    }

    // Nếu muốn parse JSON phản hồi từ node thì thêm ở đây:
    // StaticJsonDocument<512> doc;
    // if (!deserializeJson(doc, rxBuf)) { ... }
}

// Gửi lệnh xuống các node qua UDP - protocol mới
inline bool EthernetUDPHandler::sendCommand(const MistCommand &cmd)
{
    if (Ethernet.linkStatus() == LinkOFF)
        return false;

    // data
    StaticJsonDocument<256> dataDoc;

    if (cmd.opcode == 1)
    { // MIST_COMMAND
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    else if (cmd.opcode == 2)
    { // IO_COMMAND
        dataDoc["out1"] = cmd.out1 ? 1 : 0 ;
        dataDoc["out2"] = cmd.out2 ? 1 : 0 ;
        dataDoc["out3"] = cmd.out3 ? 1 : 0 ;
        dataDoc["out4"] = cmd.out4 ? 1 : 0 ;

    }
    else
    {
        return false;
    }

    // stringify data để tính auth
    String dataJson;
    dataJson.reserve(256);
    serializeJson(dataDoc, dataJson);

    // auth: id_des + opcode + dataJson + time + SECRET_KEY
    String combined;
    combined.reserve(64 + dataJson.length());
    combined = String(cmd.id_des) + String(cmd.opcode) + dataJson + String(cmd.unix) + SECRET_KEY;

    String auth = calculateMD5(combined);

    // full doc
    StaticJsonDocument<512> doc;
    doc["id_des"] = cmd.id_des;
    doc["opcode"] = cmd.opcode;
    doc["data"] = dataDoc;
    doc["time"] = cmd.unix;
    doc["auth"] = auth;

    // serialize ra buffer
    char msg[TX_BUF_SZ];
    size_t n = serializeJson(doc, msg, sizeof(msg));
    if (n == 0 || n >= sizeof(msg))
        return false;

    // UDP không cần '\n'
    udp.beginPacket(broadcastIP, port);
    udp.write((const uint8_t *)msg, n);
    bool ok = (udp.endPacket() == 1);

    return ok;
}


#endif
