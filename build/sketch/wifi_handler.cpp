#line 1 "D:\\Power_Central_v4\\wifi_handler.cpp"
#include "wifi_handler.h"

#include <Ethernet.h>
#include <WiFi.h>
#include "net_config.h"
#include "config.h"

namespace
{
    unsigned long gLastReconnectAttemptMs = 0;
    bool gConnectInProgress = false;
    WiFiConfig gWiFiConfig{};

    void loadRuntimeWiFiConfig()
    {
        loadWiFiConfig(gWiFiConfig);
    }

    bool isEthernetLinkUp()
    {
        return Ethernet.linkStatus() == LinkON;
    }

    void setWiFiLinkState(bool connected)
    {
        if (has_wifi_connection == connected)
            return;

        has_wifi_connection = connected;
        Serial.println(connected ? F("[WIFI] STA link UP") : F("[WIFI] STA link DOWN"));
    }
}

void setupWiFi()
{
    loadRuntimeWiFiConfig();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    if (isEthernetLinkUp())
    {
        gConnectInProgress = false;
        WiFi.setAutoReconnect(false);
        setWiFiLinkState(isWiFiConnected());
        Serial.println(F("[WIFI] Ethernet link is up; skip initial WiFi connect"));
        return;
    }

    connectToWiFi();
}

bool isWiFiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void connectToWiFi()
{
    if (isWiFiConnected())
    {
        gConnectInProgress = false;
        setWiFiLinkState(true);
        return;
    }

    if (isEthernetLinkUp())
    {
        gConnectInProgress = false;
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        setWiFiLinkState(false);
        return;
    }

    const unsigned long now = millis();
    if ((uint32_t)(now - gLastReconnectAttemptMs) < 1000UL)
    {
        return;
    }
    gLastReconnectAttemptMs = now;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    Serial.print(F("[WIFI] Connecting to "));
    Serial.println(gWiFiConfig.ssid);

    WiFi.begin(gWiFiConfig.ssid, gWiFiConfig.password);
    // check_wifi=true;
    gConnectInProgress = true;
}

void checkWiFiConnection()
{
    if(has_mode_config_on==true) return;
    if (isWiFiConnected())
    {
        setWiFiLinkState(true);
        if (gConnectInProgress)
        {
            gConnectInProgress = false;
            Serial.print(F("[WIFI] Connected, IP: "));
            Serial.println(WiFi.localIP());
            // check_wifi=true;
        }
        return;
    }

    setWiFiLinkState(false);

    if (isEthernetLinkUp())
    {
        if (gConnectInProgress)
        {
            gConnectInProgress = false;
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(false, false);
            Serial.println(F("[WIFI] Ethernet link is up; stop WiFi retry"));
        }
        return;
    }

    if (gConnectInProgress)
    {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_DISCONNECTED)
        {
            gConnectInProgress = false;
            Serial.print(F("[WIFI] Connect pending, status="));
            Serial.println((int)status);
            // check_wifi=false;
        }
        return;
    }

    if (!isWiFiConnected())
    {
        connectToWiFi();
        // check_wifi=false;
    }
}

bool reloadWiFiConfig()
{
    loadRuntimeWiFiConfig();
    gConnectInProgress = false;
    gLastReconnectAttemptMs = 0;

    WiFi.disconnect(true, false);
    setWiFiLinkState(false);
    delay(100);
    if (!isEthernetLinkUp())
        connectToWiFi();
    else
        Serial.println(F("[WIFI] Config saved; Ethernet link is up, WiFi reconnect skipped"));
    return isValidWiFiConfig(gWiFiConfig);
}
