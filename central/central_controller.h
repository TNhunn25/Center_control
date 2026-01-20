
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "auto_man.h"
#include "eeprom_state.h"
#include "get_info.h"
#include "Pcf8575_io.h"
#include "led_status.h"

#define GET_INFO 0x03
#define IO_COMMAND 0x02
#define SET_THRESHOLDS 0x06

extern PCF8575IO pcf;

class CentralController
{
public:
    CentralController() {}

    // ================= BEGIN =================
    void begin(Stream &pcStream = Serial)
    {
        pc = &pcStream;

        // AUTO / MAN
        setupAutoManMode();

        pcf.begin();
        ledStatusBegin();

        // OUT
        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            outState[i] = false;
            writePin(i, false);
        }

        // IN + debounce init
        uint32_t now = millis();
        for (uint8_t i = 0; i < IN_COUNT; i++)
        {
            bool v = pcf.readInput(i);
            btn[i].stableLevel = v;
            btn[i].lastReading = v;
            btn[i].lastChangeMs = now;
            nutVuaNhan[i] = false;
        }

        // EEPROM
        eepromStateBegin();
        instance = this;
        eepromStateLoad(eepromThunk);

        // snapshot ban đầu
        luuMocTrangThai();
        lastDataJson = buildDataJson();
        eepromThresholdsLoad(thresholds);
        // chống mất gói khi spam
        lastPushMs = millis();
        deferredPush = false;
    }

    // ================= Inject Aggregator =================
    void getInforCommand(GetInfoAggregator &getInfoRef)
    {
        getInfo = &getInfoRef;
    }

    // ================= HANDLE PC COMMAND =================
    void handleCommand(const MistCommand &cmd)
    {
        if (cmd.unix != 0)
            lastPcUnix = cmd.unix;

        switch (cmd.opcode)
        {
        case GET_INFO:
        {
            // trả trạng thái ngay (if changed)
            sendTrangThaiIfChanged(cmd.id_des, GET_INFO + 100, getSendTime());
            // không snapshot ở đây để tránh nuốt event đang chờ (deferred)
            clearNutNhan();
            return;
        }

        case IO_COMMAND:
        {
            if (!isAutoMode())
            {
                // MAN mode: reject remote IO
                sendResponse(cmd.id_des, IO_COMMAND + 100, cmd.unix, 1);
                return;
            }
            // 1) ACK 102
            sendResponse(cmd.id_des, IO_COMMAND + 100, cmd.unix, 0);
            // 2) áp output
            applyIO(cmd.out1, cmd.out2, cmd.out3, cmd.out4);
            return;
        }
        case SET_THRESHOLDS:
        {
            thresholds = cmd.thresholds;
            eepromThresholdsSave(thresholds);
            sendResponse(cmd.id_des, SET_THRESHOLDS + 100, cmd.unix, 0);
            return;
        }

        default:
            return;
        }
    }

    // ================= LOOP =================
    void update()
    {
        updateAutoOutputsFromVoc();
        // ===== MAN MODE: nhấn nút tay thì vẫn coi là thay đổi để auto_push đẩy =====
        if (!isAutoMode())
        {
            for (int i = 0; i < IN_COUNT; i++)
            {
                bool stableLevel = LOW; // Man = low
                if (debounceUpdate(i, stableLevel))
                {
                    bool pressed = (stableLevel == HIGH); //Man = High
                    if (pressed)
                        nutVuaNhan[i] = true;
                    writeOutput(i, pressed);
                    // Serial.printf("MAN: Button %d %s -> out%d %s\n",
                    //               i + 1,
                    //               pressed ? "pressed" : "released",
                    //               i + 1,
                    //               pressed ? "ON" : "OFF"); // released OFF Pressed ON
                }
            }
        }

        // ===== AUTO_PUSH: luôn chạy khi có thay đổi, KHÔNG MẤT GÓI =====
        tuDongDayNeuThayDoi();

        // 3) LED status

        // ledStatusBegin();
        setStatusLed(true, false);
        ledStatusUpdate(pcf);

        // EEPROM
        eepromStateUpdate(outState);
    }

private:
    // ================= HW =================
    static constexpr uint32_t DEBOUNCE_MS = 35;
    static constexpr uint32_t CHONG_SPAM_MS = 120;
    static constexpr float TEMP_ON_DEFAULT = 50.0f;
    static constexpr float TEMP_OFF_DEFAULT = 45.0f;
    static constexpr float HUMI_ON_DEFAULT = 54.0f;
    static constexpr float HUMI_OFF_DEFAULT = 53.0f;
    static constexpr float NH3_ON_DEFAULT = 50.0f;
    static constexpr float NH3_OFF_DEFAULT = 45.0f;
    static constexpr float CO_ON_DEFAULT = 50.0f;
    static constexpr float CO_OFF_DEFAULT = 45.0f;
    static constexpr float NO2_ON_DEFAULT = 5.0f;
    static constexpr float NO2_OFF_DEFAULT = 4.0f;
    static constexpr bool OFF_WHEN_ANY_CLEAR = false;
    //   - OFF_WHEN_ANY_CLEAR = false -> chi can 1 gia tri giam (qua nguong OFF) la tat output.
    //   - OFF_WHEN_ANY_CLEAR = true -> tat khi tat ca gia tri giam. (mac dinh)
    struct ButtonState
    {
        bool stableLevel;
        bool lastReading;
        uint32_t lastChangeMs;
    };

private:
    Stream *pc = nullptr;
    GetInfoAggregator *getInfo = nullptr;

    bool outState[OUT_COUNT]{};
    ButtonState btn[IN_COUNT]{};
    bool nutVuaNhan[IN_COUNT]{};

    // mốc trạng thái
    bool mocOut[OUT_COUNT]{};
    bool mocIn[IN_COUNT]{};

    // chống spam + chống mất event
    uint32_t lastPushMs = 0;
    bool deferredPush = false;

    uint32_t lastPcUnix = 0;

    // chống gửi trùng theo json
    String lastDataJson;
    bool autoOutputsOn = false;
    bool vocHigh[2][5]{};
    Thresholds thresholds{
        TEMP_ON_DEFAULT,
        TEMP_OFF_DEFAULT,
        HUMI_ON_DEFAULT,
        HUMI_OFF_DEFAULT,
        NH3_ON_DEFAULT,
        NH3_OFF_DEFAULT,
        CO_ON_DEFAULT,
        CO_OFF_DEFAULT,
        NO2_ON_DEFAULT,
        NO2_OFF_DEFAULT};

    static CentralController *instance;

    // ================= EEPROM =================
    static void eepromThunk(uint8_t ch, bool on)
    {
        if (instance)
            instance->writeOutput(ch, on);
    }

    // ================= OUTPUT =================
    void writePin(uint8_t ch, bool on)
    {
        pcf.writeOutput(ch, on);
    }

    void writeOutput(uint8_t ch, bool on)
    {
        bool prevState = outState[ch];
        outState[ch] = on;
        writePin(ch, on);
        if (prevState != on)
            eepromStateMarkDirty();
    }

    void applyIO(bool o1, bool o2, bool o3, bool o4)
    {
        writeOutput(0, o1);
        writeOutput(1, o2);
        writeOutput(2, o3);
        writeOutput(3, o4);
    }

    // ================= DEBOUNCE =================
    bool debounceUpdate(uint8_t i, bool &stableLevel)
    {
        bool reading = pcf.readInput(i);
        uint32_t now = millis();

        if (reading != btn[i].lastReading)
        {
            btn[i].lastChangeMs = now;
            btn[i].lastReading = reading;
        }

        if (now - btn[i].lastChangeMs > DEBOUNCE_MS)
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

    bool debouncePress(uint8_t i)
    {
        static bool prevState[IN_COUNT]{};
        bool evt = (!prevState[i] && btn[i].stableLevel);
        prevState[i] = btn[i].stableLevel;
        return evt;
    }

    static bool applyHysteresis(float value, float on, float off, bool state)
    {
        if (value >= on)
            return true;
        if (value <= off)
            return false;
        return state;
    }

    void updateAutoOutputsFromVoc()
    {
        if (!getInfo)
            return;

        bool anyMetric = false;
        bool anyHigh = false;
        bool allHigh = true;
        for (uint8_t i = 0; i < 2; i++)
        {
            SensorVocSnapshot snap{};
            if (!getInfo->getVocSnapshot(i, snap))
                continue;

            vocHigh[i][0] = applyHysteresis(snap.temp, thresholds.temp_on, thresholds.temp_off, vocHigh[i][0]);
            vocHigh[i][1] = applyHysteresis(snap.humi, thresholds.humi_on, thresholds.humi_off, vocHigh[i][1]);
            vocHigh[i][2] = applyHysteresis(snap.nh3, thresholds.nh3_on, thresholds.nh3_off, vocHigh[i][2]);
            vocHigh[i][3] = applyHysteresis(snap.co, thresholds.co_on, thresholds.co_off, vocHigh[i][3]);
            vocHigh[i][4] = applyHysteresis(snap.no2, thresholds.no2_on, thresholds.no2_off, vocHigh[i][4]);

            for (uint8_t k = 0; k < 5; k++)
            {
                anyMetric = true;
                anyHigh = anyHigh || vocHigh[i][k];
                allHigh = allHigh && vocHigh[i][k];
            }
        }

        bool shouldOn = false;
        if (anyMetric)
            shouldOn = OFF_WHEN_ANY_CLEAR ? allHigh : anyHigh;
        if (shouldOn != autoOutputsOn)
        {
            autoOutputsOn = shouldOn;
            applyIO(shouldOn, shouldOn, shouldOn, shouldOn);
        }
    }

    // ================= SNAPSHOT / CHANGE =================
    void luuMocTrangThai()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            mocOut[i] = outState[i];
        for (uint8_t i = 0; i < IN_COUNT; i++)
            mocIn[i] = btn[i].stableLevel;
    }

    bool coThayDoi()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            if (outState[i] != mocOut[i])
                return true;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            if (btn[i].stableLevel != mocIn[i])
                return true;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            if (nutVuaNhan[i])
                return true;

        return false;
    }

    // ================= JSON (format in/out) =================
    String buildDataJson()
    {
        StaticJsonDocument<256> doc;
        JsonObject data = doc.to<JsonObject>();

        for (uint8_t i = 0; i < OUT_COUNT; i++)
            data[String("out") + String(i + 1)] = outState[i] ? 0 : 1;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            data[String("in") + String(i + 1)] = btn[i].stableLevel ? 0 : 1;

        String out;
        serializeJson(data, out);
        return out;
    }

    void ingestTrangThaiToAggregator(int id_des, int opcode, uint32_t time, const String &data_json)
    {
        if (!getInfo)
            return;

        StaticJsonDocument<512> doc;
        doc["id_des"] = id_des;
        doc["opcode"] = opcode;

        deserializeJson(doc["data"], data_json);
        doc["time"] = time;

        String sign = String(id_des) + String(opcode) + data_json + String(time) + SECRET_KEY;
        doc["auth"] = calculateMD5(sign);

        getInfo->ingestFromNodeDoc(doc.as<JsonObjectConst>(), IPAddress(0, 0, 0, 0), 0);
    }

    bool sendTrangThaiIfChanged(int id_des, int opcode, uint32_t time)
    {
        String data_json = buildDataJson();
        if (data_json == lastDataJson)
            return false;

        lastDataJson = data_json;
        ingestTrangThaiToAggregator(id_des, opcode, time, data_json);
        return true;
    }

    // ================= TIME =================
    uint32_t getSendTime()
    {
        // để auto_push luôn hoạt động: fallback millis/1000
        if (lastPcUnix != 0)
            return lastPcUnix;
        return millis() / 1000;
    }

    // ================= AUTO PUSH (FIX MẤT GÓI) =================
    void tuDongDayNeuThayDoi()
    {
        // Nếu có thay đổi hoặc đang chờ gửi (deferred) thì mới xét
        if (!coThayDoi() && !deferredPush)
            return;

        uint32_t now = millis();

        // Trong khoảng chống spam -> chỉ đánh dấu deferred, KHÔNG cập nhật mốc
        if (now - lastPushMs < CHONG_SPAM_MS)
        {
            deferredPush = true;
            return;
        }

        // Đủ thời gian: gửi bù 1 lần
        deferredPush = false;
        lastPushMs = now;

        // 103 do auto_push
        sendTrangThaiIfChanged(1, GET_INFO + 100, getSendTime());

        // Sau khi đã gửi mới cập nhật mốc để lần sau so sánh
        luuMocTrangThai();
        clearNutNhan();
    }

    void clearNutNhan()
    {
        for (uint8_t i = 0; i < IN_COUNT; i++)
            nutVuaNhan[i] = false;
    }

    // ================= ACK RESPONSE =================
    void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status)
    {
        StaticJsonDocument<512> resp_doc;
        resp_doc["id_des"] = id_des;
        resp_doc["opcode"] = resp_opcode;

        JsonObject data = resp_doc.createNestedObject("data");
        data["status"] = status;
        resp_doc["time"] = unix_time;

        String data_json;
        serializeJson(resp_doc["data"], data_json);

        String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
        resp_doc["auth"] = calculateMD5(combined);

        String output;
        serializeJson(resp_doc, output);
        Serial.println(output);
    }
};

#ifdef CENTRAL_CONTROLLER_IMPLEMENTATION
CentralController *CentralController::instance = nullptr;
#endif
