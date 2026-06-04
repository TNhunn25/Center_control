#pragma once
#include <Arduino.h>
#include "config.h"

class LedStatus
{
public:
    enum State : uint8_t
    {
        STATE_NORMAL,
        STATE_NO_MQTT,
        STATE_NO_LINK,
        STATE_WIFI_CONNECTED,
        STATE_ACTIVE_DATA_ALL,
        STATE_CONFIG_HOLD,
        STATE_CONFIG_ACTIVE,
        STATE_OFF

    };

    LedStatus();

    // Khoi tao chan LED va kieu kich muc logic cua LED.
    void begin(uint8_t pin, bool activeHigh = true);

    // Gan trang thai LED mong muon de cap nhat nhip nhay tuong ung.
    void setState(State s);

    // Lay trang thai LED hien tai.
    State getState() const;

    // Goi lien tuc trong loop() de LED nhay dung theo trang thai he thong.
    void update();

private:
    // Tra bang thoi gian sang/tat ung voi tung trang thai LED.
    void getBlinkTiming(State state, uint32_t &onTimeMs, uint32_t &offTimeMs);

    // Ghi muc logic thuc te ra chan LED theo kieu active-high/active-low.
    void writeLed(bool on);

private:
    uint8_t _pin;
    bool _activeHigh;

    State _state;
    State _lastState;

    bool _ledOn;
    uint32_t _lastToggleMs;
    uint32_t _onTimeMs;
    uint32_t _offTimeMs;
};
