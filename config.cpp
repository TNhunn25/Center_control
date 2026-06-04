#include "config.h"

// =========================
// Shared Runtime State
// =========================
const String PRIVATE_KEY = "ALTA_2026@";
bool has_connect_link = false;
bool has_wifi_connection = false;
bool has_mqtt_connection = false;
bool has_mode_config_on = false;
bool has_mode_config = false;
uint8_t ui8_power_source = 0;

// =========================
// Ethernet Default Config
// =========================
const byte ETH_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
const IPAddress ETH_IP(192, 168, 80, 196);
const IPAddress ETH_DNS(8, 8, 8, 8);
const IPAddress ETH_GATEWAY(192, 168, 80, 254);
const IPAddress ETH_SUBNET(255, 255, 255, 0);

// =========================
// MQTT Default Config
// =========================
char MQTT_SERVER[] = "mqtt.dev.altasoftware.vn";
uint16_t MQTT_PORT = 1883;
char MQTT_USER[] = "altamedia";
char MQTT_PASSWORD[] = "Altamedia@%";
char MQTT_CLIENT_ID[] = "PDM_"; // PDM_{5-digit MAC suffix}
char MQTT_DEVICE_ID[] = "PDM_"; // PDM_{5-digit MAC suffix}
char MQTT_PAYLOAD_VERSION[] = "1.2.1";
char MQTT_STATUS_TOPIC[] = "POWER_CTRL/%s/status";
char MQTT_COMMAND_TOPIC[] = "POWER_CTRL/%s/command";
char MQTT_STATUS_ONLINE[] = "POWER_CTRL/%s/connection";
