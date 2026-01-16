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
#define MOTOR_COUNT 5
#define OUT_COUNT 4
#define IN_COUNT 4
extern const String SECRET_KEY;
// Not used when IO is handled via PCF8575; kept for compatibility.
extern const uint8_t OUT_PINS[4];
extern const uint8_t IN_PINS[4];
extern bool has_connect_link;
extern bool has_data_serial;

typedef struct
{
    uint8_t run;
    uint8_t dir;
    uint8_t speed;
} MotorCommand;
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
};
#endif // CONFIG_H
