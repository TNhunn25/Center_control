#line 1 "D:\\Power_Central_v4\\ethernet_handler.cpp"
#include "ethernet_handler.h"
#include "config.h"

EthernetUDPHandler::EthernetUDPHandler()
{
    memset(rxBuf, 0, sizeof(rxBuf));
}

namespace
{
    bool isMacZero(const uint8_t mac[6])
    {
        for (int i = 0; i < 6; i++)
        {
            if (mac[i] != 0)
                return false;
        }
        return true;
    }

    void printIpField(const __FlashStringHelper *label, const IPAddress &ip)
    {
        Serial.print(label);
        Serial.println(ip);
    }
}

void EthernetUDPHandler::begin()
{
    Serial.println(F("[ETH] begin() default pins"));
    begin(6, 5, 7, 8, 9, 8888);
}

void EthernetUDPHandler::begin(uint8_t csPin,
                               uint8_t rstPin,
                               uint8_t sckPin,
                               uint8_t misoPin,
                               uint8_t mosiPin,
                               uint16_t listenPort)
{
    Serial.println(F("[ETH] begin() enter"));
    port = listenPort;
    csPin_ = csPin;
    rstPin_ = rstPin;
    sckPin_ = sckPin;
    misoPin_ = misoPin;
    mosiPin_ = mosiPin;
    pinsReady_ = true;

    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, LOW);
    delay(100);
    digitalWrite(rstPin, HIGH);
    delay(300);
    Serial.println(F("[ETH] reset done"));

    SPI.begin(sckPin, misoPin, mosiPin, csPin);
    Ethernet.init(csPin);
    Serial.println(F("[ETH] SPI/Ethernet.init done"));

    if (!macReady_ || isMacZero(mac_))
    {
        memcpy(mac_, ETH_MAC, sizeof(mac_));
        macReady_ = true;
    }

    EthStaticConfig cfg;
    loadEthStaticConfig(cfg);
    Serial.println(F("[ETH] config loaded"));
    startEthernet(cfg);
}

void EthernetUDPHandler::update()
{
    handleReceive();
}

bool EthernetUDPHandler::applyStaticConfig(const EthStaticConfig &cfg)
{
    if (!pinsReady_ || !macReady_)
        return false;
    if (!isValidEthStaticConfig(cfg))
        return false;

    return startEthernet(cfg);
}

void EthernetUDPHandler::handleReceive()
{
    static uint32_t lastLinkCheck = 0;
    uint32_t now = millis();

    if (now - lastLinkCheck >= 1000)
    {
        lastLinkCheck = now;

        bool currentLinkStatus = (Ethernet.linkStatus() == LinkON);
        // bool currentwifi = isWiFiConnected;
        if (currentLinkStatus != has_connect_link)
        {
            has_connect_link = currentLinkStatus;
            Serial.println(has_connect_link ? F("[ETH] Link UP") : F("[ETH] Link DOWN"));
            if (has_connect_link)
                noLinkLogged_ = false;
        }

        if (!currentLinkStatus && !noLinkLogged_)
        {
            noLinkLogged_ = true;
            Serial.println(F("[ETH] Current link status: DOWN"));
            Serial.println(F("{\"error\":\"No link ethernet\"}"));
        }
    }

    if (Ethernet.linkStatus() != LinkON)
        return;

    int packetSize = udp.parsePacket();
    if (packetSize <= 0)
        return;
    Serial.print(F("[ETH] UDP packet received, size="));
    Serial.println(packetSize);
    if(packetSize > sizeof(rxBuf)-1 )
    {
        Serial.print(F("qua gioi han, size="));
        Serial.println(packetSize);
        return;

    }

    memset(rxBuf, 0, sizeof(rxBuf));
    int len = udp.read((uint8_t *)rxBuf, (int)sizeof(rxBuf) - 1);
    if (len <= 0)
        return;

    if (len > 0 && rxBuf[len - 1] == '\r')
        rxBuf[len - 1] = '\0';
    else
        rxBuf[len] = '\0';

    StaticJsonDocument<768> doc;
    DeserializationError err = deserializeJson(doc, rxBuf);
    if (err)
        return;

    int nodeId = doc["data"]["node_id"] | 0;
    if (nodeId <= 0 || nodeId > MAX_NODES)
        return;
    Serial.print(F("[ETH] Valid node heartbeat from node_id="));
    Serial.print(nodeId);
    Serial.print(F(" ip="));
    Serial.println(udp.remoteIP());

    nodes[nodeId].active = true;
    nodes[nodeId].lastSeenMs = now;
    nodes[nodeId].lastIP = udp.remoteIP();
    nodes[nodeId].lastPort = udp.remotePort();

    if (nodes[nodeId].timedOut)
    {
        nodes[nodeId].timedOut = false;
        Serial.print(F("[ETH][RECOVER] node_id="));
        Serial.print(nodeId);
        Serial.println(F(" is back"));
    }

    lastRxMs = now;
    timeoutTriggered = false;
}

void EthernetUDPHandler::checkNodeTimeouts()
{
    uint32_t now = millis();

    for (int id = 1; id <= (int)MAX_NODES; id++)
    {
        if (!nodes[id].active)
            continue;

        if (!nodes[id].timedOut && (now - nodes[id].lastSeenMs >= RX_TIMEOUT_MS))
        {
            nodes[id].timedOut = true;

            Serial.print(F("[ETH][TIMEOUT] node_id="));
            Serial.print(id);
            Serial.print(F(" lastIP="));
            Serial.print(nodes[id].lastIP);
            Serial.print(F(" lastSeen="));
            Serial.println(now - nodes[id].lastSeenMs);
        }
    }
}

bool EthernetUDPHandler::sendCommand(const IoCommand &cmd)
{
    if (Ethernet.linkStatus() != LinkON)
    {
        has_connect_link = false;
        if (!noLinkLogged_)
        {
            noLinkLogged_ = true;
            Serial.println(F("[ETH] Link OFF"));
            Serial.println(F("{\"error\":\"No link ethernet\"}"));
        }
        return false;
    }

    StaticJsonDocument<1024> dataDoc;
    if (cmd.opcode == IO_COMMAND)
    {
        dataDoc["out1"] = cmd.out1 ? 0 : 1;
        dataDoc["out2"] = cmd.out2 ? 0 : 1;
        dataDoc["out3"] = cmd.out3 ? 0 : 1;
        dataDoc["out4"] = cmd.out4 ? 0 : 1;
    }
    else
    {
        return false;
    }

    String dataJson;
    dataJson.reserve(256);
    serializeJson(dataDoc, dataJson);

    String combined;
    combined.reserve(48 + dataJson.length());
    combined = String(cmd.opcode) + dataJson + String(cmd.unix) + PRIVATE_KEY;

    String auth = calculateMD5(combined);

    StaticJsonDocument<512> doc;
    doc["opcode"] = cmd.opcode;
    doc["data"] = dataDoc;
    doc["time"] = cmd.unix;
    doc["auth"] = auth;

    char msg[TX_BUF_SZ];
    size_t n = serializeJson(doc, msg, sizeof(msg));
    if (n == 0 || n >= sizeof(msg))
        return false;
    Serial.print(F("[ETH] Broadcasting command opcode="));
    Serial.print(cmd.opcode);
    Serial.print(F(" port="));
    Serial.println(port);

    udp.beginPacket(broadcastIP, port);
    udp.write((const uint8_t *)msg, n);
    return udp.endPacket() == 1;
}

bool EthernetUDPHandler::startEthernet(const EthStaticConfig &cfg)
{
    if (!macReady_ || isMacZero(mac_))
        return false;

    Serial.println(F("[ETH] Applying static network config"));
    printIpField(F("[ETH] IP      : "), cfg.ip);
    printIpField(F("[ETH] MASK    : "), cfg.mask);
    printIpField(F("[ETH] GATEWAY : "), cfg.gateway);
    printIpField(F("[ETH] DNS1    : "), cfg.dns1);
    printIpField(F("[ETH] DNS2    : "), cfg.dns2);

    udp.stop();
    Ethernet.begin(mac_, cfg.ip, cfg.dns1, cfg.gateway, cfg.mask);
    delay(200);

    broadcastIP = calcBroadcast(cfg.ip, cfg.mask);
    printIpField(F("[ETH] BCAST   : "), broadcastIP);

    has_connect_link = (Ethernet.linkStatus() == LinkON);
    Serial.println(has_connect_link ? F("[ETH] Current link status: UP") : F("[ETH] Current link status: DOWN"));

    if (!has_connect_link)
    {
        noLinkLogged_ = true;
        Serial.println(F("{\"error\":\"No link ethernet\"}"));
        Serial.println(F("[ETH][DIAG] Check LAN cable, switch port, and W5500 SPI/power."));
    }
    else
    {
        noLinkLogged_ = false;
    }

    if (udp.begin(port))
    {
        Serial.print(F("{\"status\":\"ready\",\"ip\":\""));
        Serial.print(Ethernet.localIP());
        Serial.print(F("\",\"port\":"));
        Serial.print(port);
        Serial.println(F(",\"role\":\"w5500_master\"}"));
        Serial.println(F("[ETH][DIAG] UDP listener started successfully."));
        return true;
    }

    Serial.println(F("{\"error\":\"UDP begin failed\"}"));
    Serial.println(F("[ETH][DIAG] Ethernet is up but UDP socket could not open on listen port."));
    return false;
}

IPAddress EthernetUDPHandler::calcBroadcast(const IPAddress &ip, const IPAddress &mask)
{
    IPAddress bcast;
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t inv = (uint8_t)(0xFF ^ mask[i]);
        bcast[i] = (uint8_t)(ip[i] | inv);
    }
    return bcast;
}
