#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\central\\pcf8575_io.h"
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Arduino.h>
#include <PCF8575.h>
#include "config.h"

class PCF8575IO
{
public:
    static constexpr uint8_t I2C_ADDR = 0x20;
    static constexpr int SDA_PIN = 18; // ESP32 I2C SDA
    static constexpr int SCL_PIN = 17; // ESP32 I2C SCL
    static constexpr uint8_t NOT_USED = 0xFF;

    // Fill these mappings later (0xFF = not used).
    const uint8_t OUT_PINS[OUT_COUNT] = {1, 2, 3, 4};
    const uint8_t IN_PINS[IN_COUNT] = {11, 12, 13, 14};
    const uint8_t AUTO_MAN_PIN = 10;

    void begin()
    {
        Wire.begin(SDA_PIN, SCL_PIN);
        write_state_ = 0xFFFF; // all pins high (input mode for PCF8575)
        applyState();
    }

    void prepareInputs()
    {
        for (uint8_t i = 0; i < IN_COUNT; i++)
        {
            uint8_t pin = IN_PINS[i];
            if (pin != NOT_USED && pin < 16)
                write_state_ |= (1u << pin);
        }
        if (AUTO_MAN_PIN != NOT_USED && AUTO_MAN_PIN < 16)
            write_state_ |= (1u << AUTO_MAN_PIN);
        applyState();
    }

    void writeOutput(uint8_t ch, bool level)
    {
        if (ch >= OUT_COUNT)
            return;
        uint8_t pin = OUT_PINS[ch];
        if (pin == NOT_USED || pin >= 16)
            return;
        writePin(pin, level);
    }

    bool readInput(uint8_t ch)
    {
        if (ch >= IN_COUNT)
            return false;
        uint8_t pin = IN_PINS[ch];
        if (pin == NOT_USED || pin >= 16)
            return false;
        return readPin(pin);
    }

    bool readAutoMan()
    {
        if (AUTO_MAN_PIN == NOT_USED || AUTO_MAN_PIN >= 16)
            return true; // default AUTO
        return !readPin(AUTO_MAN_PIN);
    }

private:
    uint16_t write_state_ = 0xFFFF;

    void applyState()
    {
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(lowByte(write_state_));
        Wire.write(highByte(write_state_));
        Wire.endTransmission();
    }

    void writePin(uint8_t pin, bool level)
    {
        if (level)
            write_state_ |= (1u << pin);
        else
            write_state_ &= ~(1u << pin);
        applyState();
    }

    bool readPin(uint8_t pin)
    {
        Wire.requestFrom((int)I2C_ADDR, 2);
        if (Wire.available() < 2)
            return false;
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        uint16_t v = (uint16_t(hi) << 8) | lo;
        return ((v >> pin) & 0x01u) != 0;
    }
};
