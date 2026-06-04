#pragma once
#include <Arduino.h>
#include "config.h"
#include "pcf8575_io.h"
#include "../led_status.h"

// Status LEDs (ESP32 GPIO)
static const uint8_t LED_RUN_PIN = 1; // xanh
static const uint8_t LED_AM_PIN = 2;  // do
static constexpr bool LED_AM_ACTIVE_LOW = false;

static LedStatus runLed;

inline void ledStatusBegin()
{
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(LED_AM_PIN, OUTPUT);
    digitalWrite(LED_AM_PIN, LED_AM_ACTIVE_LOW ? HIGH : LOW);
    runLed.begin(LED_RUN_PIN, true);
}

//-----------------------------------------------------
// Giữ nguyên cấu trúc LED AUTO/MAN trong update() hiện tại (AM pin).
inline void setStatusLed(bool run_on, bool am_on)  //Bộ backup
{
    digitalWrite(LED_AM_PIN, LED_AM_ACTIVE_LOW ? (am_on ? LOW : HIGH) : (am_on ? HIGH : LOW));
    (void)run_on;
}

// inline void setStatusLed(bool run_on, bool am_on)
// {
//     digitalWrite(LED_AM_PIN, am_on ? LOW : HIGH); // Bộ Hòa Lân
//     (void)run_on;
// }

inline void ledStatusUpdate(PCF8575IO &pcf, ManualPcf8575IO &manualPcf)
{
    bool anyPressed = false;
    for (int i = 0; i < IN_COUNT; i++)
    {
        if (manualPcf.isButtonPressed(i))
        {
            anyPressed = true;
            break;
        }
    }

    const bool isAutoMode = pcf.readAutoMan();
    static bool modeKnown = false;
    static bool lastAutoMode = false;
    if (!modeKnown || lastAutoMode != isAutoMode)
    {
        modeKnown = true;
        lastAutoMode = isAutoMode;
        Serial.print(F("[LED][AM] Mode LED="));
        Serial.println(isAutoMode ? F("AUTO") : F("MAN"));
    }
    setStatusLed(anyPressed, isAutoMode);

    // LED_RUN_PIN chạy cùng logic và timing với led_status.begin(33, true)
    runLed.update();
}
