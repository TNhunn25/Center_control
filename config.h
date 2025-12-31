#pragma once
#include <Arduino.h>

struct MistCommand
{
    uint32_t unix;
    int8_t id_des;
    uint8_t opcode;            
    int8_t node_id;              
    uint16_t time_phase1;        // Chỉ dùng khi opcode=1
    uint16_t time_phase2;        // Chỉ dùng khi opcode=1
    bool out1, out2, out3, out4; // Chỉ dùng khi opcode=2
};
