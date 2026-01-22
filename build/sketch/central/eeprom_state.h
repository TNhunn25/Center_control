#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\central\\eeprom_state.h"
#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

static const size_t EEPROM_STATE_SIZE = 256;
static const uint32_t EEPROM_STATE_MAGIC = 0x4F555430; // "OUT0"
static const int EEPROM_STATE_ADDR = 0;
static const uint32_t EEPROM_THRESH_MAGIC = 0x54485230; // "THR0"
static const uint32_t EEPROM_FLUSH_MS = 300; //eepromStatemarkDirty() gọi và sau ~300ms sẽ ghi vào eeprom 

struct PersistedState
{
    uint32_t magic;
    uint8_t out[OUT_COUNT];
};

struct PersistedThresholds
{
    uint32_t magic;
    Thresholds t;
};

static const int EEPROM_THRESH_ADDR = EEPROM_STATE_ADDR + sizeof(PersistedState);

static bool eepromDirty = false;
static uint32_t eepromDirtyMs = 0;
static bool eepromSuppress = false;

// Khởi tạo EEPROM với kích thước lưu trạng thái
inline void eepromStateBegin()
{
    EEPROM.begin(EEPROM_STATE_SIZE);
}

// Đánh dấu dữ liệu trạng thái cần ghi lại vào eeprom
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

// Nạp trạng thái từ EEPROM vào buffer mà KHÔNG áp ra output.
inline bool eepromStateRead(bool outState[OUT_COUNT])
{
    PersistedState st{};
    EEPROM.get(EEPROM_STATE_ADDR, st);
    if (st.magic != EEPROM_STATE_MAGIC)
        return false;

    for (int i = 0; i < OUT_COUNT; i++)
        outState[i] = st.out[i] != 0;
    return true;
}

inline void eepromThresholdsSave(const Thresholds &t)
{
    PersistedThresholds st{};
    st.magic = EEPROM_THRESH_MAGIC;
    st.t = t;
    EEPROM.put(EEPROM_THRESH_ADDR, st);
    EEPROM.commit();
}

inline bool eepromThresholdsLoad(Thresholds &t)
{
    PersistedThresholds st{};
    EEPROM.get(EEPROM_THRESH_ADDR, st);
    if (st.magic != EEPROM_THRESH_MAGIC)
        return false;
    t = st.t;
    return true;
}
