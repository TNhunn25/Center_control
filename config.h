#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>

const String SECRET_KEY = "ALTA_MIST_CONTROLLER";

//Các opcode cho các lệnh
#define MIST_COMMAND 0x01
#define IO_COMMAND 0x02
#define GET_INFO 0x03
#define RESPONSE_OFFSET 0x64 //+100


typedef struct 
{
    uint32_t unix;
    int8_t id_des;
    uint8_t opcode;            
    int8_t node_id;              
    uint16_t time_phase1;        // Chỉ dùng khi opcode=1
    uint16_t time_phase2;        // Chỉ dùng khi opcode=1
    bool out1, out2, out3, out4; // Chỉ dùng khi opcode=2
} MistCommand;

inline String calculateMD5(const String &input)
{
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString(); //trả về chuỗi MD5 hex
}


