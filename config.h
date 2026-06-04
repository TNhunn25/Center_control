#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// =========================
// System
// =========================
#define CONFIG_BUTTON_PIN 12

// OTA Basic Auth (change in production)
#define OTA_AUTH_USER "admin"
#define OTA_AUTH_PASS "admin123"

// Config portal login credentials.
#define PORTAL_AUTH_USER OTA_AUTH_USER
#define PORTAL_AUTH_PASS OTA_AUTH_PASS

// =========================
// Shared Runtime State
// =========================
extern const String PRIVATE_KEY;

extern bool has_connect_link;
extern bool has_wifi_connection;
extern bool has_mqtt_connection;
extern bool has_mode_config_on;
extern bool has_mode_config;
extern uint8_t ui8_power_source;

// extern bool check_connectwifi; 

// =========================
// IO Layout
// =========================
#ifndef ENABLE_PCF8575
#define ENABLE_PCF8575 0
#endif

static const uint8_t OUT_COUNT = 8;
static const uint8_t IN_COUNT = 4;

// =========================
// Ethernet Default Config
// =========================
extern const byte ETH_MAC[6];
extern const IPAddress ETH_IP;
extern const IPAddress ETH_DNS;
extern const IPAddress ETH_GATEWAY;
extern const IPAddress ETH_SUBNET;

// =========================
// MQTT Default Config
// =========================
extern char MQTT_SERVER[];
extern uint16_t MQTT_PORT;
extern char MQTT_USER[];
extern char MQTT_PASSWORD[];
extern char MQTT_CLIENT_ID[];
extern char MQTT_DEVICE_ID[];
extern char MQTT_PAYLOAD_VERSION[];
extern char MQTT_STATUS_TOPIC[];
extern char MQTT_STATUS_ONLINE[];
extern char MQTT_COMMAND_TOPIC[];

// =========================
// MQTT Protocol for Control Power
// =========================
enum Opcode : uint8_t
{
    DASHBOARD_INFO = 0x01,
    IO_COMMAND = 0x02
};

struct IoCommand
{
    uint32_t unix;
    uint8_t opcode;
    // true = output vat ly ON, false = output vat ly OFF.
    bool out1;
    bool out2;
    bool out3;
    bool out4;
    bool out5;
    bool out6;
    bool out7;
    bool out8;
};

#endif // CONFIG_H
