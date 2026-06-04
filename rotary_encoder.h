#pragma once

#include <Arduino.h>

class RotaryEncoderInput
{
public:
    enum TurnDirection : int8_t
    {
        None = 0,
        Clockwise = 1,
        CounterClockwise = -1
    };

    RotaryEncoderInput(uint8_t clkPin, uint8_t dtPin, int8_t swPin = -1, bool usePullup = true)
        : clkPin_(clkPin), dtPin_(dtPin), swPin_(swPin), usePullup_(usePullup)
    {
    }

    void begin()
    {
        pinMode(clkPin_, usePullup_ ? INPUT_PULLUP : INPUT);
        pinMode(dtPin_, usePullup_ ? INPUT_PULLUP : INPUT);

        if (hasButton())
            pinMode(swPin_, usePullup_ ? INPUT_PULLUP : INPUT);

        lastClkState_ = digitalRead(clkPin_);
        lastButtonReading_ = hasButton() ? digitalRead(swPin_) : HIGH;
        buttonState_ = lastButtonReading_;
    }

    TurnDirection update()
    {
        TurnDirection direction = readRotation();
        updateButton();
        return direction;
    }

    int8_t consumeStep()
    {
        return static_cast<int8_t>(update());
    }

    int16_t updatePage(int16_t currentPage, int16_t pageCount, bool wrap = true)
    {
        if (pageCount <= 0)
            return currentPage;

        const int8_t step = consumeStep();
        if (step == 0)
            return currentPage;

        int16_t nextPage = currentPage + step;
        if (wrap)
        {
            if (nextPage < 0)
                nextPage = pageCount - 1;
            else if (nextPage >= pageCount)
                nextPage = 0;
        }
        else
        {
            if (nextPage < 0)
                nextPage = 0;
            else if (nextPage >= pageCount)
                nextPage = pageCount - 1;
        }

        return nextPage;
    }

    bool isButtonPressed() const
    {
        return hasButton() ? (buttonState_ == LOW) : false;
    }

    bool wasButtonClicked()
    {
        const bool clicked = buttonClicked_;
        buttonClicked_ = false;
        return clicked;
    }

    bool hasButton() const
    {
        return swPin_ >= 0;
    }

private:
    TurnDirection readRotation()
    {
        const uint8_t clkState = digitalRead(clkPin_);
        if (clkState == lastClkState_)
            return None;

        lastClkState_ = clkState;

        const uint8_t dtState = digitalRead(dtPin_);
        return (dtState == clkState) ? Clockwise : CounterClockwise;
    }

    void updateButton()
    {
        if (!hasButton())
            return;

        const uint32_t now = millis();
        const uint8_t reading = digitalRead(swPin_);

        if (reading != lastButtonReading_)
            lastDebounceMs_ = now;

        if ((now - lastDebounceMs_) > debounceMs_ && reading != buttonState_)
        {
            buttonState_ = reading;
            if (buttonState_ == LOW)
                buttonClicked_ = true;
        }

        lastButtonReading_ = reading;
    }

    uint8_t clkPin_;
    uint8_t dtPin_;
    int8_t swPin_;
    bool usePullup_;

    uint8_t lastClkState_ = HIGH;
    uint8_t lastButtonReading_ = HIGH;
    uint8_t buttonState_ = HIGH;
    bool buttonClicked_ = false;
    uint32_t lastDebounceMs_ = 0;
    uint16_t debounceMs_ = 25;
};

