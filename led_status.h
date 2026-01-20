#pragma once
#include <Arduino.h>
#include "config.h"
class LedStatus
{
public:
    enum State : uint8_t
    {
        STATE_NORMAL,
        STATE_NO_DATA_SERIAL,
        STATE_NO_LINK,
        STATE_ACTIVE_DATA_ALL,
        STATE_OFF

    };

    LedStatus();

    /**
     * @param pin Chân LED
     * @param activeHigh true: HIGH bật, false: LOW bật (active-low)
     */
    void begin(uint8_t pin, bool activeHigh = true);

    /** Set trạng thái ở bất kỳ đâu, LED sẽ đổi nháy theo state mới */
    void setState(State s);

    /** Lấy trạng thái hiện tại */
    State getState() const;

    /** Gọi liên tục trong loop() để LED nháy đúng theo state */
    void update();

private:
    void getBlinkTiming(State state, uint32_t &onTimeMs, uint32_t &offTimeMs);
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
