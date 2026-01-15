#line 1 "D:\\phunsuong\\master\\master\\central\\central_controller.h"
// #pragma once
// #include <Arduino.h>
// #include <ArduinoJson.h>

// #include "config.h"
// #include "auto_man.h"
// #include "eeprom_state.h"
// #include "get_info.h"
// #define GET_INFO 0x03
// #define IO_COMMAND 0x02

// class CentralController
// {
// public:
//     CentralController() {}
//     // ================= BEGIN =================
//     void begin(Stream &pcStream = Serial)
//     {
//         _pc = &pcStream;

//         // AUTO / MAN
//         setupAutoManMode();

//         // OUT
//         for (uint8_t i = 0; i < OUT_COUNT; i++)
//         {
//             pinMode(OUT_PINS[i], OUTPUT);
//             _outState[i] = false;
//             _writePin(i, false);
//         }

//         // IN + debounce init
//         uint32_t now = millis();
//         for (uint8_t i = 0; i < IN_COUNT; i++)
//         {
//             pinMode(IN_PINS[i], INPUT_PULLUP);
//             bool v = (digitalRead(IN_PINS[i]) == LOW);
//             _btn[i].stable = v;
//             _btn[i].last = v;
//             _btn[i].lastChangeMs = now;
//             _nutVuaNhan[i] = false;
//         }

//         // EEPROM
//         eepromStateBegin();
//         _instance = this;
//         eepromStateLoad(_eepromThunk);

//         // snapshot ban đầu
//         _luuMocTrangThai();
//         _lastDataJson = _buildDataJson();
//     }

//     // ================= HANDLE PC COMMAND =================
//     void handleCommand(const MistCommand &cmd)
//     {
//         if (cmd.unix != 0)
//             _lastPcUnix = cmd.unix;

//         // GET_INFO -> trả trạng thái ngay
//         if (cmd.opcode == GET_INFO)
//         {
//             _sendTrangThai(cmd.id_des, GET_INFO + 100, _getSendTime());
//             _luuMocTrangThai();
//             _clearNutNhan();
//             return;
//         }

//         // IO_COMMAND -> điều khiển output
//         if (cmd.opcode == IO_COMMAND)
//         {
//             Serial.println("co data");
//             _applyIO(cmd.out1, cmd.out2, cmd.out3, cmd.out4);
//             sendResponse(cmd.id_des, IO_COMMAND + 100, cmd.unix, 0);
//             _sendTrangThai(cmd.id_des, GET_INFO + 100, _getSendTime());
//             _luuMocTrangThai();
//             _clearNutNhan();
//             return;
//         }
//     }
//     void getInforCommand(GetInfoAggregator &getInfo)
//     {
//         _getInfo = &getInfo;
//     }

//     // ================= LOOP =================
//     void update()
//     {
//         _capNhatDebounce();

//         // ===== MAN MODE =====
//         if (!isAutoMode())
//         {
//             bool coNhan = false;
//             for (uint8_t i = 0; i < IN_COUNT; i++)
//             {
//                 if (_debouncePress(i))
//                 {
//                     _nutVuaNhan[i] = true;
//                     coNhan = true;
//                 }
//             }

//             if (coNhan)
//             {
//                 _sendTrangThai(1, GET_INFO + 100, _getSendTime());
//                 _luuMocTrangThai();
//                 _clearNutNhan();
//             }
//         }
//         else
//         {
//             // AUTO mode: chặn thao tác tay
//             _clearNutNhan();
//         }

//         // ===== AUTO_PUSH (luôn chạy) =====
//         _autoPushNeuThayDoi();

//         // EEPROM
//         eepromStateUpdate(_outState);
//     }

// private:
//     // ================= HW =================
//     static constexpr uint8_t OUT_PINS[OUT_COUNT] = {21, 18, 255, 255};
//     static constexpr uint8_t IN_PINS[IN_COUNT] = {12, 13, 255, 255};

//     static constexpr uint32_t DEBOUNCE_MS = 35;
//     static constexpr uint32_t CHONG_SPAM_MS = 120;

//     GetInfoAggregator *_getInfo = nullptr;
//     struct ButtonState
//     {
//         bool stable;
//         bool last;
//         uint32_t lastChangeMs;
//     };

// private:
//     Stream *_pc = nullptr;

//     bool _outState[OUT_COUNT]{};
//     ButtonState _btn[IN_COUNT]{};
//     bool _nutVuaNhan[IN_COUNT]{};

//     // mốc trạng thái
//     bool _mocOut[OUT_COUNT]{};
//     bool _mocIn[IN_COUNT]{};

//     uint32_t _lastPushMs = 0;
//     uint32_t _lastPcUnix = 0;
//     String _lastDataJson;

//     static CentralController *_instance;

//     // ================= EEPROM =================
//     static void _eepromThunk(uint8_t ch, bool on)
//     {
//         if (_instance)
//             _instance->_writeOutput(ch, on);
//     }

//     // ================= OUTPUT =================
//     void _writePin(uint8_t ch, bool on)
//     {
// #if OUT_ACTIVE_LOW
//         digitalWrite(OUT_PINS[ch], on ? LOW : HIGH);
// #else
//         digitalWrite(OUT_PINS[ch], on ? HIGH : LOW);
// #endif
//     }

//     void _writeOutput(uint8_t ch, bool on)
//     {
//         bool prev = _outState[ch];
//         _outState[ch] = on;
//         _writePin(ch, on);
//         if (prev != on)
//             eepromStateMarkDirty();
//     }

//     void _applyIO(bool o1, bool o2, bool o3, bool o4)
//     {
//         _writeOutput(0, o1);
//         _writeOutput(1, o2);
//         _writeOutput(2, o3);
//         _writeOutput(3, o4);
//     }

//     // ================= DEBOUNCE =================
//     void _capNhatDebounce()
//     {
//         uint32_t now = millis();
//         for (uint8_t i = 0; i < IN_COUNT; i++)
//         {
//             bool r = (digitalRead(IN_PINS[i]) == LOW);
//             if (r != _btn[i].last)
//             {
//                 _btn[i].last = r;
//                 _btn[i].lastChangeMs = now;
//             }
//             if (now - _btn[i].lastChangeMs > DEBOUNCE_MS)
//                 _btn[i].stable = r;
//         }
//     }

//     bool _debouncePress(uint8_t i)
//     {
//         static bool prev[IN_COUNT]{};
//         bool evt = (!prev[i] && _btn[i].stable);
//         prev[i] = _btn[i].stable;
//         return evt;
//     }

//     // ================= AUTO PUSH =================
//     void _luuMocTrangThai()
//     {
//         for (uint8_t i = 0; i < OUT_COUNT; i++)
//             _mocOut[i] = _outState[i];
//         for (uint8_t i = 0; i < IN_COUNT; i++)
//             _mocIn[i] = _btn[i].stable;
//     }

//     bool _coThayDoi()
//     {
//         for (uint8_t i = 0; i < OUT_COUNT; i++)
//             if (_outState[i] != _mocOut[i])
//                 return true;

//         for (uint8_t i = 0; i < IN_COUNT; i++)
//             if (_btn[i].stable != _mocIn[i])
//                 return true;

//         for (uint8_t i = 0; i < IN_COUNT; i++)
//             if (_nutVuaNhan[i])
//                 return true;

//         return false;
//     }

//     void _autoPushNeuThayDoi()
//     {
//         if (!_coThayDoi())
//             return;

//         uint32_t now = millis();
//         if (now - _lastPushMs < CHONG_SPAM_MS)
//         {
//             _luuMocTrangThai();
//             return;
//         }

//         _lastPushMs = now;
//         _sendTrangThai(1, GET_INFO + 100, _getSendTime(), false);
//         _luuMocTrangThai();
//     }

//     // ================= JSON =================
//     String _buildDataJson()
//     {
//         StaticJsonDocument<256> doc;
//         JsonObject data = doc.to<JsonObject>();

//         for (uint8_t i = 0; i < OUT_COUNT; i++)
//             data[String("out") + String(i + 1)] = _outState[i] ? 1 : 0;

//         for (uint8_t i = 0; i < IN_COUNT; i++)
//             data[String("in") + String(i + 1)] = _btn[i].stable ? 1 : 0;

//         String out;
//         serializeJson(data, out);
//         return out;
//     }

//     void _sendTrangThai(int id_des, int opcode, uint32_t time)
//     {
//         String data = _buildDataJson();
//         StaticJsonDocument<512> doc;
//         doc["id_des"] = id_des;
//         doc["opcode"] = opcode;
//         deserializeJson(doc["data"], data);
//         doc["time"] = time;

//         String sign = String(id_des) + String(opcode) + data + String(time) + SECRET_KEY;
//         doc["auth"] = calculateMD5(sign);

//         if (_getInfo)
//         {
//             // Feed nguyên JsonObjectConst sang aggregator (không parse lại)
//             _getInfo->ingestFromNodeDoc(doc.as<JsonObjectConst>(), IPAddress(0, 0, 0, 0), 0);
//         }
//     }

//     uint32_t _getSendTime()
//     {
//         if (_lastPcUnix != 0)
//             return _lastPcUnix;
//         return millis() / 1000;
//     }

//     void _clearNutNhan()
//     {
//         for (uint8_t i = 0; i < IN_COUNT; i++)
//             _nutVuaNhan[i] = false;
//     }

//     void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status)
//     {
//         StaticJsonDocument<512> resp_doc;
//         resp_doc["id_des"] = id_des;
//         resp_doc["opcode"] = resp_opcode;
//         JsonObject data = resp_doc.createNestedObject("data");
//         data["status"] = status;
//         resp_doc["time"] = unix_time;

//         // Tính auth cho phản hồirr
//         String id_des_str = String(id_des);
//         String opcode_str = String(resp_opcode);
//         String time_str = String(unix_time);

//         String data_json;
//         serializeJson(resp_doc["data"], data_json);

//         String combined = id_des_str + opcode_str + data_json + time_str + SECRET_KEY;
//         String auth_hash = calculateMD5(combined);

//         resp_doc["auth"] = auth_hash;

//         String output;
//         serializeJson(resp_doc, output);
//         Serial.println(output);
//     }
// };

// CentralController *CentralController::_instance = nullptr;

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "auto_man.h"
#include "eeprom_state.h"
#include "get_info.h"

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

        // OUT
        for (uint8_t i = 0; i < OUT_COUNT; i++)
        {
            if (OUT_PINS[i] == 255)
                continue;
            pinMode(OUT_PINS[i], OUTPUT);
            _outState[i] = false;
            _writePin(i, false);
        }

        // IN + debounce init
        uint32_t now = millis();
        for (uint8_t i = 0; i < IN_COUNT; i++)
        {
            if (IN_PINS[i] == 255)
                continue;
            pinMode(IN_PINS[i], INPUT_PULLUP);

            bool v = (digitalRead(IN_PINS[i]) == LOW);
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

        // ===== MAN MODE: nhấn nút tay thì vẫn coi là thay đổi để auto_push đẩy =====
        if (!isAutoMode())
        {
            for (uint8_t i = 0; i < IN_COUNT; i++)
            {
                if (IN_PINS[i] == 255)
                    continue;
                if (_debouncePress(i))
                    _nutVuaNhan[i] = true;
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
    static constexpr uint8_t OUT_PINS[OUT_COUNT] = {21, 18, 12, 255};
    static constexpr uint8_t IN_PINS[IN_COUNT] = {13, 255, 255, 255};

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
        if (OUT_PINS[ch] == 255)
            return;

#if OUT_ACTIVE_LOW
        digitalWrite(OUT_PINS[ch], on ? LOW : HIGH);
#else
        digitalWrite(OUT_PINS[ch], on ? HIGH : LOW);
#endif
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
            if (IN_PINS[i] == 255)
                continue;

            bool r = (digitalRead(IN_PINS[i]) == LOW);
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
