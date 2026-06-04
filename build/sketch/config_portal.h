#line 1 "D:\\Power_Central_v4\\config_portal.h"
#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "ethernet_handler.h"
#include "mqtt_handler.h"
#include "net_config.h"
#include "led_status.h"
#include "wifi_handler.h"

extern LedStatus ledstatus;
class CentralController;

class ConfigPortal
{
public:
    ConfigPortal();
    void begin(uint8_t buttonPin, EthernetUDPHandler *ethHandler, MqttHandler *mqttHandler,
               LedStatus *ledStatus, CentralController *centralController);
    void update();
    bool isActive() const;
    void handleWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    // Public control wrappers to allow external code to open/close/toggle the portal
    void openPortal();
    void closePortal();
    void togglePortal();

private:
    bool authenticatePortal();
    bool authenticateOta();
    void initPortal(uint8_t buttonPin, EthernetUDPHandler *ethHandler);
    void startPortal();
    void stopPortal();
    void handleButton();
    void handleWsText(uint8_t num, const uint8_t *payload, size_t length);
    void makePortalToken();
    bool isPortalTokenValid(const char *token) const;
    bool isClientSessionTrusted(uint8_t num) const;
    void setClientSessionTrusted(uint8_t num, bool trusted);
    bool isClientAuthenticated(uint8_t num) const;
    void setClientAuthenticated(uint8_t num, bool authenticated);
    void sendAuthRequired(uint8_t num, const char *message = nullptr);
    void sendAuthOk(uint8_t num);
    void sendConfig(uint8_t num);
    void sendAck(uint8_t num, bool ok, const char *message);

private:
    bool loginClients_[8] = {};
    bool loggedIn_ = false;
    WebServer server_;
    WebSocketsServer ws_;
    EthernetUDPHandler *ethHandler_ = nullptr;
    MqttHandler *mqttHandler_ = nullptr;
    CentralController *centralController_ = nullptr;
    uint8_t buttonPin_ = 255;
    bool active_ = false;
    bool pendingStop_ = false;
    bool buttonWasDown_ = false;
    bool pressHandled_ = false;
    uint32_t pressStartMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    uint32_t stopAtMs_ = 0;
    uint8_t stopClientId_ = 0;
    uint32_t trustedSessionClients_ = 0;
    uint32_t authenticatedClients_ = 0;
    char portalToken_[17] = {};
};
