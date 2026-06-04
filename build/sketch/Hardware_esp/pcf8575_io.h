#line 1 "D:\\Power_Central_v4\\Hardware_esp\\pcf8575_io.h"
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <PCF8575.h>
#include "config.h"

class PCF8575IO
{
public:
    static constexpr uint8_t I2C_ADDR = 0x22;
    static constexpr uint8_t NOT_USED = 0xFF;
    static constexpr uint16_t I2C_TIMEOUT_MS = 25;
    
    static constexpr int SDA_PIN = 17; // EST32 I2C SDA 
    static constexpr int SCL_PIN = 14; // EST32 I2C SCL

    const uint8_t OUT_PINS[OUT_COUNT] = {P7, P6, P5, P4, P3, P2, P1, P0}; //Bo Hòa Lân (chỗ này thì nhớ đổi T SANG P nha)
    const uint8_t IN_PINS[IN_COUNT] = {P13, P12, P11};//(chỗ này thì nhớ đổi T SANG P nha)
    const uint8_t AUTO_MAN_PIN = P14;//(chỗ này thì nhớ đổi T SANG P nha)
    

    // ORIG: PCF8575IO used global Wire directly. The default constructor still
    // keeps that behavior, while a shared TwoWire bus can now be injected.
    explicit PCF8575IO(TwoWire &wire = Wire)
        : wire_(wire)
    {
    }

    void begin(bool outputsOffLevel = true)
    {
        begin(SDA_PIN, SCL_PIN, outputsOffLevel);
    }

    void begin(int sdaPin, int sclPin, bool outputsOffLevel = true)
    {
        wire_.begin(sdaPin, sclPin);
        wire_.setTimeOut(I2C_TIMEOUT_MS);

        // Build the very first PCF8575 output image with relay outputs already
        // at OFF level. This avoids a short boot pulse where outputs are left
        // at the chip default/high state before the controller writes them.
        write_state_ = 0xFFFF; // high = input/released for PCF8575

        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            uint8_t pin = OUT_PINS[i];
            if (pin == NOT_USED || pin >= 16)
                continue;

            if (outputsOffLevel)
                write_state_ |= (1u << pin);
            else
                write_state_ &= ~(1u << pin);
        }

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

    void prepareInputPin(uint8_t pin)
    {
        if (pin == NOT_USED || pin >= 16)
            return;

        write_state_ |= (1u << pin);
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

    bool readDigital(uint8_t pin)
    {
        if (pin == NOT_USED || pin >= 16)
            return true;
        return readPin(pin);
    }

    bool readAll(uint16_t &value)
    {
        return readPort(value);
    }

private:
    TwoWire &wire_;
    uint16_t write_state_ = 0xFFFF;

    void applyState()
    {
        wire_.beginTransmission(I2C_ADDR);
        wire_.write(lowByte(write_state_));
        wire_.write(highByte(write_state_));
        wire_.endTransmission();
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
        uint16_t v = 0;
        if (!readPort(v))
            return false;
        return ((v >> pin) & 0x01u) != 0;
    }

    bool readPort(uint16_t &value)
    {
        size_t bytesRead = wire_.requestFrom(I2C_ADDR, (size_t)2);
        if (bytesRead < 2 || wire_.available() < 2)
        {
            while (wire_.available())
                wire_.read();
            return false;
        }
        uint8_t lo = wire_.read();
        uint8_t hi = wire_.read();
        value = (uint16_t(hi) << 8) | lo;
        return true;
    }
};

class ManualPcf8575IO
{
public:
    static constexpr uint8_t I2C_ADDR = 0x21;
    static constexpr uint8_t NOT_USED = PCF8575IO::NOT_USED;
    static constexpr uint8_t BUTTON_COUNT = 8;
    static constexpr uint8_t LED_COUNT = 8;
    // Pin numbers here are PCF8575 bit indexes, not ESP32 GPIO/touch macros.
    // Manual buttons pull the PCF pin LOW when pressed.
    static constexpr bool BUTTON_ACTIVE_LOW = true;
    static constexpr bool LED_ACTIVE_LOW = false;

    const uint8_t BUTTON_PINS[BUTTON_COUNT] = {8, 9, 10, 11, 12, 13, 14, 15};
    const uint8_t LED_PINS[LED_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7};

    explicit ManualPcf8575IO(TwoWire &wire = Wire)
        : wire_(wire)
    {
    }

    void begin()
    {
        write_state_ = 0xFFFF; // high = input before pin directions are settled
        prepareButtons();
        for (uint8_t i = 0; i < LED_COUNT; i++)
            writeLed(i, false);

        Serial.print(F("[PCF2] begin addr=0x"));
        Serial.print(I2C_ADDR, HEX);
        Serial.print(F(" buttons="));
        printPinList(BUTTON_PINS, BUTTON_COUNT);
        Serial.print(F(" leds="));
        printPinList(LED_PINS, LED_COUNT);
        Serial.print(F(" buttonPressed="));
        Serial.print(BUTTON_ACTIVE_LOW ? F("LOW") : F("HIGH"));
        Serial.print(F(" ledOn="));
        Serial.println(LED_ACTIVE_LOW ? F("LOW") : F("HIGH"));
    }

    void prepareButtons()
    {
        for (uint8_t i = 0; i < BUTTON_COUNT; i++)
        {
            uint8_t pin = BUTTON_PINS[i];
            if (pin != NOT_USED && pin < 16)
                write_state_ |= (1u << pin);
        }
        applyState();
    }

    bool readButton(uint8_t ch)
    {
        if (ch >= BUTTON_COUNT)
            return releasedLevel();
        uint8_t pin = BUTTON_PINS[ch];
        if (pin == NOT_USED || pin >= 16)
            return releasedLevel();
        return readPin(pin);
    }

    bool isButtonPressed(uint8_t ch)
    {
        return isPressedLevel(readButton(ch));
    }

    static bool isPressedLevel(bool level)
    {
        return BUTTON_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
    }

    static bool releasedLevel()
    {
        return BUTTON_ACTIVE_LOW ? HIGH : LOW;
    }

    void writeLed(uint8_t ch, bool on)
    {
        if (ch >= LED_COUNT)
            return;
        uint8_t pin = LED_PINS[ch];
        if (pin == NOT_USED || pin >= 16)
            return;

        const bool level = LED_ACTIVE_LOW ? !on : on;
        writePin(pin, level);
    }

    bool readAll(uint16_t &value)
    {
        return readPort(value);
    }

    bool readButtonsSnapshot(uint16_t &value)
    {
        // Buttons are on PCF pins 8..15 and LEDs are on pins 0..7, so we do
        // not need to release/toggle LED pins before reading the port.
        // The old code briefly wrote LED pins HIGH while taking this snapshot.
        // With active-high LEDs, that made LEDs that were OFF flash very fast
        // and look like they were glowing dimly in MANUAL mode.
        return readPort(value);
    }

private:
    TwoWire &wire_;
    uint16_t write_state_ = 0xFFFF;

    void applyState()
    {
        wire_.beginTransmission(I2C_ADDR);
        wire_.write(lowByte(write_state_));
        wire_.write(highByte(write_state_));
        wire_.endTransmission();
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
        uint16_t v = 0;
        if (!readPort(v))
            return releasedLevel();
        return ((v >> pin) & 0x01u) != 0;
    }

    bool readPort(uint16_t &value)
    {
        size_t bytesRead = wire_.requestFrom(I2C_ADDR, (size_t)2);
        if (bytesRead < 2 || wire_.available() < 2)
        {
            while (wire_.available())
                wire_.read();
            return false;
        }
        uint8_t lo = wire_.read();
        uint8_t hi = wire_.read();
        value = (uint16_t(hi) << 8) | lo;
        return true;
    }

    void printPinList(const uint8_t *pins, uint8_t count)
    {
        Serial.print(F("["));
        for (uint8_t i = 0; i < count; i++)
        {
            if (i > 0)
                Serial.print(F(","));
            Serial.print(pins[i]);
        }
        Serial.print(F("]"));
    }
};
