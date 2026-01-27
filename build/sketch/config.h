#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\config.h"
#pragma once
#ifndef CONFIG_H // ← Tên phải UNIQUE, thường viết hoa + _H
#define CONFIG_H
#include <Arduino.h>
#define RS485_RX_PIN -1
#define RS485_TX_PIN -1
#define PIN_CO 14
#define PIN_NO2 17
#define PIN_NH3 18
#define PIN_DHT 21
#define CONFIG_BUTTON_PIN 12
// OTA Basic Auth (change in production)
#define OTA_AUTH_USER "admin"
#define OTA_AUTH_PASS "admin123"
// #define MOTOR_COUNT 5
// #define OUT_COUNT 4
// #define IN_COUNT 4
extern const String SECRET_KEY;

extern bool has_mode_config_on;
extern bool has_mode_config;

static const uint8_t OUT_COUNT = 4;
static const uint8_t IN_COUNT = 4;
static const uint8_t MOTOR_COUNT = 5;

extern bool has_connect_link;
extern bool has_data_serial;

typedef struct
{
    uint8_t run;
    uint8_t dir;
    uint8_t speed;
} MotorCommand;
struct Thresholds
{
    float temp_on;
    float temp_off;
    float humi_on;
    float humi_off;
    float nh3_on;
    float nh3_off;
    float co_on;
    float co_off;
    float no2_on;
    float no2_off;
};
struct MistCommand
{
    uint32_t unix;
    int8_t id_des;
    uint8_t opcode;
    int8_t node_id;
    uint16_t time_phase1;             // Chỉ dùng khi opcode=1
    uint16_t time_phase2;             // Chỉ dùng khi opcode=1
    bool out1, out2, out3, out4;      // Chỉ dùng khi opcode=2
    MotorCommand motors[MOTOR_COUNT]; // Chi dung khi opcode=4
    uint8_t motor_mask;
    Thresholds thresholds; // Chi dung khi opcode=6
};
#endif // CONFIG_H
