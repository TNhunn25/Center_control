#pragma once

#include <Arduino.h>
#include <Wire.h>

class LcdI2C
{
public:
    LcdI2C(uint8_t address = 0x22, uint8_t columns = 16, uint8_t rows = 2, TwoWire &wire = Wire)
        : address_(address), columns_(columns), rows_(rows), wire_(wire)
    {
    }

    bool begin(int sda = -1, int scl = -1)
    {
        if (sda >= 0 && scl >= 0)
            wire_.begin(sda, scl);
        else
            wire_.begin();

        delay(50);

        expanderWrite(backlightState_);
        delay(1000);

        write4bits(0x03 << 4);
        delayMicroseconds(4500);

        write4bits(0x03 << 4);
        delayMicroseconds(4500);

        write4bits(0x03 << 4);
        delayMicroseconds(150);

        write4bits(0x02 << 4);

        command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
        command(LCD_DISPLAYCONTROL | LCD_DISPLAYON);
        command(LCD_CLEARDISPLAY);
        delayMicroseconds(2000);
        command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
        home();

        initialized_ = true;
        return true;
    }

    bool init(int sda = -1, int scl = -1)
    {
        return begin(sda, scl);
    }

    void clear()
    {
        command(LCD_CLEARDISPLAY);
        delayMicroseconds(2000);
    }

    void home()
    {
        command(LCD_RETURNHOME);
        delayMicroseconds(2000);
    }

    void setCursor(uint8_t column, uint8_t row)
    {
        static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
        if (row >= rows_)
            row = rows_ - 1;
        command(LCD_SETDDRAMADDR | (column + rowOffsets[row]));
    }

    void display()
    {
        displayControl_ |= LCD_DISPLAYON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void noDisplay()
    {
        displayControl_ &= ~LCD_DISPLAYON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void cursor()
    {
        displayControl_ |= LCD_CURSORON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void noCursor()
    {
        displayControl_ &= ~LCD_CURSORON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void blink()
    {
        displayControl_ |= LCD_BLINKON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void noBlink()
    {
        displayControl_ &= ~LCD_BLINKON;
        command(LCD_DISPLAYCONTROL | displayControl_);
    }

    void backlight()
    {
        backlightState_ = LCD_BACKLIGHT;
        expanderWrite(0);
    }

    void noBacklight()
    {
        backlightState_ = 0x00;
        expanderWrite(0);
    }

    size_t print(const char *text)
    {
        if (text == nullptr)
            return 0;

        size_t count = 0;
        while (*text)
        {
            write(static_cast<uint8_t>(*text++));
            count++;
        }
        return count;
    }

    size_t print(const String &text)
    {
        return print(text.c_str());
    }

    size_t print(char c)
    {
        return write(static_cast<uint8_t>(c));
    }

    size_t print(int value)
    {
        return print(String(value));
    }

    size_t print(unsigned int value)
    {
        return print(String(value));
    }

    size_t print(long value)
    {
        return print(String(value));
    }

    size_t print(unsigned long value)
    {
        return print(String(value));
    }

    size_t print(float value, uint8_t decimals = 2)
    {
        return print(String((double)value, (unsigned int)decimals));
    }

    void printAt(uint8_t column, uint8_t row, const String &text)
    {
        setCursor(column, row);
        print(text);
    }

    void printLine(uint8_t row, const String &text)
    {
        setCursor(0, row);

        uint8_t count = 0;
        while (count < columns_ && count < text.length())
        {
            write(static_cast<uint8_t>(text[count]));
            count++;
        }

        while (count < columns_)
        {
            write(' ');
            count++;
        }
    }

    uint8_t columns() const
    {
        return columns_;
    }

    uint8_t rows() const
    {
        return rows_;
    }

    bool isInitialized() const
    {
        return initialized_;
    }

private:
    static constexpr uint8_t LCD_CLEARDISPLAY = 0x01;
    static constexpr uint8_t LCD_RETURNHOME = 0x02;
    static constexpr uint8_t LCD_ENTRYMODESET = 0x04;
    static constexpr uint8_t LCD_DISPLAYCONTROL = 0x08;
    static constexpr uint8_t LCD_FUNCTIONSET = 0x20;
    static constexpr uint8_t LCD_SETDDRAMADDR = 0x80;

    static constexpr uint8_t LCD_ENTRYLEFT = 0x02;
    static constexpr uint8_t LCD_ENTRYSHIFTDECREMENT = 0x00;

    static constexpr uint8_t LCD_DISPLAYON = 0x04;
    static constexpr uint8_t LCD_CURSORON = 0x02;
    static constexpr uint8_t LCD_BLINKON = 0x01;

    static constexpr uint8_t LCD_4BITMODE = 0x00;
    static constexpr uint8_t LCD_2LINE = 0x08;
    static constexpr uint8_t LCD_5x8DOTS = 0x00;

    static constexpr uint8_t LCD_BACKLIGHT = 0x08;
    static constexpr uint8_t ENABLE_BIT = 0x04;
    static constexpr uint8_t READ_WRITE_BIT = 0x02;
    static constexpr uint8_t REGISTER_SELECT_BIT = 0x01;

    void command(uint8_t value)
    {
        send(value, 0);
    }

    size_t write(uint8_t value)
    {
        send(value, REGISTER_SELECT_BIT);
        return 1;
    }

    void send(uint8_t value, uint8_t mode)
    {
        uint8_t highNibble = value & 0xF0;
        uint8_t lowNibble = (value << 4) & 0xF0;
        write4bits(highNibble | mode);
        write4bits(lowNibble | mode);
    }

    void write4bits(uint8_t value)
    {
        expanderWrite(value);
        pulseEnable(value);
    }

    void expanderWrite(uint8_t value)
    {
        wire_.beginTransmission(address_);
        wire_.write(value | backlightState_);
        wire_.endTransmission();
    }

    void pulseEnable(uint8_t value)
    {
        expanderWrite(value | ENABLE_BIT);
        delayMicroseconds(1);
        expanderWrite(value & ~ENABLE_BIT);
        delayMicroseconds(50);
    }

    uint8_t address_;
    uint8_t columns_;
    uint8_t rows_;
    TwoWire &wire_;
    uint8_t backlightState_ = LCD_BACKLIGHT;
    uint8_t displayControl_ = LCD_DISPLAYON;
    bool initialized_ = false;
};
