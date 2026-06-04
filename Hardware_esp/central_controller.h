#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "auto_man.h"
#include "eeprom_state.h"
#include "Pcf8575_io.h"
#include "led_status.h"

extern PCF8575IO pcf;
extern ManualPcf8575IO manualPcf;

class CentralController
{
public:
    CentralController() {}

    // ================= BEGIN =================
    // Khá»Ÿi táº¡o controller trung tÃ¢m vÃ  tráº¡ng thÃ¡i IO.
    void begin(Stream &pcStream = Serial)
    {
        pc = &pcStream;
        // AUTO / MAN
        setupAutoManMode();

        // OUT_ACTIVE_LOW=true => OFF level = HIGH.
        // Pass the OFF level into PCF begin so the first I2C write after boot
        // already puts relay pins in OFF state, avoiding a boot pulse.
        const bool offLevel = OUT_ACTIVE_LOW ? true : false;
        pcf.begin(offLevel);

        // Force all relay outputs OFF before any EEPROM restore or MAN snapshot.
        // This prevents the previous saved state from making all relays pull in
        // first, then being corrected by the manual stagger sequence.
        forceAllOutputsOff(false);

        manualPcf.begin();
        ledStatusBegin();
        syncManualLeds();

        // IN + debounce init
        uint32_t now = millis();
        // static const bool OUT_ACTIVE_LOW = true;
        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
        {
            bool v = readManualButton(i);
            btn[i].stableLevel = v;
            btn[i].lastReading = v;
            btn[i].lastChangeMs = now;
            nutVuaNhan[i] = false;
            printManualButtonState(F("boot"), i, v);
        }

        // EEPROM
        eepromStateBegin();
        // instance = this;
        // eepromStateLoad(eepromThunk);
        // // Re-apply outputs sequentially to avoid inrush at startup.
        // applyIOStaggeredArray(outState);

        bool desiredOut[OUT_COUNT];
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            desiredOut[i] = offLevel;
        const bool hasSavedEepromState = eepromStateRead(desiredOut);
        lastAutoMode = isAutoMode();

        if (lastAutoMode)
        {
            // Boot dang o AUTO: relay phai OFF het truoc.
            // Sau khi setup/boot on dinh moi lay trang thai da luu EEPROM
            // va kich lai tung kenh tuan tu. KHONG doi MQTT o buoc boot nay.
            Serial.println(F("[CTRL] Boot AUTO: force OFF, then restore EEPROM sequentially after boot"));
            forceAllOutputsOff(false);
            autoWaitingForMqttCommand = false;
            bootAutoRestorePending = hasSavedEepromState;
            bootAutoRestoreAtMs = millis() + BOOT_RESTORE_DELAY_MS;
            for (uint8_t i = 0; i < OUT_COUNT; i++)
            {
                bootAutoRestoreState[i] = desiredOut[i];
                autoRestoreState[i] = desiredOut[i];
            }
            hasAutoRestore = hasSavedEepromState;
        }
        else
        {
            // Boot dang o MAN: bo qua trang thai EEPROM/AUTO.
            // Relay dau tien phai nam OFF het, sau do nut nao dang nhan
            // moi duoc dua vao hang doi bat tuan tu.
            Serial.println(F("[CTRL] Boot MAN: force OFF then apply pressed manual buttons sequentially"));
            forceAllOutputsOff(false);
            autoWaitingForMqttCommand = false;
            applyManualButtonsSnapshot(true);
        }

        // snapshot ban Ä‘áº§u
        luuMocTrangThai();
        // chá»‘ng máº¥t gÃ³i khi spam
        lastPushMs = millis();
        deferredPush = false;
    }

    // ================= HANDLE PC COMMAND =================
    // Xá»­ lÃ½ lá»‡nh tá»« PC (IO_COMMAND).
    bool handleCommand(const IoCommand &cmd)
    {
        switch (cmd.opcode)
        {
        case 254:
        {
            // tráº£ tráº¡ng thÃ¡i ngay (if changed)
            return false;
            // khÃ´ng snapshot á»Ÿ Ä‘Ã¢y Ä‘á»ƒ trÃ¡nh nuá»‘t event Ä‘ang chá» (deferred)
            return false;
        }

        case IO_COMMAND:
        {
            if (!isAutoMode())
            {
                Serial.println(F("[CTRL] Ignore MQTT command because mode=MAN"));
                return false;
            }
            bool desiredLevel[OUT_COUNT] = {};
            for (uint8_t i = 0; i < OUT_COUNT; i++)
                desiredLevel[i] = physicalOnToOutputLevel(commandPhysicalOn(cmd, i));

            Serial.print(F("[CTRL] Apply MQTT command"));
            for (uint8_t i = 0; i < OUT_COUNT; i++)
            {
                Serial.print(F(" out"));
                Serial.print(i + 1);
                Serial.print(F("="));
                Serial.print(commandPhysicalOn(cmd, i) ? F("ON") : F("OFF"));
            }
            Serial.println();

            // Neu MQTT/server gui lenh moi truoc khi restore EEPROM luc boot,
            // huy restore EEPROM de khong ghi de lenh moi.
            bootAutoRestorePending = false;
            autoWaitingForMqttCommand = false;
            captureManualOutputsArray(desiredLevel);
            applyIOStaggeredArray(desiredLevel);
            return true;
        }
        default:
            return false;
        }
    }

    // ================= LOOP =================
    // Cáº­p nháº­t Ä‘á»‹nh ká»³: mode, auto, manual, push tráº¡ng thÃ¡i, LED, EEPROM.
    void update()
    {
        bool autoMode = isAutoMode();

        // Neu dang o MAN, chi huy stagger cua AUTO.
        // Stagger do MAN tao ra (boot/chuyen MAN/nhan nhieu nut) van phai duoc chay tiep.
        if (!autoMode && staggerActive && !staggerIsManual)
            cancelStaggeredOutputs();
        processStaggeredOutputs();

        // Boot o AUTO: ban dau da tat het relay. Doi boot/setup on dinh xong
        // moi restore trang thai da luu EEPROM, va restore cung di tuan tu.
        if (bootAutoRestorePending)
        {
            if (!autoMode)
            {
                bootAutoRestorePending = false;
            }
            else if ((int32_t)(millis() - bootAutoRestoreAtMs) >= 0)
            {
                Serial.println(F("[CTRL] Boot AUTO: restore EEPROM outputs sequentially"));
                bootAutoRestorePending = false;
                applyIOStaggeredArray(bootAutoRestoreState, false);
            }
        }

        // if (autoMode != lastAutoMode)
        // {
        //     Serial.print(F("[CTRL] Mode changed to "));
        //     Serial.println(autoMode ? F("AUTO") : F("MAN"));
        //     if (autoMode)
        //     {
        //         // Vua chuyen tu MANUAL sang AUTO: PHAI nho lai trang thai AUTO
        //         // truoc do, khong doi MQTT. Yeu cau doi MQTT chi ap dung luc
        //         // KHOI DONG thiet bi o AUTO.
        //         autoWaitingForMqttCommand = false;
        //         if (hasAutoRestore)
        //         {
        //             Serial.println(F("[CTRL] Enter AUTO: restore previous AUTO outputs sequentially"));
        //             applyIOStaggeredArray(autoRestoreState, false);
        //         }
        //         else
        //         {
        //             // Truong hop chua tung co snapshot AUTO trong RAM, giu OFF an toan.
        //             // MQTT lenh moi van co the cap nhat sau do qua handleCommand().
        //             Serial.println(F("[CTRL] Enter AUTO: no previous AUTO snapshot, keep outputs OFF"));
        //             forceAllOutputsOff(false);
        //         }
        //     }
        //     else
        //     {
        //         autoWaitingForMqttCommand = false;
        //         bootAutoRestorePending = false;
        //         saveAutoRestoreState();
        //         autoOutputsOn = false;
        //         // Vua chuyen sang MAN: doc toan bo nut, LED sang/tat ngay theo nut.
        //         // Relay nao can ON se duoc bat tuan tu, khong dong loat.
        //         applyManualButtonsSnapshot(true);
        //     }
        //     lastAutoMode = autoMode;
        // }

        if (autoMode != lastAutoMode)
        {
            Serial.print(F("[CTRL] Mode changed to "));
            Serial.println(autoMode ? F("AUTO") : F("MAN"));

            if (autoMode)
            {
                // MANUAL -> AUTO:
                // Giữ nguyên trạng thái output hiện tại của tủ nguồn.
                // Không restore trạng thái AUTO cũ.
                // Không force OFF.
                // Không áp EEPROM.
                // SkillSwitch/server phải được đồng bộ theo trạng thái output hiện tại.
                Serial.println(F("[CTRL] Chọn chế độ AUTO: giữ nguyên các đầu ra hiện tại, đồng bộ trạng thái với SkillSwitch"));

                autoWaitingForMqttCommand = false;
                bootAutoRestorePending = false;

                // Hủy stagger manual nếu còn đang chạy, để tránh sau khi vào AUTO
                // manual queue tiếp tục thay đổi output.
                cancelStaggeredOutputs();

                // Đánh dấu có thay đổi để vòng auto-push gửi lại trạng thái hiện tại.
                outputChanged = true;
                deferredPush = true;

                // Cập nhật mốc input để không lấy trạng thái nút vật lý làm điều kiện điều khiển nữa.
                for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
                {
                    btn[i].lastReading = btn[i].stableLevel;
                    btn[i].lastChangeMs = millis();
                    nutVuaNhan[i] = false;
                }

                // LED manual trong AUTO bám theo output.
                syncManualLeds();
            }
            else
            {
                // AUTO -> MANUAL:
                // Không quan tâm trạng thái AUTO trước đó.
                // Output phải đồng bộ theo trạng thái nút vật lý hiện tại.
                Serial.println(F("[CTRL] Chọn chế độ MANUAL: áp dụng trạng thái nút vật lý"));

                autoWaitingForMqttCommand = false;
                bootAutoRestorePending = false;
                autoOutputsOn = false;

                // Không cần saveAutoRestoreState nữa vì MAN -> AUTO không restore trạng thái AUTO cũ.
                // saveAutoRestoreState();

                applyManualButtonsSnapshot(true);
            }

            lastAutoMode = autoMode;
        }

        // ===== MAN MODE: nháº¥n nÃºt tay thÃ¬ váº«n coi lÃ  thay Ä‘á»•i Ä‘á»ƒ auto_push Ä‘áº©y =====
        // if (!autoMode && !autoOutputsOn)
        if (!autoMode)
        {
            uint16_t manualSnapshot = 0;
            const bool hasManualSnapshotRead = manualPcf.readButtonsSnapshot(manualSnapshot);
            for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            {
                bool stableLevel = LOW; // Man = low
                const bool reading = hasManualSnapshotRead ? readManualButtonFromSnapshot(manualSnapshot, i) : readManualButton(i);
                if (debounceUpdate(i, reading, stableLevel))
                {
                    const bool pressed = isManualButtonPressed(stableLevel);
                    const bool level = physicalOnToOutputLevel(pressed);
                    printManualButtonState(pressed ? F("pressed") : F("released"), i, stableLevel);
                    if (pressed)
                        nutVuaNhan[i] = true;
                    Serial.print(F("[MANBTN] set ch="));
                    Serial.print(i + 1);
                    Serial.print(F(" out"));
                    Serial.print(i + 1);
                    Serial.print(F("="));
                    Serial.print(pressed ? F("ON") : F("OFF"));
                    Serial.print(F(" level="));
                    Serial.println(level ? F("HIGH") : F("LOW"));
                    requestManualOutput(i, level);
                    manualOutState[i] = level;
                    hasManualSnapshot = true;
                    // Đã check đúng
                    // Serial.printf("MAN: Button %d %s -> out%d %s\n",
                    //               i + 1,
                    //               pressed ? "pressed" : "released",
                    //               i + 1,
                    //               pressed ? "OFF" : "ON"); // released OFF Pressed ON
                }

                // Trong MAN, LED nút phải luôn bám theo trạng thái nút đã debounce:
                // chưa bấm = tắt, đang bấm = sáng. Đặt ở ngoài nhánh thay đổi
                // để nếu hàm LED status khác ghi đè thì vòng sau vẫn kéo về đúng.
                syncManualLedFromButton(i, stableLevel);
            }
        }

        // ===== AUTO_PUSH: luÃ´n cháº¡y khi cÃ³ thay Ä‘á»•i, KHÃ”NG Máº¤T GÃ“I =====
        tuDongDayNeuThayDoi();

        // 3) LED status

        // ORIG: setStatusLed(true, false) here forced AM LED off before
        // ledStatusUpdate() turned it back on, which made AUTO/MAN flicker.
        ledStatusUpdate(pcf, manualPcf);

        // ledStatusUpdate() có thể ghi lại các LED trên PCF manual.
        // Vì yêu cầu ở MAN là LED nút chỉ phụ thuộc nút nhấn, ép sync lại cuối vòng
        // bằng mức đã debounce, không đọc raw lại để tránh nháy do dội phím.
        if (!autoMode)
            syncManualLedsFromStableButtons();

        // EEPROM
        eepromStateUpdate(outState);
    }

    bool isAutoModeActive() const
    {
        return isAutoMode();
    }

    void copyOutputStates(bool outputs[OUT_COUNT]) const
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            outputs[i] = outState[i];
    }

    void copyPhysicalOutputStates(bool outputs[OUT_COUNT]) const
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            outputs[i] = outputLevelToPhysicalOn(outState[i]);
    }

    bool consumeOutputChanged()
    {
        const bool changed = outputChanged;
        outputChanged = false;
        return changed;
    }

private:
    static constexpr bool OUT_ACTIVE_LOW = true;
    static constexpr uint32_t DEBOUNCE_MS = 200; // chống dội input, tăng nếu có nhiều nút hoặc nút có tiếp điểm kém tăng lên 200
    static constexpr uint32_t CHONG_SPAM_MS = 120;
    static constexpr uint32_t SEQ_ON_DELAY_MS = 800;
    static constexpr uint32_t BOOT_RESTORE_DELAY_MS = 2000;

    // So kenh nut/LED manual that su can xu ly.
    // Dung BUTTON_COUNT/LED_COUNT thay vi IN_COUNT de CH5-CH8 van duoc sync LED
    // neu config cu con de IN_COUNT = 4.
    static constexpr uint8_t MANUAL_CH_COUNT =
        (OUT_COUNT < ManualPcf8575IO::BUTTON_COUNT ? (OUT_COUNT < ManualPcf8575IO::LED_COUNT ? OUT_COUNT : ManualPcf8575IO::LED_COUNT) : (ManualPcf8575IO::BUTTON_COUNT < ManualPcf8575IO::LED_COUNT ? ManualPcf8575IO::BUTTON_COUNT : ManualPcf8575IO::LED_COUNT));

    struct ButtonState
    {
        bool stableLevel;
        bool lastReading;
        uint32_t lastChangeMs;
    };

    Stream *pc = nullptr;

    bool outState[OUT_COUNT]{};
    ButtonState btn[MANUAL_CH_COUNT]{};
    bool nutVuaNhan[MANUAL_CH_COUNT]{};

    // mốc trạng thái để so sánh và quyết định có push hay không.
    bool mocOut[OUT_COUNT]{};
    bool mocIn[MANUAL_CH_COUNT]{};

    // chá»‘ng spam + chá»‘ng máº¥t event
    uint32_t lastPushMs = 0;
    bool deferredPush = false;

    // chá»‘ng gá»­i trÃ¹ng theo json
    bool autoOutputsOn = false;

    bool hasAutoRestore = false;
    bool autoRestoreState[OUT_COUNT]{};
    // Chi dung cho luc BOOT o AUTO: relay OFF het va cho lenh MQTT moi tu server.
    // Khi dang chay ma chuyen MANUAL -> AUTO thi khong dung co nay, ma restore snapshot AUTO cu.
    bool autoWaitingForMqttCommand = false;

    // Chi dung luc khoi dong o AUTO: tat het relay, doi boot on dinh,
    // roi restore trang thai EEPROM bang stagger. Neu MQTT gui lenh moi truoc
    // thoi diem restore thi huy restore EEPROM de lenh moi uu tien.
    bool bootAutoRestorePending = false;
    uint32_t bootAutoRestoreAtMs = 0;
    bool bootAutoRestoreState[OUT_COUNT]{};

    bool manualOutState[OUT_COUNT]{}; // Ä‘iá»u khiá»ƒn output
    bool hasManualSnapshot = false;
    bool lastAutoMode = true;
    bool outputChanged = false;
    bool staggerActive = false;
    bool staggerIsManual = false;
    bool staggerDesired[OUT_COUNT]{};
    uint8_t staggerIndex = 0;
    uint32_t nextStaggerMs = 0;

    bool physicalOnToOutputLevel(bool physicalOn) const
    {
        return OUT_ACTIVE_LOW ? !physicalOn : physicalOn;
    }

    bool outputLevelToPhysicalOn(bool outputLevel) const
    {
        return OUT_ACTIVE_LOW ? !outputLevel : outputLevel;
    }

    bool commandPhysicalOn(const IoCommand &cmd, uint8_t ch) const
    {
        switch (ch)
        {
        case 0:
            return cmd.out1;
        case 1:
            return cmd.out2;
        case 2:
            return cmd.out3;
        case 3:
            return cmd.out4;
        case 4:
            return cmd.out5;
        case 5:
            return cmd.out6;
        case 6:
            return cmd.out7;
        case 7:
            return cmd.out8;
        default:
            return false;
        }
    }

    bool isManualButtonPressed(bool level) const
    {
        return ManualPcf8575IO::isPressedLevel(level);
    }

    bool readManualButton(uint8_t ch)
    {
        return manualPcf.readButton(ch);
    }

    void saveAutoRestoreState()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            autoRestoreState[i] = outState[i];
        hasAutoRestore = true;

        Serial.print(F("[CTRL] Save AUTO restore outputs physical="));
        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            if (i > 0)
                Serial.print(F(","));
            Serial.print(outputLevelToPhysicalOn(autoRestoreState[i]) ? F("ON") : F("OFF"));
        }
        Serial.println();
    }

    bool readManualButtonFromSnapshot(uint16_t snapshot, uint8_t ch)
    {
        if (ch >= ManualPcf8575IO::BUTTON_COUNT)
            return ManualPcf8575IO::releasedLevel();

        const uint8_t pin = manualPcf.BUTTON_PINS[ch];
        if (pin == ManualPcf8575IO::NOT_USED || pin >= 16)
            return ManualPcf8575IO::releasedLevel();

        return ((snapshot >> pin) & 0x01u) != 0;
    }

    void syncManualLedFromOutput(uint8_t ch)
    {
        if (ch >= OUT_COUNT)
            return;
        manualPcf.writeLed(ch, outputLevelToPhysicalOn(outState[ch]));
    }

    void syncManualLedFromButton(uint8_t ch, bool buttonLevel)
    {
        if (ch >= ManualPcf8575IO::LED_COUNT)
            return;
        manualPcf.writeLed(ch, isManualButtonPressed(buttonLevel));
    }

    void syncManualLed(uint8_t ch)
    {
        if (isAutoMode())
        {
            syncManualLedFromOutput(ch);
            return;
        }

        syncManualLedFromButton(ch, readManualButton(ch));
    }

    void syncManualLeds()
    {
        for (uint8_t i = 0; i < OUT_COUNT && i < ManualPcf8575IO::LED_COUNT; i++)
            syncManualLed(i);
    }

    void syncManualLedsFromStableButtons()
    {
        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            syncManualLedFromButton(i, btn[i].stableLevel);
    }

    void printManualButtonState(const __FlashStringHelper *tag, uint8_t ch, bool level)
    {
        Serial.print(F("[MANBTN] "));
        Serial.print(tag);
        Serial.print(F(" ch="));
        Serial.print(ch + 1);
        Serial.print(F(" pcf2_pin="));
        if (ch < ManualPcf8575IO::BUTTON_COUNT)
            Serial.print(manualPcf.BUTTON_PINS[ch]);
        else
            Serial.print(F("NA"));
        Serial.print(F(" raw="));
        Serial.print(level ? F("HIGH") : F("LOW"));
        Serial.print(F(" pressed="));
        Serial.println(isManualButtonPressed(level) ? F("YES") : F("NO"));
    }

    void forceAllOutputsOff(bool markDirty)
    {
        const bool offLevel = OUT_ACTIVE_LOW ? true : false;
        cancelStaggeredOutputs();
        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            pcf.writeOutput(i, offLevel);
            outState[i] = offLevel;
            manualOutState[i] = offLevel;
        }
        outputChanged = false;
        if (markDirty)
        {
            outputChanged = true;
            eepromStateMarkDirty();
        }
    }

    // Ghi má»©c logic ra chÃ¢n output tÆ°Æ¡ng á»©ng trÃªn PCF8575.
    void writePin(uint8_t ch, bool on)
    {
        pcf.writeOutput(ch, on);
    }

    // LÆ°u tráº¡ng thÃ¡i output vÃ  ghi ra chÃ¢n; Ä‘Ã¡nh dáº¥u EEPROM náº¿u thay Ä‘á»•i.
    void writeOutput(uint8_t ch, bool on)
    {
        bool prevState = outState[ch]; // lÆ°u tráº¡ng thÃ¡i cÅ© Ä‘á»ƒ chá»‰ log khi cÃ³ thay Ä‘á»•i tháº­t
        outState[ch] = on;

        writePin(ch, on);
        syncManualLed(ch);
        Serial.print(F("[CTRL] writeOutput ch="));
        Serial.print(ch + 1);
        Serial.print(F(" level="));
        Serial.print(on ? F("HIGH") : F("LOW"));
        Serial.print(F(" physical="));
        Serial.println(on ? F("OFF") : F("ON"));
        if (prevState != on)
        {
            outputChanged = true;
            eepromStateMarkDirty();
        }
    }

    // Apply output states directly (no stagger). Works with OUT_COUNT channels.
    void applyIOArray(const bool desired[OUT_COUNT])
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            writeOutput(i, desired[i]);
    }

    // Backward-compatible helper for old 4-output callers.
    void applyIO(bool o1, bool o2, bool o3, bool o4)
    {
        bool desired[OUT_COUNT] = {};
        desired[0] = o1;
        desired[1] = o2;
        desired[2] = o3;
        desired[3] = o4;
        applyIOArray(desired);
    }

    // Apply outputs sequentially to avoid inrush. Works with OUT_COUNT channels.
    void applyIOStaggeredArray(const bool desired[OUT_COUNT], bool manualOwner = false)
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            staggerDesired[i] = desired[i];

        staggerIndex = 0;
        nextStaggerMs = millis();
        staggerIsManual = manualOwner;
        staggerActive = true;
        processStaggeredOutputs();
    }

    // Backward-compatible helper for old 4-output callers.
    void applyIOStaggered(bool o1, bool o2, bool o3, bool o4)
    {
        bool desired[OUT_COUNT] = {};
        desired[0] = o1;
        desired[1] = o2;
        desired[2] = o3;
        desired[3] = o4;
        applyIOStaggeredArray(desired, false);
    }

    void cancelStaggeredOutputs()
    {
        staggerActive = false;
        staggerIsManual = false;
    }

    // void processStaggeredOutputs()
    // {
    //     if (!staggerActive)
    //         return;

    //     const uint32_t now = millis();
    //     if ((int32_t)(now - nextStaggerMs) < 0)
    //         return;

    //     const bool onLevel = OUT_ACTIVE_LOW ? false : true;
    //     const bool offLevel = !onLevel;
    //     while (staggerIndex < OUT_COUNT)
    //     {
    //         // if (desired[i] && !outState[i])
    //         if (staggerDesired[staggerIndex] == onLevel && outState[staggerIndex] == offLevel)
    //         {
    //             // writeOutput(i, true);
    //             writeOutput(staggerIndex, onLevel);
    //             staggerIndex++;
    //             nextStaggerMs = now + SEQ_ON_DELAY_MS;
    //             return;
    //         }
    //         staggerIndex++;
    //     }

    //     staggerActive = false;
    // }

    void processStaggeredOutputs()
    {
        if (!staggerActive)
            return;

        const uint32_t now = millis();
        if ((int32_t)(now - nextStaggerMs) < 0)
            return;

        while (staggerIndex < OUT_COUNT)
        {
            // Nếu trạng thái hiện tại khác trạng thái mong muốn
            // thì đổi từng output một theo thời gian
            if (outState[staggerIndex] != staggerDesired[staggerIndex])
            {
                writeOutput(staggerIndex, staggerDesired[staggerIndex]);

                staggerIndex++;
                nextStaggerMs = now + SEQ_ON_DELAY_MS;
                return;
            }

            staggerIndex++;
        }

        staggerActive = false;
        staggerIsManual = false;
    }

    void applyManualButtonsSnapshot(bool staggerRelays)
    {
        bool desired[OUT_COUNT] = {};
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            desired[i] = outState[i];

        const uint32_t now = millis();
        uint16_t manualSnapshot = 0;
        const bool hasManualSnapshotRead = manualPcf.readButtonsSnapshot(manualSnapshot);

        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
        {
            const bool v = hasManualSnapshotRead ? readManualButtonFromSnapshot(manualSnapshot, i) : readManualButton(i);
            const bool pressed = isManualButtonPressed(v);
            const bool level = physicalOnToOutputLevel(pressed);

            btn[i].stableLevel = v;
            btn[i].lastReading = v;
            btn[i].lastChangeMs = now;
            desired[i] = level;
            manualOutState[i] = level;
            hasManualSnapshot = true;

            // LED nut sang/tat ngay theo nut, khong doi relay bat xong moi sang.
            syncManualLedFromButton(i, v);
            printManualButtonState(F("sync"), i, v);
        }

        if (staggerRelays)
            applyIOStaggeredArray(desired, true);
        else
            applyIOArray(desired);
    }

    void requestManualOutput(uint8_t ch, bool level)
    {
        if (ch >= OUT_COUNT)
            return;

        // MANUAL cung phai ON/OFF tuan tu giong AUTO.
        // Khong writeOutput truc tiep khi nha nut nua, vi nhu vay relay OFF se tat dong loat.
        // Moi thay doi nut chi cap nhat trang thai mong muon, hang doi stagger se xu ly
        // tung kenh mot theo SEQ_ON_DELAY_MS.
        if (!staggerActive || !staggerIsManual)
        {
            for (uint8_t i = 0; i < OUT_COUNT; i++)
                staggerDesired[i] = outState[i];
        }

        staggerDesired[ch] = level;
        staggerIndex = 0;         // quet lai tu CH1 de khong bo sot kenh da thay doi
        nextStaggerMs = millis(); // cho phep xu ly ngay 1 kenh dau tien
        staggerIsManual = true;
        staggerActive = true;
        processStaggeredOutputs();
    }

    // Save manual/output snapshot. Works with OUT_COUNT channels.
    void captureManualOutputsArray(const bool desired[OUT_COUNT])
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            manualOutState[i] = desired[i];
        hasManualSnapshot = true;
    }

    // Backward-compatible helper for old 4-output callers.
    void captureManualOutputs(bool o1, bool o2, bool o3, bool o4)
    {
        bool desired[OUT_COUNT] = {};
        desired[0] = o1;
        desired[1] = o2;
        desired[2] = o3;
        desired[3] = o4;
        captureManualOutputsArray(desired);
    }

    // ================= DEBOUNCE =================
    // Debounce input, tráº£ true náº¿u cÃ³ thay Ä‘á»•i á»•n Ä‘á»‹nh.
    bool debounceUpdate(uint8_t i, bool &stableLevel)
    {
        bool reading = readManualButton(i);
        return debounceUpdate(i, reading, stableLevel);
    }

    bool debounceUpdate(uint8_t i, bool reading, bool &stableLevel)
    {
        if (i >= MANUAL_CH_COUNT)
        {
            stableLevel = ManualPcf8575IO::releasedLevel();
            return false;
        }

        uint32_t now = millis();

        if (reading != btn[i].lastReading)
        {
            Serial.print(F("[MANBTN] raw-change ch="));
            Serial.print(i + 1);
            Serial.print(F(" "));
            Serial.print(btn[i].lastReading ? F("HIGH") : F("LOW"));
            Serial.print(F("->"));
            Serial.println(reading ? F("HIGH") : F("LOW"));
            btn[i].lastChangeMs = now;
            btn[i].lastReading = reading;
        }

        if (now - btn[i].lastChangeMs > DEBOUNCE_MS) // chống dội input
        {
            if (btn[i].stableLevel != reading)
            {
                btn[i].stableLevel = reading;
                stableLevel = reading;
                return true;
            }
        }
        stableLevel = btn[i].stableLevel;
        return false;
    }

    // Phát hiện cạnh nhấn (rising edge) sau chống dội.
    bool debouncePress(uint8_t i)
    {
        if (i >= MANUAL_CH_COUNT)
            return false;

        static bool prevState[MANUAL_CH_COUNT]{};
        bool evt = (!prevState[i] && btn[i].stableLevel);
        prevState[i] = btn[i].stableLevel;
        return evt;
    }

    // ================= SNAPSHOT / CHANGE =================
    // Lưu trạng thái trước đó để so sánh khi có thay đổi.
    void luuMocTrangThai()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            mocOut[i] = outState[i];
        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            mocIn[i] = btn[i].stableLevel;
    }

    // Kiểm tra sự thay đổi trạng thái của input/output.
    bool coThayDoi()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            if (outState[i] != mocOut[i])
                return true;

        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            if (btn[i].stableLevel != mocIn[i])
                return true;

        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            if (nutVuaNhan[i])
                return true;

        return false;
    }

    // ================= JSON (format in/out) =================
    // Xây dựng JSON trạng thái IO để gửi về PC.
    String buildDataJson()
    {
        StaticJsonDocument<768> doc;
        JsonObject data = doc.to<JsonObject>();

        const bool onLevel = OUT_ACTIVE_LOW ? false : true;
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            data[String("out") + String(i + 1)] = (outState[i] == onLevel) ? 1 : 0;

        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            data[String("in") + String(i + 1)] = btn[i].stableLevel ? 0 : 1;

        String out;
        serializeJson(data, out);
        return out;
    }

    // Gá»­i tráº¡ng thÃ¡i náº¿u cÃ³ thay Ä‘á»•i (hoáº·c Ã©p gá»­i khi force=true).
    bool sendTrangThaiIfChanged(int opcode, uint32_t time, bool force = false)
    {
        String data_json = buildDataJson();
        (void)opcode;
        (void)time;
        (void)force;
        (void)data_json;
        return false;
    }

    // ================= TIME =================
    // Lấy thời gian gửi: ưu tiên Unix time từ PC, fallback về millis().
    uint32_t getSendTime()
    {
        // Ä‘á»ƒ auto_push luÃ´n hoáº¡t Ä‘á»™ng: fallback millis/1000
        return millis() / 1000;
    }

    // ================= AUTO PUSH=================
    // Auto push trạng thái khi có thay đổi, có chống spam.
    void tuDongDayNeuThayDoi()
    {
        // Náº¿u cÃ³ thay Ä‘á»•i hoáº·c Ä‘ang chá» gá»­i (deferred) thÃ¬ má»›i xÃ©t
        if (!coThayDoi() && !deferredPush)
            return;

        uint32_t now = millis();

        // Trong khoáº£ng chá»‘ng spam -> chá»‰ Ä‘Ã¡nh dáº¥u deferred, KHÃ”NG cáº­p nháº­t má»‘c
        if (now - lastPushMs < CHONG_SPAM_MS)
        {
            if (!deferredPush)
                Serial.println(F("[CTRL] Change detected but deferred by anti-spam"));
            deferredPush = true;
            return;
        }

        // Äá»§ thá»i gian: gá»­i bÃ¹ 1 láº§n
        deferredPush = false;
        lastPushMs = now;

        // Sau khi Ä‘Ã£ gá»­i má»›i cáº­p nháº­t má»‘c Ä‘á»ƒ láº§n sau so sÃ¡nh
        luuMocTrangThai();
        clearNutNhan();
    }

    // Xóa cờ input vừa nhận
    void clearNutNhan()
    {
        for (uint8_t i = 0; i < MANUAL_CH_COUNT; i++)
            nutVuaNhan[i] = false;
    }
};

// #ifdef CENTRAL_CONTROLLER_IMPLEMENTATION
// CentralController *CentralController::instance = nullptr;
// #endif
