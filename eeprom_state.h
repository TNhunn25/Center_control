#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

static const size_t EEPROM_STATE_SIZE = 64;
static const uint32_t EEPROM_STATE_MAGIC = 0x4F555430; // "OUT0"
static const int EEPROM_STATE_ADDR = 0;
static const uint32_t EEPROM_FLUSH_MS = 300;

struct PersistedState
{
    uint32_t magic;
    uint8_t out[OUT_COUNT];
};

static bool eepromDirty = false;
static uint32_t eepromDirtyMs = 0;
static bool eepromSuppress = false;

//Khởi tạo EEPROM với kích thước lưu trạng thái
inline void eepromStateBegin()
{
    EEPROM.begin(EEPROM_STATE_SIZE);
}

//Đánh dấu dữ liệu trạng thái cần ghi lại vào eeprom 
inline void eepromStateMarkDirty()
{
    if (eepromSuppress)
        return;
    eepromDirty = true;
    eepromDirtyMs = millis();
}

// Ghi trạng thái output xuống EEPROM sau thời gian chống ghi liên tục.
inline void eepromStateUpdate(const bool outState[OUT_COUNT])
{
    if (!eepromDirty)
        return;
    if (millis() - eepromDirtyMs < EEPROM_FLUSH_MS)
        return;

    PersistedState st{};
    st.magic = EEPROM_STATE_MAGIC;
    for (int i = 0; i < OUT_COUNT; i++)
        st.out[i] = outState[i] ? 1 : 0;

    EEPROM.put(EEPROM_STATE_ADDR, st);
    EEPROM.commit();
    eepromDirty = false;
}

// Nạp trạng thái từ EEPROM và áp dụng ra output.
inline void eepromStateLoad(void (*applyOutput)(uint8_t, bool))
{
    PersistedState st{};
    EEPROM.get(EEPROM_STATE_ADDR, st);
    if (st.magic != EEPROM_STATE_MAGIC)
        return;

    eepromSuppress = true;
    for (int i = 0; i < OUT_COUNT; i++)
        applyOutput(i, st.out[i] != 0);
    eepromSuppress = false;
}
