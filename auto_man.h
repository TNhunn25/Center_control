#ifndef AUTO_MAN_H
#define AUTO_MAN_H

#include <Arduino.h>
#include "pcf8575_io.h"

// Chế độ AUTO/MAN:
// AUTO: cho phép nhận lệnh remote.
// MAN: chặn remote, cho phép nhấn nút để toggle output.

// AUTO/MAN switch
extern PCF8575IO pcf;

inline void setupAutoManMode()
{
}

inline bool isAutoMode()
{
    // INPUT_PULLUP: HIGH=AUTO, LOW=MAN
    return pcf.readAutoMan() == HIGH;
}

#endif // AUTO_MAN_H