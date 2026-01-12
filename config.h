#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>

const String SECRET_KEY = "ALTA_MIST_CONTROLLER";

//Các opcode cho các lệnh
#define MIST_COMMAND 0x01
#define IO_COMMAND 0x02
#define GET_INFO 0x03
#define MOTOR_COMMAND 0x04
#define SENSOR_VOC 0x05
#define RESPONSE_OFFSET 0x64 //+100

static const uint8_t OUT_COUNT = 4;
static const uint8_t IN_COUNT = 4;
static const uint8_t MOTOR_COUNT = 5;

typedef struct
{
    uint8_t run;
    uint8_t dir;
    uint8_t speed;
} MotorCommand;

typedef struct
{
    uint32_t unix;
    int8_t id_des;
    uint8_t opcode;
    int8_t node_id;
    uint16_t time_phase1;        // Chi dung khi opcode=1
    uint16_t time_phase2;        // Chi dung khi opcode=1
    bool out1, out2, out3, out4; // Chi dung khi opcode=2
    MotorCommand motors[MOTOR_COUNT]; // Chi dung khi opcode=4
    uint8_t motor_mask;              // bit i=1 neu mi co trong data
} MistCommand;

struct VocReading {
  float temp;
  float humi;
  float nh3;
  float co;
  float no2;
};

inline String calculateMD5(const String &input)
{
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString(); //trả về chuỗi MD5 hex
}


