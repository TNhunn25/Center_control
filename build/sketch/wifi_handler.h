#line 1 "D:\\Power_Central_v4\\wifi_handler.h"
#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>

// Khoi tao WiFi o che do STA de co san fallback khi Ethernet mat internet.
void setupWiFi();

// Kiem tra WiFi da ket noi thanh cong hay chua.
bool isWiFiConnected();

// Thu ket noi lai WiFi neu dang mat ket noi.
void connectToWiFi();

// Kiem tra va giu WiFi san sang de fallback khi can.
void checkWiFiConnection();

// Tai nap cau hinh WiFi runtime va reconnect theo thong so moi.
bool reloadWiFiConfig();

#endif
