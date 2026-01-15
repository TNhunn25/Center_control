#ifndef AUTO_MAN_H
#define AUTO_MAN_H

#include <Arduino.h>
#include "PCF8575.h"

// AUTO/MAN switch
// AUTO: allow remote commands.
// MAN: local buttons only.

inline void setupAutoManMode()
{
    PcfIo::begin();
}

inline bool isAutoMode()
{
    // INPUT_PULLUP: HIGH=AUTO, LOW=MAN
    return PcfIo::readAutoMode();
}

#endif // AUTO_MAN_H
