#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\phunsuong\\master\\master\\central\\central_controller.h"

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "auto_man.h"
#include "eeprom_state.h"
#include "get_info.h"
#include "PCF8575.h"

#define GET_INFO 0x03
#define IO_COMMAND 0x02

class CentralController
{
public:
    CentralController() {}

    // ================= BEGIN =================
    void begin(Stream &pcStream = Serial)
    {
        _pc = &pcStream;

        // AUTO / MAN
        setupAutoManMode();

        PcfIo::begin();

        // OUT
        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            _outState[i] = false;
            _writePin(i, false);
        }

        // IN + debounce init
        uint32_t now = millis();
        for (uint8_t i = 0; i < IN_COUNT; i++)
        {
            bool v = PcfIo::readIn(i);
            _btn[i].stable = v;
            _btn[i].last = v;
            _btn[i].lastChangeMs = now;
            _nutVuaNhan[i] = false;
        }

        // EEPROM
        eepromStateBegin();
        _instance = this;
        eepromStateLoad(_eepromThunk);

        // snapshot ban đầu
        _luuMocTrangThai();
        _lastDataJson = _buildDataJson();

        // chống mất gói khi spam
        _lastPushMs = millis();
        _deferredPush = false;
    }

    // ================= Inject Aggregator =================
    void getInforCommand(GetInfoAggregator &getInfo)
    {
        _getInfo = &getInfo;
    }

    // ================= HANDLE PC COMMAND =================
    void handleCommand(const MistCommand &cmd)
    {
        if (cmd.unix != 0)
            _lastPcUnix = cmd.unix;

        switch (cmd.opcode)
        {
        case GET_INFO:
        {
            // trả trạng thái ngay (if changed)
            _sendTrangThaiIfChanged(cmd.id_des, GET_INFO + 100, _getSendTime());
            // không snapshot ở đây để tránh nuốt event đang chờ (deferred)
            _clearNutNhan();
            return;
        }

        case IO_COMMAND:
        {
            // 1) ACK 102
            sendResponse(cmd.id_des, IO_COMMAND + 100, cmd.unix, 0);
            // 2) áp output
            _applyIO(cmd.out1, cmd.out2, cmd.out3, cmd.out4);
            return;
        }

        default:
            return;
        }
    }

    // ================= LOOP =================
    void update()
    {
        _capNhatDebounce();

        if (!isAutoMode())
            _dongBoOutputTheoInput();

        // ===== MAN MODE: nhấn nút tay thì vẫn coi là thay đổi để auto_push đẩy =====
        if (!isAutoMode())
        {
            for (uint8_t i = 0; i < IN_COUNT; i++)
            {
                if (_debouncePress(i))
                {
                    _nutVuaNhan[i] = true;
                    _writeOutput(i, !_outState[i]); // toggle output for matching button
                }
            }
        }
        else
        {
            _clearNutNhan();
        }

        // ===== AUTO_PUSH: luôn chạy khi có thay đổi, KHÔNG MẤT GÓI =====
        _tuDongDayNeuThayDoi();

        // EEPROM
        eepromStateUpdate(_outState);
    }

private:
    // ================= HW =================
    static constexpr uint32_t DEBOUNCE_MS = 35;
    static constexpr uint32_t CHONG_SPAM_MS = 120;

    struct ButtonState
    {
        bool stable;
        bool last;
        uint32_t lastChangeMs;
    };

private:
    Stream *_pc = nullptr;
    GetInfoAggregator *_getInfo = nullptr;

    bool _outState[OUT_COUNT]{};
    ButtonState _btn[IN_COUNT]{};
    bool _nutVuaNhan[IN_COUNT]{};

    // mốc trạng thái
    bool _mocOut[OUT_COUNT]{};
    bool _mocIn[IN_COUNT]{};

    // chống spam + chống mất event
    uint32_t _lastPushMs = 0;
    bool _deferredPush = false;

    uint32_t _lastPcUnix = 0;

    // chống gửi trùng theo json
    String _lastDataJson;

    static CentralController *_instance;

    // ================= EEPROM =================
    static void _eepromThunk(uint8_t ch, bool on)
    {
        if (_instance)
            _instance->_writeOutput(ch, on);
    }

    // ================= OUTPUT =================
    void _writePin(uint8_t ch, bool on)
    {
        PcfIo::writeOut(ch, on);
    }

    void _writeOutput(uint8_t ch, bool on)
    {
        bool prev = _outState[ch];
        _outState[ch] = on;
        _writePin(ch, on);
        if (prev != on)
            eepromStateMarkDirty();
    }

    void _applyIO(bool o1, bool o2, bool o3, bool o4)
    {
        _writeOutput(0, o1);
        _writeOutput(1, o2);
        _writeOutput(2, o3);
        _writeOutput(3, o4);
    }

    // ================= DEBOUNCE =================
    void _capNhatDebounce()
    {
        uint32_t now = millis();
        for (uint8_t i = 0; i < IN_COUNT; i++)
        {
            bool r = PcfIo::readIn(i);
            if (r != _btn[i].last)
            {
                _btn[i].last = r;
                _btn[i].lastChangeMs = now;
            }
            if (now - _btn[i].lastChangeMs > DEBOUNCE_MS)
                _btn[i].stable = r;
        }
    }

    bool _debouncePress(uint8_t i)
    {
        static bool prev[IN_COUNT]{};
        bool evt = (!prev[i] && _btn[i].stable);
        prev[i] = _btn[i].stable;
        return evt;
    }

    void _dongBoOutputTheoInput()
    {
        uint8_t count = OUT_COUNT < IN_COUNT ? OUT_COUNT : IN_COUNT;
        for (uint8_t i = 0; i < count; i++)
            _writeOutput(i, _btn[i].stable);
    }

    // ================= SNAPSHOT / CHANGE =================
    void _luuMocTrangThai()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            _mocOut[i] = _outState[i];
        for (uint8_t i = 0; i < IN_COUNT; i++)
            _mocIn[i] = _btn[i].stable;
    }

    bool _coThayDoi()
    {
        for (uint8_t i = 0; i < OUT_COUNT; i++)
            if (_outState[i] != _mocOut[i])
                return true;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            if (_btn[i].stable != _mocIn[i])
                return true;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            if (_nutVuaNhan[i])
                return true;

        return false;
    }

    // ================= JSON (format in/out) =================
    String _buildDataJson()
    {
        StaticJsonDocument<256> doc;
        JsonObject data = doc.to<JsonObject>();

        for (uint8_t i = 0; i < OUT_COUNT; i++)
            data[String("out") + String(i + 1)] = _outState[i] ? 1 : 0;

        for (uint8_t i = 0; i < IN_COUNT; i++)
            data[String("in") + String(i + 1)] = _btn[i].stable ? 1 : 0;

        String out;
        serializeJson(data, out);
        return out;
    }

    void _ingestTrangThaiToAggregator(int id_des, int opcode, uint32_t time, const String &data_json)
    {
        if (!_getInfo)
            return;

        StaticJsonDocument<512> doc;
        doc["id_des"] = id_des;
        doc["opcode"] = opcode;

        deserializeJson(doc["data"], data_json);
        doc["time"] = time;

        String sign = String(id_des) + String(opcode) + data_json + String(time) + SECRET_KEY;
        doc["auth"] = calculateMD5(sign);

        _getInfo->ingestFromNodeDoc(doc.as<JsonObjectConst>(), IPAddress(0, 0, 0, 0), 0);
    }

    bool _sendTrangThaiIfChanged(int id_des, int opcode, uint32_t time)
    {
        String data_json = _buildDataJson();
        if (data_json == _lastDataJson)
            return false;

        _lastDataJson = data_json;
        _ingestTrangThaiToAggregator(id_des, opcode, time, data_json);
        return true;
    }

    // ================= TIME =================
    uint32_t _getSendTime()
    {
        // để auto_push luôn hoạt động: fallback millis/1000
        if (_lastPcUnix != 0)
            return _lastPcUnix;
        return millis() / 1000;
    }

    // ================= AUTO PUSH (FIX MẤT GÓI) =================
    void _tuDongDayNeuThayDoi()
    {
        // Nếu có thay đổi hoặc đang chờ gửi (deferred) thì mới xét
        if (!_coThayDoi() && !_deferredPush)
            return;

        uint32_t now = millis();

        // Trong khoảng chống spam -> chỉ đánh dấu deferred, KHÔNG cập nhật mốc
        if (now - _lastPushMs < CHONG_SPAM_MS)
        {
            _deferredPush = true;
            return;
        }

        // Đủ thời gian: gửi bù 1 lần
        _deferredPush = false;
        _lastPushMs = now;

        // 103 do auto_push
        _sendTrangThaiIfChanged(1, GET_INFO + 100, _getSendTime());

        // Sau khi đã gửi mới cập nhật mốc để lần sau so sánh
        _luuMocTrangThai();
        _clearNutNhan();
    }

    void _clearNutNhan()
    {
        for (uint8_t i = 0; i < IN_COUNT; i++)
            _nutVuaNhan[i] = false;
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
CentralController *CentralController::_instance = nullptr;
#endif
