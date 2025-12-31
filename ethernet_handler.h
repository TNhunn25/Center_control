#ifndef ETHERNET_HANDLER_H
#define ETHERNET_HANDLER_H

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "config.h"
class EthernetUDPHandler
{
public:
    EthernetUDPHandler();
    void begin();
    bool sendCommand(const MistCommand &cmd);         

private:
    static constexpr unsigned int port = 8888;
    static constexpr unsigned long broadcastInterval = 3000; // 3 giây

    IPAddress broadcastIP = IPAddress(192, 168, 1, 255);

    EthernetUDP udp;

    unsigned long lastSend = 0;

    void handleReceive(); // Xử lý gói tin đến
};

#endif