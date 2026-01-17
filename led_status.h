#pragma once
#include <Arduino.h>
#include "config.h"
#include "pcf8575_io.h"

// Status LEDs (ESP32 GPIO)
static const uint8_t LED_RUN_PIN = 1; // xanh
static const uint8_t LED_AM_PIN = 2;  // do

inline void ledStatusBegin()
{
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(LED_AM_PIN, OUTPUT);
}

inline void setStatusLed(bool run_on, bool am_on)
{
    digitalWrite(LED_RUN_PIN, run_on ? HIGH : LOW);
    digitalWrite(LED_AM_PIN, am_on ? HIGH : LOW);
}

inline void ledStatusUpdate(PCF8575IO &pcf)
{
    bool anyPressed = false;
    for (int i = 0; i < IN_COUNT; i++)
    {
        if (pcf.readInput(i) == LOW) // INPUT_PULLUP: nhấn = LOW
        
        {
            anyPressed = true;
            break;
        }
    }

    bool isManMode = (pcf.readAutoMan() == LOW); // INPUT_PULLUP: LOW = MAN
    setStatusLed(anyPressed, isManMode);
}
