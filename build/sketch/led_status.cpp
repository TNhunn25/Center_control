#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\led_status.cpp"
#include "led_status.h"

LedStatus::LedStatus()
    : _pin(255),
      _activeHigh(true),
      _state(STATE_NORMAL),
      _lastState((State)255),
      _ledOn(false),
      _lastToggleMs(0),
      _onTimeMs(500),
      _offTimeMs(500)
{
}

void LedStatus::begin(uint8_t pin, bool activeHigh)
{
    _pin = pin;
    _activeHigh = activeHigh;

    pinMode(_pin, OUTPUT);
    writeLed(false);

    // force apply current state immediately
    _lastState = (State)255;
    _lastToggleMs = millis();
}

void LedStatus::setState(State s)
{
    _state = s;
}

LedStatus::State LedStatus::getState() const
{
    return _state;
}

void LedStatus::update()
{
    if (_pin == 255)
        return;
    const uint32_t now = millis();
    // Chọn state theo link ethernet + data serial
    State newState;
    if (!has_connect_link && !has_data_serial)
    {
        newState = STATE_NO_LINK;
    }
    else if (!has_connect_link && has_data_serial)
    {
        newState = STATE_OFF;
    }
    else if (has_connect_link && !has_data_serial)
    {
        newState = STATE_NO_DATA_SERIAL;
    }
    else
    {
        newState = STATE_ACTIVE_DATA_ALL;
    }

    // Nếu state thay đổi → reset chu kỳ nháy ngay lập tức
    if (newState != _state)
    {
        _state = newState;
        _lastState = (State)255; // ép buộc vào khối reset bên dưới
    }
    if (_state != _lastState)
    {
        _lastState = _state;
        if (_state == STATE_OFF)
        {
            _ledOn = false;
            writeLed(false);
            return;
        }

        if(_state == STATE_ACTIVE_DATA_ALL)   // bình thường => luôn sáng
        {
            _ledOn = true;
            writeLed(true); // luôn sáng
            return;         // không chạy nháy
        }

        getBlinkTiming(_state, _onTimeMs, _offTimeMs);

        _ledOn = true; // bắt đầu ON
        writeLed(true);
        _lastToggleMs = now;
        return;
    }
    if (_state == STATE_OFF)
    {
        _ledOn = false;
        writeLed(false);
        return;
    }

    const uint32_t interval = _ledOn ? _onTimeMs : _offTimeMs;
    if ((uint32_t)(now - _lastToggleMs) < interval)
        return;

    _ledOn = !_ledOn;
    writeLed(_ledOn);
    _lastToggleMs = now;
}

void LedStatus::writeLed(bool on)
{
    if (_activeHigh)
        digitalWrite(_pin, on ? HIGH : LOW);
    else
        digitalWrite(_pin, on ? LOW : HIGH);
}

void LedStatus::getBlinkTiming(State state, uint32_t &onTimeMs, uint32_t &offTimeMs)
{
    switch (state)
    {
    case STATE_NORMAL:
        onTimeMs = 500UL;
        offTimeMs = 500UL;
        break;
    case STATE_NO_DATA_SERIAL:
        onTimeMs = 500UL;
        offTimeMs = 500UL;
        break;
    case STATE_NO_LINK:
        onTimeMs = 200UL;
        offTimeMs = 200UL;
        break;
    case STATE_ACTIVE_DATA_ALL:
        onTimeMs = 0;
        offTimeMs = 0;
        break;
    case STATE_OFF:
        onTimeMs = 0;
        offTimeMs = 0;
        break;
    }
}
