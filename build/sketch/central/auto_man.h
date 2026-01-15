#line 1 "D:\\phunsuong\\master\\master\\central\\auto_man.h"
#ifndef AUTO_MAN_H
#define AUTO_MAN_H

#include <Arduino.h>

// Chế độ AUTO/MAN:
// AUTO: cho phép nhận lệnh remote.
// MAN: chặn remote, cho phép nhấn nút để toggle output.

// AUTO/MAN switch
static const uint8_t MODE_PIN = 23; // INPUT_PULLUP: HIGH=AUTO, LOW=MAN (đổi nếu cần)

inline void setupAutoManMode()
{
    pinMode(MODE_PIN, INPUT_PULLUP);
}

inline bool isAutoMode()
{
    // INPUT_PULLUP: HIGH=AUTO, LOW=MAN
    return digitalRead(MODE_PIN) == HIGH;
}

#endif // AUTO_MAN_H