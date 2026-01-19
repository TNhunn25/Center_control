#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\MICS6814.h"
// MICS6814.h (header - chỉ phần thay đổi chính)
#ifndef MICS6814_H
#define MICS6814_H

#include <Arduino.h>

enum gas_t
{
    CO,
    NO2,
    NH3
};
enum channel_t
{
    CH_CO,
    CH_NO2,
    CH_NH3
};

class MICS6814
{
public:
    MICS6814();

    // Đo thông thường (nhanh, không blocking)
    float measure(gas_t gas);
    void begin(int pinCO, int pinNO2, int pinNH3);
    // Non-blocking calibration
    void calibrate(uint16_t maxSeconds); // Bắt đầu calibrate, tối đa bao nhiêu giây
    bool updateCalibrate();                    // Gọi liên tục trong loop(), trả về true khi xong
    bool isCalibrated() const { return _calibrated; }

    // Load calibration thủ công (nếu đã lưu trước)
    void loadCalibrationData(uint16_t baseNH3, uint16_t baseCO, uint16_t baseNO2);

    uint16_t getResistance(channel_t channel) const;
    uint16_t getBaseResistance(channel_t channel) const;
    float getCurrentRatio(channel_t channel) const;
    bool isConnected() const;

private:
    int _pinCO, _pinNO2, _pinNH3;

    uint16_t _baseNH3 = 0;
    uint16_t _baseCO = 0;
    uint16_t _baseNO2 = 0;

    bool _calibrated = false;

    // Dùng cho non-blocking calibrate
    static const uint8_t BUFFER_SIZE = 10; // 10 giây ổn định
    uint16_t bufferNH3[BUFFER_SIZE];
    uint16_t bufferCO[BUFFER_SIZE];
    uint16_t bufferNO2[BUFFER_SIZE];

    uint8_t idx = 0;
    unsigned long lastSampleTime = 0;
    uint16_t timeoutStart = 0;
    uint16_t maxCalibrateSeconds = 180;
};

#endif