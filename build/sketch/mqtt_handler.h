#line 1 "D:\\Power_Central_v4\\mqtt_handler.h"
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "net_config.h"

class MqttHandler
{
public:
    using CommandCallback = void (*)(const IoCommand &cmd);
    enum class TransportMode : uint8_t
    {
        Auto = 0,
        EthernetOnly,
        WiFiOnly
    };
    enum class ActiveNetwork : uint8_t
    {
        None = 0,
        Ethernet,
        WiFi
    };

    MqttHandler();
    // Khoi tao client MQTT va nap cau hinh hien tai.
    void begin();
    // Duy tri ket noi MQTT, dong bo NTP va xu ly reconnect.
    void update();
    // Dang ky callback nhan lenh dieu khien tu broker.
    void onCommandReceived(CommandCallback cb);
    // Tai nap cau hinh MQTT tu runtime config.
    bool reloadConfig();
    // Kiem tra ca Ethernet va MQTT da san sang hay chua.
    bool isConnected();
    // Gui goi trang thai hien tai cua thiet bi len topic status.
    bool publishStatus(bool autoMode, const bool outputs[OUT_COUNT], uint8_t ui8_power_state, uint8_t outputCount);

    // Gui trang thai online/offline len topic connection.
    bool publishOnline(bool online = true);
    // Nhan callback thuan cua PubSubClient va xu ly payload.
    void handleMessage(char *topic, uint8_t *payload, unsigned int length);
    // Tu dong uu tien Ethernet, fallback WiFi neu co.
    void setAutoTransport();
    // Chi su dung Ethernet cho MQTT.
    void setPreferredEthernet();
    // Chi su dung WiFi cho MQTT.
    void setPreferredWiFi();
    // Lay che do transport hien tai.
    TransportMode getTransportMode() const;
    // Lay duong mang MQTT dang su dung thuc te.
    ActiveNetwork getActiveNetwork() const;

private:
    enum class NetworkTransport : uint8_t
    {
        None = 0,
        Ethernet,
        WiFi
    };

    EthernetClient ethClient_;
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;
    CommandCallback commandCallback_ = nullptr;
    MqttConfig config_{};
    uint32_t lastReconnectAttemptMs_ = 0;
    uint32_t reconnectIntervalMs_ = 2000;
    uint32_t preferWiFiUntilMs_ = 0;
    bool lastConnectionState_ = false;
    bool connectionStateKnown_ = false;
    int lastClientState_ = MQTT_DISCONNECTED;
    bool lastEthernetLinkUp_ = false;
    bool lastWiFiLinkUp_ = false;
    NetworkTransport activeTransport_ = NetworkTransport::None;
    TransportMode transportMode_ = TransportMode::Auto;

    // Mo ket noi toi broker va subscribe topic command.
    bool connectBroker();
    // Dong/mo trang thai MQTT khi lien ket mang thay doi.
    void handleNetworkLinkState(bool ethernetUp, bool wifiUp);
    // Chi log khi state MQTT thuc su thay doi.
    void logConnectionStateIfChanged();
    // Chon ket noi uu tien Ethernet, neu khong co thi dung WiFi.
    NetworkTransport selectActiveTransport() const;
    // Gan network client phu hop cho PubSubClient theo transport hien tai.
    void bindActiveClient(NetworkTransport transport);
    // Tam thoi uu tien WiFi trong mot khoang thoi gian neu Ethernet co link nhung khong len broker.
    bool shouldPreferWiFiFailover(uint32_t now) const;
    // Kiem tra co bat ky duong mang nao dang san sang hay khong.
    bool isNetworkAvailable() const;
    // Tao topic connection dua tren UUID phan cung.
    const char *getOnlineTopic();
    // Xac thuc topic co dung UUID cua thiet bi hay khong.
    bool isTopicBoundToDevice(const char *topic) const;
    // Parse JSON command thanh cau truc IoCommand.
    bool parseCommandPayload(const uint8_t *payload, unsigned int length, IoCommand &cmd);
    // Hook xac thuc Auth, hien dang cho phep payload khong bat buoc.
    bool verifyAuthIfPresent(const JsonDocument &doc, const char *topicDeviceId = nullptr);
    // Serialize JSON va publish len topic dich.
    bool publishJson(const char *topic, const JsonDocument &doc, bool retained);
    // Dam bao client da subscribe lai topic command sau reconnect.
    bool ensureSubscription();
    // Kiem tra da den nguong reconnect hay chua.
    bool shouldAttemptReconnect(uint32_t now) const;
    // Ghi log payload dau vao de debug command.
    void logIncomingPayload(const char *topic, const uint8_t *payload, unsigned int length) const;
    char onlineTopic_[96] = {};
    bool subscribed_ = false;
};

#endif
