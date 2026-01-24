#include "ethernet_handler.h"
#include "config.h"
#include "get_info.h"
extern GetInfoAggregator getInfo;

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
}

void EthernetUDPHandler::begin()
{
    // Mặc định theo sơ đồ của bạn:
    // RST=5, CS=6, SCK=7, MISO=8, MOSI=9, port=8888
    begin(6, 5, 7, 8, 9, 8888);
}

void EthernetUDPHandler::begin(uint8_t csPin,
                               uint8_t rstPin,
                               uint8_t sckPin,
                               uint8_t misoPin,
                               uint8_t mosiPin,
                               uint16_t listenPort)
{
    port = listenPort;
    //-------------
    csPin_ = csPin; // neww
    rstPin_ = rstPin;
    sckPin_ = sckPin;
    misoPin_ = misoPin;
    mosiPin_ = mosiPin;
    pinsReady_ = true;
    //-------------

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
    if (!macReady_ || isMacZero(mac_))
    {
        uint64_t chipid = ESP.getEfuseMac();
        for (int i = 0; i < 6; i++)
        {
            mac_[i] = (chipid >> (40 - i * 8)) & 0xFF;
        }
        macReady_ = true;
    }

    EthStaticConfig cfg;
    loadEthStaticConfig(cfg);
    startEthernet(cfg);
    //-------------

    /*
    // IP tĩnh
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
    */
}

void EthernetUDPHandler::update()
{

    handleReceive();
}

//----------------------------
bool EthernetUDPHandler::applyStaticConfig(const EthStaticConfig &cfg)
{
    if (!pinsReady_ || !macReady_)
        return false;
    if (!isValidEthStaticConfig(cfg))
        return false;

    return startEthernet(cfg);
}
//---------------------

// Nhận gói UDP từ node, forward ACK và ingest dữ liệu
void EthernetUDPHandler::handleReceive()
{
    // Chỉ check link mỗi 250ms
    static uint32_t lastLinkCheck = 0;
    uint32_t now = millis();

    if (now - lastLinkCheck >= 1000)
    {
        lastLinkCheck = now;

        bool currentLinkStatus = (Ethernet.linkStatus() == LinkON);
        if (currentLinkStatus != has_connect_link)
        {
            has_connect_link = currentLinkStatus;
            Serial.println(has_connect_link ? F("[ETH] Link UP") : F("[ETH] Link DOWN"));
        }
    }

    int packetSize = udp.parsePacket();
    if (packetSize <= 0)
        return;

    memset(rxBuf, 0, sizeof(rxBuf));
    int len = udp.read((uint8_t *)rxBuf, (int)sizeof(rxBuf) - 1);
    if (len <= 0)
        return;

    if (len > 0 && rxBuf[len - 1] == '\r')
        rxBuf[len - 1] = '\0';
    else
        rxBuf[len] = '\0';
    // ---- NEW: parse JSON để lấy node_id ----
    StaticJsonDocument<768> doc;
    DeserializationError err = deserializeJson(doc, rxBuf);
    if (err)
        return;
    int opcode = doc["opcode"];
    // Serial.println(rxBuf);

    // if (opcode == 104 || opcode == 105)
    if (opcode == 101 || opcode == 104 || opcode == 105) // FIXME: cần xem lại
    {
        serializeJson(doc, Serial);
        // Serial.println(rxBuf);
        Serial.println();
    }
    getInfo.ingestFromNodeDoc(doc.as<JsonObjectConst>(), udp.remoteIP(), udp.remotePort());
    int nodeId = doc["data"]["node_id"] | 0;
    if (nodeId <= 0 || nodeId > MAX_NODES)
        return;
    nodes[nodeId].active = true;
    nodes[nodeId].lastSeenMs = now;
    nodes[nodeId].lastIP = udp.remoteIP();
    nodes[nodeId].lastPort = udp.remotePort();

    // nếu trước đó timeout, giờ node hồi lại
    if (nodes[nodeId].timedOut)
    {
        nodes[nodeId].timedOut = false;
        Serial.print(F("[ETH][RECOVER] node_id="));
        Serial.print(nodeId);
        Serial.println(F(" is back"));
    }

    // nếu bạn vẫn muốn timeout global “có packet hay không”
    lastRxMs = now;
    timeoutTriggered = false;
}

void EthernetUDPHandler::checkNodeTimeouts()
{
    uint32_t now = millis();

    for (int id = 1; id <= (int)MAX_NODES; id++)
    {
        if (!nodes[id].active)
            continue; // chưa từng thấy node này

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

// Gửi lệnh xuống các node qua UDP - protocol mới
bool EthernetUDPHandler::sendCommand(const MistCommand &cmd)
{
    if (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println(F("[ETH] Link OFF"));
        return false;
    }
    // data
    StaticJsonDocument<1024> dataDoc;
    if (cmd.opcode == 1)
    { // MIST_COMMAND
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    else if (cmd.opcode == 3)
    {
    }
    else if (cmd.opcode == 4)
    {
        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            if ((cmd.motor_mask & (1u << i)) == 0)
                continue;

            JsonObject m = dataDoc.createNestedObject(String("m") + String(i + 1));
            m["run"] = cmd.motors[i].run;
            m["dir"] = cmd.motors[i].dir;
            m["speed"] = cmd.motors[i].speed;
        }
    }
    else if (cmd.opcode == 5)
    {
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

//--------------------------------------------------------

bool EthernetUDPHandler::startEthernet(const EthStaticConfig &cfg)
{
    if (!macReady_ || isMacZero(mac_))
        return false;

    udp.stop();
    Ethernet.begin(mac_, cfg.ip, cfg.dns1, cfg.gateway, cfg.mask);
    delay(200);

    broadcastIP = calcBroadcast(cfg.ip, cfg.mask);

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
        return true;
    }

    Serial.println(F("{\"error\":\"UDP begin failed\"}"));
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
