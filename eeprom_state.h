#pragma once
#include <Arduino.h>
#include <EEPROM.h>

static const size_t EEPROM_STATE_SIZE = 64;
static const uint32_t EEPROM_STATE_MAGIC = 0x4F555430; // "OUT0"
static const int EEPROM_STATE_ADDR = 0;
static const uint32_t EEPROM_FLUSH_MS = 300;

struct PersistedState
{
    uint32_t magic;
    uint8_t out[4];
};

static bool eepromDirty = false;
static uint32_t eepromDirtyMs = 0;
static bool eepromSuppress = false;

inline void eepromStateBegin()
{
    EEPROM.begin(EEPROM_STATE_SIZE);
}

inline void eepromStateMarkDirty()
{
    if (eepromSuppress)
        return;
    eepromDirty = true;
    eepromDirtyMs = millis();
}

inline void eepromStateUpdate(const bool outState[4])
{
    if (!eepromDirty)
        return;
    if (millis() - eepromDirtyMs < EEPROM_FLUSH_MS)
        return;

    PersistedState st{};
    st.magic = EEPROM_STATE_MAGIC;
    for (int i = 0; i < 4; i++)
        st.out[i] = outState[i] ? 1 : 0;

    EEPROM.put(EEPROM_STATE_ADDR, st);
    EEPROM.commit();
    eepromDirty = false;
}

inline void eepromStateLoad(void (*applyOutput)(uint8_t, bool))
{
    PersistedState st{};
    EEPROM.get(EEPROM_STATE_ADDR, st);
    if (st.magic != EEPROM_STATE_MAGIC)
        return;

    eepromSuppress = true;
    for (int i = 0; i < 4; i++)
        applyOutput(i, st.out[i] != 0);
    eepromSuppress = false;
}
