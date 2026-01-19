#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\MICS6814.cpp"
// MICS6814.cpp (implementation đã sửa)

#include "MICS6814.h"

MICS6814::MICS6814()
{
}
void MICS6814::begin(int pinCO, int pinNO2, int pinNH3)
{
    _pinCO = pinCO;
    _pinNO2 = pinNO2;
    _pinNH3 = pinNH3;

    pinMode(_pinCO, INPUT);
    pinMode(_pinNO2, INPUT);
    pinMode(_pinNH3, INPUT);
}

// --------------------- NON-BLOCKING CALIBRATION ---------------------

void MICS6814::calibrate(uint16_t maxSeconds)
{
    maxCalibrateSeconds = maxSeconds;
    timeoutStart = millis() / 1000; // giây hiện tại
    idx = 0;
    _calibrated = false;

    // Đặt buffer về 0
    memset(bufferNH3, 0, sizeof(bufferNH3));
    memset(bufferCO, 0, sizeof(bufferCO));
    memset(bufferNO2, 0, sizeof(bufferNO2));
    lastSampleTime = millis();
}

bool MICS6814::updateCalibrate()
{
    if (_calibrated)
        return true;

    // Timeout bảo vệ (tránh treo mãi mãi)
    if ((millis() / 1000 - timeoutStart) > maxCalibrateSeconds)
    {
        // Dùng giá trị trung bình hiện có làm baseline
        uint32_t sumNH3 = 0, sumCO = 0, sumNO2 = 0;
        for (uint8_t i = 0; i < BUFFER_SIZE; i++)
        {
            sumNH3 += bufferNH3[i];
            sumCO += bufferCO[i];
            sumNO2 += bufferNO2[i];
        }
        _baseNH3 = sumNH3 / BUFFER_SIZE;
        _baseCO = sumCO / BUFFER_SIZE;
        _baseNO2 = sumNO2 / BUFFER_SIZE;
        _calibrated = true;
        return true;
    }

    // Lấy mẫu mỗi 1 giây
    if (millis() - lastSampleTime < 1000)
        return false;

    lastSampleTime = millis();

    // Đọc trung bình 5 lần để giảm nhiễu (nhanh, không delay)
    auto readAvg = [](int pin) -> uint16_t
    {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 5; i++)
        {
            sum += analogRead(pin);
        }
        return sum / 5;
    };

    uint16_t curNH3 = readAvg(_pinNH3);
    uint16_t curCO = readAvg(_pinCO);
    uint16_t curNO2 = readAvg(_pinNO2);

    // Cập nhật buffer vòng
    bufferNH3[idx] = curNH3;
    bufferCO[idx] = curCO;
    bufferNO2[idx] = curNO2;

    // Chỉ kiểm tra ổn định khi buffer đã đầy (sau 10 mẫu)
    if (idx == BUFFER_SIZE - 1)
    {
        uint32_t sumNH3 = 0, sumCO = 0, sumNO2 = 0;
        for (uint8_t i = 0; i < BUFFER_SIZE; i++)
        {
            sumNH3 += bufferNH3[i];
            sumCO += bufferCO[i];
            sumNO2 += bufferNO2[i];
        }

        uint16_t avgNH3 = sumNH3 / BUFFER_SIZE;
        uint16_t avgCO = sumCO / BUFFER_SIZE;
        uint16_t avgNO2 = sumNO2 / BUFFER_SIZE;

        // Kiểm tra độ lệch < 5 (có thể điều chỉnh)
        const uint8_t delta = 5;
        bool stableNH3 = abs((int)avgNH3 - (int)curNH3) < delta;
        bool stableCO = abs((int)avgCO - (int)curCO) < delta;
        bool stableNO2 = abs((int)avgNO2 - (int)curNO2) < delta;

        if (stableNH3 && stableCO && stableNO2)
        {
            _baseNH3 = avgNH3;
            _baseCO = avgCO;
            _baseNO2 = avgNO2;
            _calibrated = true;
            return true;
        }
    }

    idx = (idx + 1) % BUFFER_SIZE;
    return false;
}

// --------------------- ĐO BÌNH THƯỜNG (NHANH) ---------------------

uint16_t MICS6814::getResistance(channel_t channel) const
{
    int pin;
    switch (channel)
    {
    case CH_CO:
        pin = _pinCO;
        break;
    case CH_NO2:
        pin = _pinNO2;
        break;
    case CH_NH3:
        pin = _pinNH3;
        break;
    default:
        return 0;
    }

    uint32_t sum = 0;
    const uint8_t samples = 20; // đủ để giảm nhiễu, rất nhanh
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += analogRead(pin);
    }
    return sum / samples;
}

uint16_t MICS6814::getBaseResistance(channel_t channel) const
{
    switch (channel)
    {
    case CH_NH3:
        return _baseNH3;
    case CH_CO:
        return _baseCO;
    case CH_NO2:
        return _baseNO2;
    default:
        return 0;
    }
}

float MICS6814::getCurrentRatio(channel_t channel) const
{
    float base = (float)getBaseResistance(channel);
    float curr = (float)getResistance(channel);

    if (base == 0)
        return -1.0;

    // Công thức chuyển đổi từ giá trị ADC sang tỷ lệ kháng trở
    return (curr / base) * (1023.0f - base) / (1023.0f - curr);
}

float MICS6814::measure(gas_t gas)
{
    if (!_calibrated)
        return -1.0; // Chưa calibrate

    float ratio = getCurrentRatio(gas == CO ? CH_CO : gas == NO2 ? CH_NO2
                                                                 : CH_NH3);

    if (ratio <= 0 || isnan(ratio))
        return -1.0;

    float c = 0;
    switch (gas)
    {
    case CO:
        c = pow(ratio, -1.179) * 4.385;
        break;
    case NO2:
        c = pow(ratio, 1.007) / 6.855;
        break;
    case NH3:
        c = pow(ratio, -1.67) / 1.47;
        break;
    }

    return isnan(c) ? -1.0 : c;
}

void MICS6814::loadCalibrationData(uint16_t baseNH3, uint16_t baseCO, uint16_t baseNO2)
{
    _baseNH3 = baseNH3;
    _baseCO = baseCO;
    _baseNO2 = baseNO2;
    _calibrated = true;
}
// --------------------- KIỂM TRA KẾT NỐI CẢM BIẾN ---------------------
bool MICS6814::isConnected() const
{
    // Nếu chưa bắt đầu begin() → chắc chắn không kết nối
    if (_pinCO == 0 && _pinNO2 == 0 && _pinNH3 == 0)
        return false;

    // Đọc trung bình 10 lần mỗi kênh để giảm nhiễu
    auto readAvg = [](int pin) -> uint16_t
    {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 10; i++)
        {
            sum += analogRead(pin);
        }
        return sum / 10;
    };

    uint16_t valCO = readAvg(_pinCO);
    uint16_t valNO2 = readAvg(_pinNO2);
    uint16_t valNH3 = readAvg(_pinNH3);

    // Nếu không cắm cảm biến:
    // - Tất cả 3 chân sẽ đọc được giá trị gần 0 hoặc gần 4095 (tùy board)
    // - Hoặc dao động rất mạnh (nhưng trung bình 10 lần sẽ ổn)
    bool allZero = (valCO < 50) && (valNO2 < 50) && (valNH3 < 50);
    bool allFull = (valCO > 4045) && (valNO2 > 4045) && (valNH3 > 4045);

    if (allZero || allFull)
        return false; // Không có cảm biến

    // Nếu có cảm biến: ít nhất 1 kênh phải có giá trị "hợp lý" (100 - 4000)
    // và 3 kênh không được giống hệt nhau hoàn toàn (do cảm biến thực luôn có chút khác biệt)
    bool reasonable = (valCO > 100 && valCO < 4000) ||
                      (valNO2 > 100 && valNO2 < 4000) ||
                      (valNH3 > 100 && valNH3 < 4000);

    if (!reasonable)
        return false;

    // Bonus: kiểm tra 3 giá trị không quá giống nhau (tránh nhầm với nhiễu)
    int diff1 = abs((int)valCO - (int)valNO2);
    int diff2 = abs((int)valCO - (int)valNH3);
    if (diff1 < 10 && diff2 < 10 && abs((int)valNO2 - (int)valNH3) < 10)
        return false;

    return true; 
}