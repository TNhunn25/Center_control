#include "mqtt_handler.h"

#include <ArduinoJson.h>
#include <time.h>
#include <cstring>
#include <sys/time.h>
#include <EthernetUdp.h>
#include <WiFiUdp.h>

#include "md5.h"
#include "wifi_handler.h"

namespace
{
    MqttHandler *gMqttHandler = nullptr;
    constexpr uint32_t kCommandValidityWindowSeconds = 10UL * 60UL;
    constexpr uint32_t kStatusRequestValidityWindowSeconds = 12UL * 60UL * 60UL;
    constexpr uint32_t kMinValidUnixTime = 1700000000UL;
    // Compatibility for old dashboard builds that send uptime seconds in Time.
    constexpr bool kAllowLegacyNonUnixCommandTime = true;
    constexpr uint16_t kMqttSocketTimeoutSeconds = 8;
    constexpr uint16_t kMqttKeepAliveSeconds = 30;
    constexpr uint16_t kMqttBufferSize = 1024;
    constexpr uint16_t kNtpPort = 123;
    constexpr uint16_t kNtpLocalPort = 2390;
    constexpr size_t kNtpPacketSize = 48;
    constexpr uint32_t kNtpEpochOffset = 2208988800UL;
    constexpr uint32_t kNtpRetryIntervalMs = 15000UL;
    constexpr uint32_t kNtpRefreshIntervalMs = 6UL * 60UL * 60UL * 1000UL;
    constexpr uint32_t kNtpResponseTimeoutMs = 1200UL;
    EthernetUDP gNtpEthUdp;
    WiFiUDP gNtpWiFiUdp;
    bool gNtpEthUdpReady = false;
    bool gNtpWiFiUdpReady = false;
    uint32_t gLastNtpAttemptMs = 0;
    uint32_t gLastNtpSyncMs = 0;
    uint8_t gNtpServerIndex = 0;

    bool syncUnixTimeFromNtp(bool force);

    bool isEthernetUsableForNtp()
    {
        return Ethernet.linkStatus() == LinkON;
    }

    bool isWiFiUsableForNtp()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    const __FlashStringHelper *mqttStateLabel(int state)
    {
        switch (state)
        {
        case MQTT_CONNECTION_TIMEOUT:
            return F("connection timeout");
        case MQTT_CONNECTION_LOST:
            return F("connection lost");
        case MQTT_CONNECT_FAILED:
            return F("TCP connect failed");
        case MQTT_DISCONNECTED:
            return F("disconnected");
        case MQTT_CONNECTED:
            return F("connected");
        case MQTT_CONNECT_BAD_PROTOCOL:
            return F("bad protocol");
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return F("bad client id");
        case MQTT_CONNECT_UNAVAILABLE:
            return F("server unavailable");
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return F("bad credentials");
        case MQTT_CONNECT_UNAUTHORIZED:
            return F("unauthorized");
        default:
            return F("unknown");
        }
    }

    JsonVariantConst getDataVariant(const JsonDocument &doc)
    {
        // Ho tro ca hai key "Data" va "data" de tuong thich payload cu va moi.
        JsonVariantConst value = doc["Data"];
        if (!value.isNull())
            return value;
        return doc["data"];
    }

    String getStringField(const JsonDocument &doc, const char *primaryKey, const char *secondaryKey)
    {
        // Lay field string voi ten uu tien va ten du phong.
        if (!doc[primaryKey].isNull())
            return doc[primaryKey].as<String>();
        if (!doc[secondaryKey].isNull())
            return doc[secondaryKey].as<String>();
        return "";
    }

    String getStringField3(const JsonDocument &doc, const char *key1, const char *key2, const char *key3)
    {
        if (!doc[key1].isNull())
            return doc[key1].as<String>();
        if (!doc[key2].isNull())
            return doc[key2].as<String>();
        if (!doc[key3].isNull())
            return doc[key3].as<String>();
        return "";
    }

    int getIntField(const JsonDocument &doc, const char *primaryKey, const char *secondaryKey, int fallback)
    {
        // Lay field int voi ten uu tien va ten du phong.
        if (!doc[primaryKey].isNull())
            return doc[primaryKey].as<int>();
        if (!doc[secondaryKey].isNull())
            return doc[secondaryKey].as<int>();
        return fallback;
    }

    uint32_t normalizeUnixTime(uint64_t unixTime)
    {
        if (unixTime >= 1000000000000ULL)
            unixTime /= 1000ULL;
        return (uint32_t)unixTime;
    }

    bool hasValidSystemTime()
    {
        return time(nullptr) >= (time_t)kMinValidUnixTime;
    }

    bool isUnixTimeWithinWindow(uint32_t unixTime, uint32_t allowedDeltaSeconds, uint32_t &deltaSeconds)
    {
        if (unixTime < kMinValidUnixTime)
            return false;

        const time_t nowRaw = time(nullptr);
        if (nowRaw < (time_t)kMinValidUnixTime)
            return true;

        const uint32_t now = (uint32_t)nowRaw;
        deltaSeconds = (now >= unixTime) ? (now - unixTime) : (unixTime - now);
        return deltaSeconds <= allowedDeltaSeconds;
    }

    bool isCommandTimeValid(uint32_t unixTime, uint8_t opcode)
    {
        unixTime = normalizeUnixTime(unixTime);
        if (unixTime < kMinValidUnixTime)
        {
            if (kAllowLegacyNonUnixCommandTime && unixTime > 0)
            {
                Serial.print(F("[MQTT][DIAG] Accept legacy non-Unix command time="));
                Serial.println((unsigned long)unixTime);
                return true;
            }

            Serial.print(F("[MQTT][DIAG] Command Time is not a valid Unix timestamp: "));
            Serial.println((unsigned long)unixTime);
            return false;
        }

        const uint32_t allowedDeltaSeconds = (opcode == DASHBOARD_INFO) ? kStatusRequestValidityWindowSeconds : kCommandValidityWindowSeconds;
        uint32_t deltaSeconds = 0;
        if (isUnixTimeWithinWindow(unixTime, allowedDeltaSeconds, deltaSeconds))
            return true;

        if (hasValidSystemTime())
        {
            Serial.print(F("[MQTT][DIAG] Command time delta before NTP retry="));
            Serial.println((unsigned long)deltaSeconds);
        }

        if (!syncUnixTimeFromNtp(true))
            return false;

        deltaSeconds = 0;
        const bool validAfterSync = isUnixTimeWithinWindow(unixTime, allowedDeltaSeconds, deltaSeconds);
        if (!validAfterSync && hasValidSystemTime())
        {
            Serial.print(F("[MQTT][DIAG] Command time delta after NTP retry="));
            Serial.println((unsigned long)deltaSeconds);
        }
        return validAfterSync;
    }

    uint32_t getCurrentUnixTime()
    {
        const time_t nowRaw = time(nullptr);
        if (nowRaw < (time_t)kMinValidUnixTime)
            return 0;
        return (uint32_t)nowRaw;
    }

    void applyUnixTime(uint32_t unixTime)
    {
        timeval tv{};
        tv.tv_sec = (time_t)unixTime;
        settimeofday(&tv, nullptr);
        gLastNtpSyncMs = millis();
    }

    IPAddress getNtpServerIp()
    {
        static const IPAddress servers[] = {
            IPAddress(129, 6, 15, 28),   // time.nist.gov
            IPAddress(129, 6, 15, 29),   // time.nist.gov
            IPAddress(216, 239, 35, 0),  // time.google.com
            IPAddress(216, 239, 35, 4)   // time.google.com
        };

        const size_t serverCount = sizeof(servers) / sizeof(servers[0]);
        const IPAddress selected = servers[gNtpServerIndex % serverCount];
        gNtpServerIndex = (uint8_t)((gNtpServerIndex + 1) % serverCount);
        return selected;
    }

    bool syncUnixTimeFromNtp(bool force)
    {
        UDP *udp = nullptr;
        bool *udpReady = nullptr;

        if (isEthernetUsableForNtp())
        {
            udp = &gNtpEthUdp;
            udpReady = &gNtpEthUdpReady;
        }
        else if (isWiFiUsableForNtp())
        {
            udp = &gNtpWiFiUdp;
            udpReady = &gNtpWiFiUdpReady;
        }

        if (!udp || !udpReady)
            return false;

        const uint32_t nowMs = millis();
        const bool hasValidTime = getCurrentUnixTime() >= kMinValidUnixTime;
        if (!force)
        {
            if (!hasValidTime && (uint32_t)(nowMs - gLastNtpAttemptMs) < kNtpRetryIntervalMs)
                return false;
            if (hasValidTime && gLastNtpSyncMs != 0 && (uint32_t)(nowMs - gLastNtpSyncMs) < kNtpRefreshIntervalMs)
                return true;
        }

        gLastNtpAttemptMs = nowMs;

        if (!*udpReady)
        {
            if (!udp->begin(kNtpLocalPort))
            {
                Serial.println(F("[NTP] UDP begin failed"));
                return false;
            }
            *udpReady = true;
        }

        while (udp->parsePacket() > 0)
        {
            uint8_t discard[kNtpPacketSize];
            udp->read(discard, sizeof(discard));
        }

        const IPAddress ntpServerIp = getNtpServerIp();

        uint8_t packet[kNtpPacketSize] = {};
        packet[0] = 0x1B;

        udp->beginPacket(ntpServerIp, kNtpPort);
        udp->write(packet, sizeof(packet));
        if (!udp->endPacket())
        {
            Serial.println(F("[NTP] Request send failed"));
            return false;
        }

        const uint32_t waitStartMs = millis();
        while ((uint32_t)(millis() - waitStartMs) < kNtpResponseTimeoutMs)
        {
            const int packetSize = udp->parsePacket();
            if (packetSize >= (int)kNtpPacketSize)
            {
                udp->read(packet, sizeof(packet));
                const uint32_t secondsSince1900 =
                    ((uint32_t)packet[40] << 24) |
                    ((uint32_t)packet[41] << 16) |
                    ((uint32_t)packet[42] << 8) |
                    (uint32_t)packet[43];

                if (secondsSince1900 > kNtpEpochOffset)
                {
                    const uint32_t unixTime = secondsSince1900 - kNtpEpochOffset;
                    applyUnixTime(unixTime);
                    Serial.print(F("[NTP] Synced unixTime="));
                    Serial.println((unsigned long)unixTime);
                    return true;
                }

                Serial.println(F("[NTP] Invalid response"));
                return false;
            }
            delay(10);
        }

        Serial.println(F("[NTP] Response timeout"));
        return false;
    }

    uint32_t getNonZeroTime()
    {
        const uint32_t unixTime = getCurrentUnixTime();
        if (unixTime > 0)
            return unixTime;

        const uint32_t uptimeSeconds = millis() / 1000UL;
        return uptimeSeconds > 0 ? uptimeSeconds : 1;
    }

    const char *getUuidFromDeviceId(const char *deviceId)
    {
        if (!deviceId)
            return "";

        const char *separator = strchr(deviceId, '_');
        if (separator && separator[1] != '\0')
            return separator + 1;

        return deviceId;
    }

    bool extractUuidFromTopic(const char *topic, String &uuid)
    {
        uuid = "";
        if (!topic)
            return false;

        const char *firstSlash = strchr(topic, '/');
        if (!firstSlash || firstSlash[1] == '\0')
            return false;

        const char *secondSlash = strchr(firstSlash + 1, '/');
        if (!secondSlash || secondSlash <= firstSlash + 1)
            return false;

        uuid = String(firstSlash + 1).substring(0, secondSlash - (firstSlash + 1));
        return uuid.length() > 0;
    }

    bool topicMatchesDeviceUuid(const char *topic, const char *deviceId)
    {
        String topicUuid;
        if (!extractUuidFromTopic(topic, topicUuid))
            return false;

        return topicUuid == String(getUuidFromDeviceId(deviceId));
    }

    bool getOutputValue(JsonObjectConst dataObj, const char *lowerKey, const char *upperKey, bool &outValue)
    {
        JsonVariantConst field = dataObj[lowerKey];
        if (field.isNull())
            field = dataObj[upperKey];
        if (field.isNull())
            return false;

        if (field.is<bool>())
        {
            outValue = field.as<bool>();
            return true;
        }

        if (field.is<const char *>())
        {
            String text = field.as<String>();
            text.trim();
            if (text.equalsIgnoreCase("1") || text.equalsIgnoreCase("on") || text.equalsIgnoreCase("true"))
            {
                outValue = true;
                return true;
            }
            if (text.equalsIgnoreCase("0") || text.equalsIgnoreCase("off") || text.equalsIgnoreCase("false"))
            {
                outValue = false;
                return true;
            }
            return false;
        }

        int raw = field.as<int>();
        if (raw == 1)
        {
            outValue = true;
            return true;
        }
        if (raw == 0)
        {
            outValue = false;
            return true;
        }
        return false;
    }

    bool buildOnlinePayload(const char *deviceId, bool online, char *payload, size_t payloadSize)
    {
        if (!deviceId || !payload || payloadSize == 0)
            return false;

        StaticJsonDocument<192> doc;
        doc["Device_ID"] = deviceId;
        doc["Status"] = online ? "Online" : "Offline";
        doc["Time"] = getNonZeroTime();

        size_t len = serializeJson(doc, payload, payloadSize);
        if (len == 0 || len >= payloadSize)
        {
            payload[0] = '\0';
            return false;
        }
        return true;
    }

    uint32_t getUintField(const JsonDocument &doc, const char *primaryKey, const char *secondaryKey, uint32_t fallback)
    {
        // Lay field uint32 voi ten uu tien va ten du phong.
        if (!doc[primaryKey].isNull())
            return doc[primaryKey].as<uint32_t>();
        if (!doc[secondaryKey].isNull())
            return doc[secondaryKey].as<uint32_t>();
        return fallback;
    }

    uint32_t getUnixTimeField(const JsonDocument &doc, const char *primaryKey, const char *secondaryKey, uint32_t fallback)
    {
        if (!doc[primaryKey].isNull())
            return normalizeUnixTime(doc[primaryKey].as<uint64_t>());
        if (!doc[secondaryKey].isNull())
            return normalizeUnixTime(doc[secondaryKey].as<uint64_t>());
        return fallback;
    }

    void mqttMessageThunk(char *topic, uint8_t *payload, unsigned int length)
    {
        // Chuyen callback C-style cua PubSubClient ve dung instance MqttHandler.
        if (gMqttHandler)
            gMqttHandler->handleMessage(topic, payload, length);
    }
}

MqttHandler::MqttHandler()
    : mqttClient_()
{
}

void MqttHandler::setAutoTransport()
{
    transportMode_ = TransportMode::Auto;
}

void MqttHandler::setPreferredEthernet()
{
    transportMode_ = TransportMode::EthernetOnly;
}

void MqttHandler::setPreferredWiFi()
{
    transportMode_ = TransportMode::WiFiOnly;
}

MqttHandler::TransportMode MqttHandler::getTransportMode() const
{
    return transportMode_;
}

MqttHandler::ActiveNetwork MqttHandler::getActiveNetwork() const
{
    switch (selectActiveTransport())
    {
    case NetworkTransport::Ethernet:
        return ActiveNetwork::Ethernet;
    case NetworkTransport::WiFi:
        return ActiveNetwork::WiFi;
    default:
        return ActiveNetwork::None;
    }
}

void MqttHandler::begin()
{
    reloadConfig();
    mqttClient_.setSocketTimeout(kMqttSocketTimeoutSeconds);
    mqttClient_.setKeepAlive(kMqttKeepAliveSeconds);
    mqttClient_.setBufferSize(kMqttBufferSize);
    mqttClient_.setCallback(mqttMessageThunk);
    gMqttHandler = this;
    lastEthernetLinkUp_ = (Ethernet.linkStatus() == LinkON);
    lastWiFiLinkUp_ = (WiFi.status() == WL_CONNECTED);
    activeTransport_ = selectActiveTransport();
    bindActiveClient(activeTransport_);
    Serial.print(F("[MQTT] begin host="));
    Serial.print(config_.host);
    Serial.print(F(" port="));
    Serial.println(config_.port);
    logConnectionStateIfChanged();
}

const char *MqttHandler::getOnlineTopic()
{
    char uuid[6] = {};
    buildShortDeviceCode(uuid, sizeof(uuid));
    snprintf(onlineTopic_, sizeof(onlineTopic_), MQTT_STATUS_ONLINE, uuid);
    return onlineTopic_;
}

bool MqttHandler::isTopicBoundToDevice(const char *topic) const
{
    return topicMatchesDeviceUuid(topic, config_.deviceId);
}

void MqttHandler::update()
{
    const bool ethernetUp = (Ethernet.linkStatus() == LinkON);
    const bool wifiUp = (WiFi.status() == WL_CONNECTED);
    handleNetworkLinkState(ethernetUp, wifiUp);
    activeTransport_ = selectActiveTransport();
    if (!isNetworkAvailable())
    {
        subscribed_ = false;
        logConnectionStateIfChanged();
        return;
    }

    if (activeTransport_ == NetworkTransport::Ethernet)
        syncUnixTimeFromNtp(false);

    if (!mqttClient_.connected())
    {
        uint32_t now = millis();
        if (!shouldAttemptReconnect(now))
        {
            logConnectionStateIfChanged();
            return;
        }

        connectBroker();
        lastReconnectAttemptMs_ = millis();
        logConnectionStateIfChanged();
        return;
    }

    if (!subscribed_ && !ensureSubscription())
    {
        lastClientState_ = mqttClient_.state();
        logConnectionStateIfChanged();
        return;
    }

    const bool loopOk = mqttClient_.loop();
    if (!loopOk && !mqttClient_.connected())
    {
        lastClientState_ = mqttClient_.state();
        subscribed_ = false;
        Serial.print(F("[MQTT] loop() lost connection, state="));
        Serial.println(lastClientState_);
        Serial.print(F("[MQTT][DIAG] loop() state label: "));
        Serial.println(mqttStateLabel(lastClientState_));
        ethClient_.stop();
        wifiClient_.stop();
    }
    logConnectionStateIfChanged();
}

void MqttHandler::bindActiveClient(NetworkTransport transport)
{
    if (transport == NetworkTransport::Ethernet)
    {
        mqttClient_.setClient(ethClient_);
    }
    else if (transport == NetworkTransport::WiFi)
    {
        mqttClient_.setClient(wifiClient_);
    }
}

bool MqttHandler::shouldPreferWiFiFailover(uint32_t now) const
{
    return preferWiFiUntilMs_ != 0 && (int32_t)(preferWiFiUntilMs_ - now) > 0;
}

void MqttHandler::handleNetworkLinkState(bool ethernetUp, bool wifiUp)
{
    if (ethernetUp != lastEthernetLinkUp_)
    {
        lastEthernetLinkUp_ = ethernetUp;
        Serial.println(ethernetUp ? F("[MQTT] Ethernet link restored") : F("[MQTT] Ethernet link lost"));
        if (ethernetUp)
            lastReconnectAttemptMs_ = millis() - reconnectIntervalMs_;
        else
        {
            // Ethernet just went down: proactively start WiFi connect
            // so fallback to WiFi is faster instead of waiting for periodic checks.
            connectToWiFi();
        }
    }

    if (wifiUp != lastWiFiLinkUp_)
    {
        lastWiFiLinkUp_ = wifiUp;
        Serial.println(wifiUp ? F("[MQTT] WiFi link restored") : F("[MQTT] WiFi link lost"));
        if (wifiUp)
            lastReconnectAttemptMs_ = millis() - reconnectIntervalMs_;
    }

    const NetworkTransport selectedTransport = selectActiveTransport();
    if (selectedTransport != activeTransport_)
    {
        if (mqttClient_.connected())
        {
            Serial.println(F("[MQTT] Network transport changed, closing MQTT socket"));
            mqttClient_.disconnect();
        }
        ethClient_.stop();
        wifiClient_.stop();
        bindActiveClient(selectedTransport);
        lastClientState_ = MQTT_CONNECTION_LOST;
        connectionStateKnown_ = false;
        subscribed_ = false;
        activeTransport_ = selectedTransport;
        if (selectedTransport != NetworkTransport::None)
            lastReconnectAttemptMs_ = millis() - reconnectIntervalMs_;
    }
}

void MqttHandler::onCommandReceived(CommandCallback cb)
{
    commandCallback_ = cb;
}

bool MqttHandler::reloadConfig()
{
    if (mqttClient_.connected())
    {
        mqttClient_.disconnect();
    }

    loadMqttConfig(config_);
    mqttClient_.setServer(config_.host, config_.port);
    mqttClient_.setSocketTimeout(kMqttSocketTimeoutSeconds);
    mqttClient_.setKeepAlive(kMqttKeepAliveSeconds);
    mqttClient_.setBufferSize(kMqttBufferSize);
    Serial.print(F("[MQTT] reloadConfig host="));
    Serial.print(config_.host);
    Serial.print(F(" clientId="));
    Serial.print(config_.clientId);
    Serial.print(F(" statusTopic="));
    Serial.print(config_.statusTopic);
    Serial.print(F(" commandTopic="));
    Serial.print(config_.commandTopic);
    Serial.print(F(" onlineTopic="));
    Serial.print(getOnlineTopic());
    Serial.println();
    subscribed_ = false;
    return isValidMqttConfig(config_);
}

bool MqttHandler::isConnected()
{
    return isNetworkAvailable() && mqttClient_.connected();
}

bool MqttHandler::connectBroker()
{
    if (!isValidMqttConfig(config_))
    {
        Serial.println(F("[MQTT][DIAG] Config invalid, skip broker connect."));
        return false;
    }

    activeTransport_ = selectActiveTransport();
    if (activeTransport_ == NetworkTransport::None)
    {
        Serial.println(F("[MQTT][DIAG] No network available, skip broker connect."));
        return false;
    }

    bindActiveClient(activeTransport_);

    Serial.print(F("[MQTT] Connecting to broker host="));
    Serial.print(config_.host);
    Serial.print(F(" port="));
    Serial.print(config_.port);
    Serial.print(F(" via="));
    Serial.print(activeTransport_ == NetworkTransport::Ethernet ? F("Ethernet") : F("WiFi"));
    Serial.print(F(" clientId="));
    Serial.print(config_.clientId);
    Serial.print(F(" onlineTopic="));
    Serial.println(getOnlineTopic());

    const char *clientId = config_.clientId;
    if (!clientId || clientId[0] == '\0')
        clientId = "DEMO_HG_QUANTRAC_2025";

    ethClient_.stop();
    wifiClient_.stop();

    char offlinePayload[256];
    if (!buildOnlinePayload(config_.deviceId, false, offlinePayload, sizeof(offlinePayload)))
        snprintf(offlinePayload, sizeof(offlinePayload), "{\"Device_ID\":\"%s\",\"Status\":\"Offline\",\"Time\":%lu}", config_.deviceId, (unsigned long)getNonZeroTime());

    bool connected = false;
    if (config_.username[0] != '\0')
        connected = mqttClient_.connect(clientId, config_.username, config_.password, getOnlineTopic(), 1, true, offlinePayload);
    else
        connected = mqttClient_.connect(clientId, getOnlineTopic(), 1, true, offlinePayload);

    if (!connected)
    {
        if (transportMode_ == TransportMode::Auto &&
            activeTransport_ == NetworkTransport::Ethernet &&
            WiFi.status() == WL_CONNECTED)
        {
            preferWiFiUntilMs_ = millis() + 30000UL;
            Serial.println(F("[MQTT][DIAG] Ethernet broker connect failed, prefer WiFi fallback for 30s."));
        }
        ethClient_.stop();
        wifiClient_.stop();
        subscribed_ = false;
        lastClientState_ = mqttClient_.state();
        Serial.print(F("[MQTT] Connect failed, state="));
        Serial.println(mqttClient_.state());
        Serial.print(F("[MQTT][DIAG] Reason: "));
        Serial.println(mqttStateLabel(mqttClient_.state()));
        Serial.println(F("[MQTT][DIAG] If Ethernet is UP, recheck gateway, DNS, broker host/port, and MQTT credentials."));
        return false;
    }

    Serial.print(F("[MQTT] Connected, topic="));
    Serial.println(config_.commandTopic);
    subscribed_ = false;
    if (!ensureSubscription())
    {
        lastClientState_ = mqttClient_.state();
        mqttClient_.disconnect();
        ethClient_.stop();
        wifiClient_.stop();
        subscribed_ = false;
        Serial.println(F("[MQTT][DIAG] Disconnecting so reconnect can retry subscribe."));
        return false;
    }

    if (!publishOnline(true))
        Serial.println(F("[MQTT][DIAG] Connected but failed to publish retained online status."));

    return true;
}

bool MqttHandler::ensureSubscription()
{
    if (!mqttClient_.connected())
    {
        subscribed_ = false;
        return false;
    }

    if (subscribed_)
        return true;

    const bool subscribed = mqttClient_.subscribe(config_.commandTopic, 1);
    if (!subscribed)
    {
        Serial.println(F("[MQTT] Subscribe failed"));
        Serial.println(F("[MQTT][DIAG] Broker connection is up but subscribe request was rejected."));
        return false;
    }

    subscribed_ = true;
    lastClientState_ = MQTT_CONNECTED;
    Serial.print(F("[MQTT] Subscribe OK topic="));
    Serial.println(config_.commandTopic);
    return true;
}

void MqttHandler::logConnectionStateIfChanged()
{
    const bool connected = mqttClient_.connected();
    const int state = connected ? MQTT_CONNECTED : mqttClient_.state();

    if (connectionStateKnown_ && connected == lastConnectionState_ && state == lastClientState_)
        return;

    connectionStateKnown_ = true;
    lastConnectionState_ = connected;
    lastClientState_ = state;

    if (connected)
    {
        Serial.print(F("[MQTT] Broker UP, subscribed topic="));
        Serial.println(config_.commandTopic);
        return;
    }

    Serial.print(F("[MQTT] Broker DOWN, state="));
    Serial.println(state);
    Serial.print(F("[MQTT][DIAG] State label: "));
    Serial.println(mqttStateLabel(state));
}

void MqttHandler::handleMessage(char *topic, uint8_t *payload, unsigned int length)
{
    if (!topic || strcmp(topic, config_.commandTopic) != 0)
        return;
    if (!topicMatchesDeviceUuid(topic, config_.deviceId))
    {
        Serial.print(F("[MQTT][DIAG] Topic UUID mismatch, topic="));
        Serial.print(topic);
        Serial.print(F(" deviceId="));
        Serial.println(config_.deviceId);
        return;
    }
    logIncomingPayload(topic, payload, length);

    IoCommand cmd{};
    if (!parseCommandPayload(payload, length, cmd))
    {
        Serial.println(F("[MQTT] Invalid command payload"));
        return;
    }
    if (commandCallback_)
    {
        Serial.println(F("[MQTT] Dispatching command callback"));
        commandCallback_(cmd);
    }
}

bool MqttHandler::parseCommandPayload(const uint8_t *payload, unsigned int length, IoCommand &cmd)
{
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
        Serial.print(F("[MQTT][DIAG] JSON parse failed: "));
        Serial.println(error.c_str());
        return false;
    }

    int opcode = getIntField(doc, "Opcode", "opcode", -1);
    uint32_t unix_time = getUnixTimeField(doc, "Time", "time", 0);

    cmd.unix = unix_time;
    cmd.opcode = (uint8_t)((opcode >= 0) ? opcode : 0);

    if (opcode != DASHBOARD_INFO && opcode != IO_COMMAND)
    {
        Serial.print(F("[MQTT][DIAG] Unsupported opcode="));
        Serial.println(opcode);
        return false;
    }

    if (!verifyAuthIfPresent(doc, config_.deviceId))
    {
        Serial.println(F("[MQTT][DIAG] Auth verification failed"));
        return false;
    }

    if (!isCommandTimeValid(unix_time, cmd.opcode))
    {
        Serial.print(F("[MQTT][DIAG] Invalid command time="));
        Serial.println((unsigned long)unix_time);
        return false;
    }

    if (opcode == DASHBOARD_INFO)
    {
        cmd.out1 = false;
        cmd.out2 = false;
        cmd.out3 = false;
        cmd.out4 = false;
        cmd.out5 = false;
        cmd.out6 = false;
        cmd.out7 = false;
        cmd.out8 = false;
        Serial.println(F("[MQTT] Status request parsed successfully"));
        return true;
    }

    JsonObjectConst dataObj = getDataVariant(doc).as<JsonObjectConst>();
    if (dataObj.isNull())
    {
        Serial.println(F("[MQTT][DIAG] Missing Data object"));
        return false;
    }

    if (!getOutputValue(dataObj, "out1", "Out1", cmd.out1) ||
        !getOutputValue(dataObj, "out2", "Out2", cmd.out2) ||
        !getOutputValue(dataObj, "out3", "Out3", cmd.out3) ||
        !getOutputValue(dataObj, "out4", "Out4", cmd.out4))
    {
        String dataJson;
        serializeJson(dataObj, dataJson);
        Serial.print(F("[MQTT][DIAG] Invalid output mapping in Data="));
        Serial.println(dataJson);
        return false;
    }

    if (!dataObj["out5"].isNull() || !dataObj["Out5"].isNull())
    {
        if (!getOutputValue(dataObj, "out5", "Out5", cmd.out5))
            return false;
    }
    else
    {
        cmd.out5 = false;
    }

    if (!dataObj["out6"].isNull() || !dataObj["Out6"].isNull())
    {
        if (!getOutputValue(dataObj, "out6", "Out6", cmd.out6))
            return false;
    }
    else
    {
        cmd.out6 = false;
    }

    if (!dataObj["out7"].isNull() || !dataObj["Out7"].isNull())
    {
        if (!getOutputValue(dataObj, "out7", "Out7", cmd.out7))
            return false;
    }
    else
    {
        cmd.out7 = false;
    }

    if (!dataObj["out8"].isNull() || !dataObj["Out8"].isNull())
    {
        if (!getOutputValue(dataObj, "out8", "Out8", cmd.out8))
            return false;
    }
    else
    {
        cmd.out8 = false;
    }
    Serial.print(F("[MQTT] Command parsed successfully opcode="));
    Serial.println(cmd.opcode);
    Serial.print(F("[MQTT] Command outputs out1="));
    Serial.print(cmd.out1 ? F("ON") : F("OFF"));
    Serial.print(F(" out2="));
    Serial.print(cmd.out2 ? F("ON") : F("OFF"));
    Serial.print(F(" out3="));
    Serial.print(cmd.out3 ? F("ON") : F("OFF"));
    Serial.print(F(" out4="));
    Serial.print(cmd.out4 ? F("ON") : F("OFF"));
    Serial.print(F(" out5="));
    Serial.print(cmd.out5 ? F("ON") : F("OFF"));
    Serial.print(F(" out6="));
    Serial.print(cmd.out6 ? F("ON") : F("OFF"));
    Serial.print(F(" out7="));
    Serial.print(cmd.out7 ? F("ON") : F("OFF"));
    Serial.print(F(" out8="));
    Serial.println(cmd.out8 ? F("ON") : F("OFF"));
    return true;
}

bool MqttHandler::verifyAuthIfPresent(const JsonDocument &doc, const char *topicDeviceId)
{
    (void)doc;
    (void)topicDeviceId;
    return true;
}

bool MqttHandler::publishStatus(bool autoMode, const bool outputs[OUT_COUNT], uint8_t ui8_power_state, uint8_t outputCount)
{
    if (!mqttClient_.connected())
        return false;
    if (!isTopicBoundToDevice(config_.statusTopic))
    {
        Serial.print(F("[MQTT][DIAG] Refuse publish status, topic UUID mismatch: "));
        Serial.println(config_.statusTopic);
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["Device_ID"] = config_.deviceId;
    doc["Opcode"] = DASHBOARD_INFO;

    JsonObject data = doc.createNestedObject("Data");
    data["Version"] = MQTT_PAYLOAD_VERSION;
    const uint8_t count = (outputCount > OUT_COUNT) ? OUT_COUNT : outputCount;
    bool anyOutputOn = false;
    for (uint8_t i = 0; i < count; i++)
    {
        if (outputs[i])
        {
            anyOutputOn = true;
            break;
        }
    }

    data["State"] = ui8_power_state ? "On" : "Off";
    data["Mode"] = autoMode ? "Auto" : "Man";
    for (uint8_t i = 0; i < count; i++)
    {
        char key[8];
        snprintf(key, sizeof(key), "out%u", i + 1);
        data[key] = outputs[i] ? 1 : 0;
    }

    uint32_t unixTime = getCurrentUnixTime();
    if (unixTime == 0 && syncUnixTimeFromNtp(true))
        unixTime = getCurrentUnixTime();
    doc["Time"] = unixTime;

    String dataJson;
    serializeJson(doc["Data"], dataJson);
    doc["Auth"] = calculateMD5(String(config_.deviceId) + String((uint8_t)DASHBOARD_INFO) + dataJson + String(unixTime) + PRIVATE_KEY);
    Serial.print(F("[MQTT] Publishing status mode="));
    Serial.print(autoMode ? F("AUTO") : F("MAN"));
    Serial.print(F(" topic="));
    Serial.println(config_.statusTopic);

    Serial.println(F("[MQTT][DIAG] payload:"));
    Serial.println(dataJson);
    return publishJson(config_.statusTopic, doc, true);
}

bool MqttHandler::publishOnline(bool online)
{
    if (!mqttClient_.connected())
        return false;
    if (!isTopicBoundToDevice(getOnlineTopic()))
    {
        Serial.print(F("[MQTT][DIAG] Refuse publish online, topic UUID mismatch: "));
        Serial.println(getOnlineTopic());
        return false;
    }

    char payload[256];
    if (!buildOnlinePayload(config_.deviceId, online, payload, sizeof(payload)))
        return false;

    Serial.print(F("[MQTT] Publishing online status="));
    Serial.print(online ? F("ONLINE") : F("OFFLINE"));
    Serial.print(F(" topic="));
    Serial.print(getOnlineTopic());
    Serial.print(F(" payload="));
    Serial.println(payload);
    return mqttClient_.publish(getOnlineTopic(), (const uint8_t *)payload, strlen(payload), true);
}

bool MqttHandler::publishJson(const char *topic, const JsonDocument &doc, bool retained)
{
    if (!topic || topic[0] == '\0')
        return false;

    char payload[768];
    size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0 || len >= sizeof(payload))
        return false;

    return mqttClient_.publish(topic, (const uint8_t *)payload, len, retained);
}

MqttHandler::NetworkTransport MqttHandler::selectActiveTransport() const
{
    const bool ethernetUp = (Ethernet.linkStatus() == LinkON);
    const bool wifiUp = (WiFi.status() == WL_CONNECTED);
    const uint32_t now = millis();

    if (transportMode_ == TransportMode::EthernetOnly)
        return ethernetUp ? NetworkTransport::Ethernet : NetworkTransport::None;

    if (transportMode_ == TransportMode::WiFiOnly)
        return wifiUp ? NetworkTransport::WiFi : NetworkTransport::None;

    if (ethernetUp && wifiUp && shouldPreferWiFiFailover(now))
        return NetworkTransport::WiFi;

    if (ethernetUp)
        return NetworkTransport::Ethernet;
    if (wifiUp)
        return NetworkTransport::WiFi;
    return NetworkTransport::None;
}

bool MqttHandler::isNetworkAvailable() const
{
    return selectActiveTransport() != NetworkTransport::None;
}

bool MqttHandler::shouldAttemptReconnect(uint32_t now) const
{
    return (uint32_t)(now - lastReconnectAttemptMs_) >= reconnectIntervalMs_;
}

void MqttHandler::logIncomingPayload(const char *topic, const uint8_t *payload, unsigned int length) const
{
    Serial.print(F("[MQTT] Message received on topic="));
    Serial.print(topic);
    Serial.print(F(" length="));
    Serial.println(length);

    if (!payload || length == 0)
        return;

    Serial.print(F("[MQTT][DIAG] Payload="));
    for (unsigned int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();
}



