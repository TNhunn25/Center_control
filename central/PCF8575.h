#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <PCF8575.h>

namespace PcfIo
{
    static constexpr uint8_t I2C_ADDR = 0x20;
    static constexpr int SDA_PIN = 8;
    static constexpr int SCL_PIN = 9;

    // Fill these mappings later (0xFF = not used).
    static constexpr uint8_t OUT_PINS[4] = {10, 11, 12, 13};
    static constexpr uint8_t IN_PINS[4] = {0, 1, 2, 3};
    static constexpr uint8_t AUTO_MAN_PIN = 4;

    static PCF8575 pcf(I2C_ADDR);
    static bool initialized = false;

    inline void configureInput(uint8_t pin)
    {
        if (pin == 0xFF)
            return;
        pcf.pinMode(pin, INPUT);
        pcf.digitalWrite(pin, HIGH); // release input
    }

    inline void begin()
    {
        if (initialized)
            return;

        Wire.begin(SDA_PIN, SCL_PIN);
        pcf.begin();
        for (uint8_t i = 0; i < 4; i++)
        {
            if (OUT_PINS[i] != 0xFF)
            {
                pcf.pinMode(OUT_PINS[i], OUTPUT);
                pcf.digitalWrite(OUT_PINS[i], HIGH); // active-low: OFF
            }

            if (IN_PINS[i] != 0xFF)
            {
                configureInput(IN_PINS[i]);
            }
        }
        configureInput(AUTO_MAN_PIN);

        initialized = true;
    }

    inline void writeOut(uint8_t index, bool on)
    {
        if (index >= 4)
            return;
        const uint8_t pin = OUT_PINS[index];
        if (pin == 0xFF)
            return;
        pcf.digitalWrite(pin, on ? LOW : HIGH);
    }

    inline bool readIn(uint8_t index)
    {
        if (index >= 4)
            return false;
        const uint8_t pin = IN_PINS[index];
        if (pin == 0xFF)
            return false;
        return pcf.digitalRead(pin) == LOW; // active-low input
    }

    inline bool readAutoMode()
    {
        return pcf.digitalRead(AUTO_MAN_PIN) == HIGH;
    }
} // namespace PcfIo
