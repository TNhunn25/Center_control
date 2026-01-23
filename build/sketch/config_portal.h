#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\config_portal.h"
#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "ethernet_handler.h"
#include "net_config.h"
#include "led_status.h"
extern LedStatus ledstatus;
class ConfigPortal
{
public:
    ConfigPortal();
    void begin(uint8_t buttonPin, EthernetUDPHandler *ethHandler, LedStatus *ledStatus);
    void update();
    bool isActive() const;
    void handleWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

private:
    void initPortal(uint8_t buttonPin, EthernetUDPHandler *ethHandler);
    void startPortal();
    void stopPortal();
    void handleButton();
    void handleWsText(uint8_t num, const uint8_t *payload, size_t length);
    void sendConfig(uint8_t num);
    void sendAck(uint8_t num, bool ok, const char *message);

private:
    WebServer server_;
    WebSocketsServer ws_;
    EthernetUDPHandler *ethHandler_ = nullptr;
    // LedStatus *ledStatus_ = nullptr;
    uint8_t buttonPin_ = 255;
    bool active_ = false;
    bool pendingStop_ = false;
    bool buttonWasDown_ = false;
    bool pressHandled_ = false;
    uint32_t pressStartMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    uint32_t stopAtMs_ = 0;
    uint8_t stopClientId_ = 0;
};
