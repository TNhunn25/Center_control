#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\central\\led_status.h"
#pragma once
#include <Arduino.h>
#include "config.h"
#include "pcf8575_io.h"
#include "../led_status.h"

// Status LEDs (ESP32 GPIO)
static const uint8_t LED_RUN_PIN = 1; // xanh
static const uint8_t LED_AM_PIN = 2;  // do

static LedStatus runLed;

inline void ledStatusBegin()
{
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(LED_AM_PIN, OUTPUT);
    runLed.begin(LED_RUN_PIN, true);
}

// Giữ nguyên cấu trúc LED AUTO/MAN trong update() hiện tại (AM pin).
inline void setStatusLed(bool run_on, bool am_on)
{
    digitalWrite(LED_AM_PIN, am_on ? LOW : HIGH);
    (void)run_on;
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

    bool isManMode = (pcf.readAutoMan() == HIGH); // INPUT_PULLUP: LOW = MAN
    setStatusLed(anyPressed, isManMode);

    // LED_RUN_PIN chạy cùng logic và timing với led_status.begin(33, true)
    runLed.update();
}
