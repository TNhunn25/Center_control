#include "ethernet_handler.h"

EthernetUDPHandler::EthernetUDPHandler() : lastSend(0) {}

void EthernetUDPHandler::begin()
{
    Ethernet.init(10);

    uint64_t chipid = ESP.getEfuseMac();
    uint8_t mac[6];
    mac[0] = (chipid >> 40) & 0xFF;
    mac[1] = (chipid >> 32) & 0xFF;
    mac[2] = (chipid >> 24) & 0xFF;
    mac[3] = (chipid >> 16) & 0xFF;
    mac[4] = (chipid >> 8) & 0xFF;
    mac[5] = (chipid >> 0) & 0xFF;

    IPAddress ip(192, 168, 1, 100);
    IPAddress dns(8, 8, 8, 8);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

    Ethernet.begin(mac, ip, dns, gateway, subnet);
    udp.begin(port);

    Serial.print("{\"status\":\"ready\",\"ip\":\"");
    Serial.print(Ethernet.localIP());
    Serial.println("\",\"role\":\"w5500_master\"}");
}

void EthernetUDPHandler::handleReceive()
{
    if (udp.parsePacket() > 0)
    {
        char packetBuffer[256];
        int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0)
        {
            packetBuffer[len] = '\0';
            Serial.print("Received from Node (");
            Serial.print(udp.remoteIP());
            Serial.print("): ");
            Serial.println(packetBuffer);
        }
    }
}

bool EthernetUDPHandler::sendCommand(const MistCommand &cmd)
{
    char msg[240];

    int n = snprintf(
        msg, sizeof(msg),
        "{\"unix\":%lu,\"node_id\":%d,"
        "\"duration_fan\":%u,\"duration_device\":%u,"
        "\"duration_off\":%u,"
        "\"out1\":%s,\"out2\":%s,\"out3\":%s,\"out4\":%s}\n",
        (unsigned long)cmd.unix,
        (int)cmd.node_id,
        (unsigned)cmd.duration_fan,
        (unsigned)cmd.duration_device,
        (unsigned)cmd.duration_off,
        cmd.out1 ? "true" : "false",
        cmd.out2 ? "true" : "false",
        cmd.out3 ? "true" : "false",
        cmd.out4 ? "true" : "false");

    udp.beginPacket(broadcastIP, port);
    udp.write((const uint8_t *)msg, (size_t)n);
    bool sent = udp.endPacket();

    Serial.print(F("{\"udp_sent\":"));
    Serial.print(sent ? F("true") : F("false"));
    Serial.println(F("}"));

    return sent;
}
