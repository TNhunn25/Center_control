// #include "ethernet_handler.h"
// #include "config.h"

// EthernetUDPHandler::EthernetUDPHandler()
// {
//     memset(rxBuf, 0, sizeof(rxBuf));
// }

// void EthernetUDPHandler::begin()
// {
//     // Mặc định theo sơ đồ của bạn:
//     // RST=5, CS=6, SCK=7, MISO=8, MOSI=9, port=8888
//     begin(6, 5, 7, 8, 9, 8888);
// }

// void EthernetUDPHandler::begin(uint8_t csPin,
//                                uint8_t rstPin,
//                                uint8_t sckPin,
//                                uint8_t misoPin,
//                                uint8_t mosiPin,
//                                uint16_t listenPort)
// {
//     port = listenPort;

//     // Reset W5500 (RSTn active-low)
//     pinMode(rstPin, OUTPUT);
//     digitalWrite(rstPin, LOW);
//     delay(100);
//     digitalWrite(rstPin, HIGH);
//     delay(300);

//     // SPI theo chân custom
//     SPI.begin(sckPin, misoPin, mosiPin, csPin);

//     Ethernet.init(csPin);

//     // MAC từ efuse
//     uint64_t chipid = ESP.getEfuseMac();
//     uint8_t mac[6];
//     for (int i = 0; i < 6; i++)
//     {
//         mac[i] = (chipid >> (40 - i * 8)) & 0xFF;
//     }

//     // IP tĩnh
//     IPAddress ip(192, 168, 80, 118);
//     IPAddress dns(8, 8, 8, 8);
//     IPAddress gateway(192, 168, 80, 255);
//     IPAddress subnet(255, 255, 255, 0);

//     Ethernet.begin(mac, ip, dns, gateway, subnet);
//     delay(200);

//     if (Ethernet.linkStatus() == LinkOFF)
//     {
//         Serial.println(F("{\"error\":\"No link ethernet\"}"));
//     }

//     if (udp.begin(port))
//     {
//         Serial.print(F("{\"status\":\"ready\",\"ip\":\""));
//         Serial.print(Ethernet.localIP());
//         Serial.print(F("\",\"port\":"));
//         Serial.print(port);
//         Serial.println(F(",\"role\":\"w5500_master\"}"));
//     }
//     else
//     {
//         Serial.println(F("{\"error\":\"UDP begin failed\"}"));
//     }
// }

// void EthernetUDPHandler::update()
// {
  
//     handleReceive();
// }

// void EthernetUDPHandler::onReceive(UdpReceiveCallback cb)
// {
//     receiveCallback = cb;
// }

// bool EthernetUDPHandler::sendResponse(IPAddress remoteIp, uint16_t remotePort, const String &payload)
// {
//     if (payload.length() == 0)
//         return false;

//     udp.beginPacket(remoteIp, remotePort);
//     udp.write((const uint8_t *)payload.c_str(), payload.length());
//     return (udp.endPacket() == 1);
// }

// bool EthernetUDPHandler::isLinkUp() const
// {
//     return Ethernet.linkStatus() != LinkOFF;
// }

// // Nhận gói UDP từ node
// void EthernetUDPHandler::handleReceive()
// {
//     if (Ethernet.linkStatus() == LinkOFF)
//     {
//         Serial.println(F("[ETH] Link OFF"));
//         // return false;
//         return;
//     }
//     int packetSize = udp.parsePacket();
//     if (packetSize <= 0)
//         return;

//     memset(rxBuf, 0, sizeof(rxBuf));
//     int len = udp.read((uint8_t *)rxBuf, (int)sizeof(rxBuf) - 1);
//     if (len <= 0)
//         return;

//     if (len > 0 && rxBuf[len - 1] == '\r')
//         rxBuf[len - 1] = '\0';
//     else
//         rxBuf[len] = '\0';

//     Serial.print(F("[NODE]==>("));
//     Serial.print(udp.remoteIP());
//     Serial.print(F(":"));
//     Serial.print(udp.remotePort());
//     Serial.print(F("): "));
//     Serial.println(rxBuf);

//      if (receiveCallback)
//         receiveCallback(rxBuf, len, udp.remoteIP(), udp.remotePort());

//     // ---- NEW: parse JSON để lấy node_id ----
//     StaticJsonDocument<512> doc;
//     DeserializationError err = deserializeJson(doc, rxBuf);
//     if (err)
//         return;

//     int nodeId = doc["data"]["node_id"] | 0;
//     if (nodeId <= 0 || nodeId > MAX_NODES)
//         return;
//     uint32_t now = millis();
//     nodes[nodeId].active = true;
//     nodes[nodeId].lastSeenMs = now;
//     nodes[nodeId].lastIP = udp.remoteIP();
//     nodes[nodeId].lastPort = udp.remotePort();

//     // nếu trước đó timeout, giờ node hồi lại
//     if (nodes[nodeId].timedOut)
//     {
//         nodes[nodeId].timedOut = false;
//         Serial.print(F("[ETH][RECOVER] node_id="));
//         Serial.print(nodeId);
//         Serial.println(F(" is back"));
//     }

//     // nếu bạn vẫn muốn timeout global “có packet hay không”
//     lastRxMs = now;
//     timeoutTriggered = false;
// }

// void EthernetUDPHandler::checkNodeTimeouts()
// {
//     uint32_t now = millis();

//     for (int id = 1; id <= (int)MAX_NODES; id++)
//     {
//         if (!nodes[id].active)
//             continue; // chưa từng thấy node này

//         if (!nodes[id].timedOut && (now - nodes[id].lastSeenMs >= RX_TIMEOUT_MS))
//         {
//             nodes[id].timedOut = true;

//             Serial.print(F("[ETH][TIMEOUT] node_id="));
//             Serial.print(id);
//             Serial.print(F(" lastIP="));
//             Serial.print(nodes[id].lastIP);
//             Serial.print(F(" lastSeen="));
//             Serial.println(now - nodes[id].lastSeenMs);
//         }
//     }
// }

// // Gửi lệnh xuống các node qua UDP - protocol mới
// bool EthernetUDPHandler::sendCommand(const MistCommand &cmd)
// {
 
//     // data
//     StaticJsonDocument<256> dataDoc;
//     if (cmd.opcode == 1)
//     { // MIST_COMMAND
//         dataDoc["node_id"] = cmd.node_id;
//         dataDoc["time_phase1"] = cmd.time_phase1;
//         dataDoc["time_phase2"] = cmd.time_phase2;
//     }
//     else
//     {
//         return false;
//     }

//     // stringify data để tính auth
//     String dataJson;
//     dataJson.reserve(256);
//     serializeJson(dataDoc, dataJson);

//     // auth: id_des + opcode + dataJson + time + SECRET_KEY
//     String combined;
//     combined.reserve(64 + dataJson.length());
//     combined = String(cmd.id_des) + String(cmd.opcode) + dataJson + String(cmd.unix) + SECRET_KEY;

//     String auth = calculateMD5(combined);

//     // full doc
//     StaticJsonDocument<512> doc;
//     doc["id_des"] = cmd.id_des;
//     doc["opcode"] = cmd.opcode;
//     doc["data"] = dataDoc;
//     doc["time"] = cmd.unix;
//     doc["auth"] = auth;

//     // serialize ra buffer
//     char msg[TX_BUF_SZ];
//     size_t n = serializeJson(doc, msg, sizeof(msg));
//     if (n == 0 || n >= sizeof(msg))
//         return false;

//     // UDP không cần '\n'
//     udp.beginPacket(broadcastIP, port);
//     udp.write((const uint8_t *)msg, n);
//     bool ok = (udp.endPacket() == 1);

//     return ok;
// }
