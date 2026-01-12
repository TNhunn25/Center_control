#ifndef SERIAL_CONTROL_H
#define SERIAL_CONTROL_H

#include <Arduino.h>

// Dùng chung với opcode (PC JSON) ở chỗ khác
extern bool outState[4];
extern void applyIOCommand(bool o1, bool o2, bool o3, bool o4);

extern bool swModeOverride;
extern bool swAutoMode;
extern bool getEffectiveAutoMode();
extern bool serialTextPushRequested;
extern bool serialTextPushForce;

// Trả true nếu line này là lệnh text và đã xử lý, false nếu không phải
inline bool handleSerialTextLine(String s)
{
    s.trim();
    s.toUpperCase();
    if (!s.length())
        return false;

    // MODE (liên quan opcode set mode)
    if (s == "AUTO")
    {
        swModeOverride = true;
        swAutoMode = true;
        Serial.println("[OK] AUTO");
        return true;
    }
    if (s == "MAN")
    {
        swModeOverride = true;
        swAutoMode = false;
        Serial.println("[OK] MAN");
        return true;
    }

    // AUTO khóa điều khiển tay
    if (getEffectiveAutoMode())
    {
        Serial.println("[DENY] AUTO");
        return true;
    }

    // IN1..IN4 ON/OFF (điều khiển chân/kênh qua applyIOCommand)
    for (int i = 1; i <= 4; i++)
    {
        bool o1 = outState[0], o2 = outState[1], o3 = outState[2], o4 = outState[3];

        if (s == "IN" + String(i) + " ON")
        {
            if (i == 1)
                o1 = true;
            if (i == 2)
                o2 = true;
            if (i == 3)
                o3 = true;
            if (i == 4)
                o4 = true;
            applyIOCommand(o1, o2, o3, o4);
            serialTextPushRequested = true;
            serialTextPushForce = false;
            Serial.println("[OK]");
            return true;
        }

        if (s == "IN" + String(i) + " OFF")
        {
            if (i == 1)
                o1 = false;
            if (i == 2)
                o2 = false;
            if (i == 3)
                o3 = false;
            if (i == 4)
                o4 = false;
            applyIOCommand(o1, o2, o3, o4);
            serialTextPushRequested = true;
            serialTextPushForce = false;
            Serial.println("[OK]");
            return true;
        }
    }

    return false;
}

#endif // SERIAL_CONTROL_H
