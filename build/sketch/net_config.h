#line 1 "D:\\Power_Central_v4\\net_config.h"
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct EthStaticConfig
{
    IPAddress ip;
    IPAddress mask;
    IPAddress gateway;
    IPAddress dns1;
    IPAddress dns2;
};

struct MqttConfig
{
    char host[64];
    uint16_t port;
    char deviceId[32];
    char clientId[64];
    char username[32];
    char password[32];
    char statusTopic[96];
    char commandTopic[96];
};

struct WiFiConfig
{
    char ssid[32];
    char password[64];
};

struct TotpConfig
{
    bool enabled;
    char secret[33];
};

struct DeviceConfig
{
    uint8_t outputCount;
};

//mới thêm
struct PortalAuthConfig
{
    char username[32];
    char password[64];
};

EthStaticConfig defaultEthStaticConfig();
bool loadEthStaticConfig(EthStaticConfig &out);
bool saveEthStaticConfig(const EthStaticConfig &cfg);
bool isValidEthStaticConfig(const EthStaticConfig &cfg);

MqttConfig defaultMqttConfig();
void buildShortDeviceCode(char *out, size_t outSize);
bool loadMqttConfig(MqttConfig &out);
bool saveMqttConfig(const MqttConfig &cfg);
bool isValidMqttConfig(const MqttConfig &cfg);

WiFiConfig defaultWiFiConfig();
bool loadWiFiConfig(WiFiConfig &out);
bool saveWiFiConfig(const WiFiConfig &cfg);
bool isValidWiFiConfig(const WiFiConfig &cfg);

TotpConfig defaultTotpConfig();
bool loadTotpConfig(TotpConfig &out);
bool saveTotpConfig(const TotpConfig &cfg);

DeviceConfig defaultDeviceConfig();
bool loadDeviceConfig(DeviceConfig &out);
bool saveDeviceConfig(const DeviceConfig &cfg);

//mới thêm
PortalAuthConfig defaultPortalAuthConfig();
bool loadPortalAuthConfig(PortalAuthConfig &out);
bool savePortalAuthConfig(const PortalAuthConfig &cfg);
bool isValidPortalAuthConfig(const PortalAuthConfig &cfg);
