#ifndef PC_HANDLER_H
#define PC_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "auto_man.h" 

extern const String SECRET_KEY;

class PCHandler
{
public:
    using CommandCallback = void (*)(const MistCommand &cmd);

    PCHandler();
    void begin();
    void update();
    void onCommandReceived(CommandCallback cb);
    // void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status);
    void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status, const MistCommand *cmd = nullptr);

private:
    String rxLine;
    StaticJsonDocument<512> doc;
    CommandCallback commandCallback = nullptr;

    void processLine();
};

// Xác thực auth nhận từ PC
static inline bool auth_PC(const JsonDocument &doc, const String &receivedHash)
{
    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();

    String id_des_str = String(id_des);
    String opcode_str = String(opcode);
    String time_str = String(unix_time);

    // Stringify phần data ở dạng compact
    String data_json;
    serializeJson(doc["data"], data_json);

    String combined = id_des_str + opcode_str + data_json + time_str + SECRET_KEY;

    String computedHash = calculateMD5(combined);
    return computedHash.equalsIgnoreCase(receivedHash);
}

inline PCHandler::PCHandler()
{
    rxLine.reserve(1024);
}

inline void PCHandler::begin()
{
    Serial.begin(115200);
}

inline void PCHandler::onCommandReceived(CommandCallback cb)
{
    commandCallback = cb;
}

// inline void PCHandler::update()
// {
//     while (Serial.available())
//     {
//         char c = (char)Serial.read();
//         if (c == '\n')
//         {
//             rxLine.trim();
//             if (rxLine.length() == 0)
//             {
//                 rxLine = "";
//                 continue;
//             }
//             processLine();
//             rxLine = "";
//         }
//         else
//         {
//             if (rxLine.length() < 512)
//             {
//                 rxLine += c;
//             }
//         }
//     }
// }

inline void PCHandler::update()
{
    uint16_t bytesProcessed = 0;
    const uint16_t MAX_BYTES_PER_LOOP = 256;

    while (Serial.available() && bytesProcessed < MAX_BYTES_PER_LOOP)
    {
        char c = (char)Serial.read();
        bytesProcessed++;

        if (c == '\r') continue; // Windows CRLF

        if (c == '\n')
        {
            rxLine.trim();
            if (rxLine.length() > 0) {
                processLine();
            }
            rxLine = "";
        }
        else
        {
            if (rxLine.length() < 512) rxLine += c;
            else rxLine = ""; // quá dài -> drop tránh bơm heap
        }

        if ((bytesProcessed & 0x3F) == 0) yield(); // mỗi 64 byte
    }

    if (Serial.available()) yield();
}

inline void PCHandler::processLine()
{
    DeserializationError error = deserializeJson(doc, rxLine);
    if (error)
    {
        return;
    }

    // Serial.print(F("RX PC PACKET: "));
    Serial.println(rxLine);

    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();
    String receivedHash = doc["auth"].as<String>();

    if (id_des < 0 || (opcode != 1 && opcode != 2 && opcode != 3))
    {
        sendResponse(id_des, opcode + 100, unix_time, 255);
        return;
    }

    if (!auth_PC(doc, receivedHash))
    {
        sendResponse(id_des, opcode + 100, unix_time, 1);
        return;
    }
    if(opcode == 1 || opcode == 2 & isAutoMode())
    {
        sendResponse(id_des, opcode + 100, unix_time, 255);
        return;
    }
    // === AUTH OK → Xử lý lệnh ===
    MistCommand cmd{};
    cmd.unix = unix_time;
    cmd.id_des = (int8_t)id_des;

    if (opcode == 1)
    {

        auto dataObj = doc["data"].as<JsonObject>();
        cmd.opcode = 1;
        cmd.node_id = dataObj["node_id"].as<int8_t>();
        cmd.time_phase1 = dataObj["time_phase1"].as<uint16_t>();
        cmd.time_phase2 = dataObj["time_phase2"].as<uint16_t>();
    }
    else if (opcode == 2)
    {
        // auto dataObj = doc["data"].as<JsonObject>();
        JsonObjectConst dataObj = doc["data"].as<JsonObjectConst>();
        if (dataObj.isNull() || !dataObj.containsKey("out1") || !dataObj.containsKey("out2") ||
            !dataObj.containsKey("out3") || !dataObj.containsKey("out4"))
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }
        cmd.opcode = 2;
        cmd.out1 = dataObj["out1"].as<int>() != 0;
        cmd.out2 = dataObj["out2"].as<int>() != 0;
        cmd.out3 = dataObj["out3"].as<int>() != 0;
        cmd.out4 = dataObj["out4"].as<int>() != 0;
    }
    else if (opcode == 3)
    {
        cmd.opcode = 3;
    }
    if (commandCallback)
    {
        commandCallback(cmd);
    }
    if(opcode == 3)
    {
        return;
    }
    // sendResponse(id_des, opcode + 100, unix_time, 0);
    sendResponse(id_des, opcode + 100, unix_time, 0, &cmd);
}

// inline void PCHandler::sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status)
inline void PCHandler::sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status, const MistCommand *cmd)
{
    StaticJsonDocument<512> resp_doc;
    resp_doc["id_des"] = id_des;
    resp_doc["opcode"] = resp_opcode;
    JsonObject data = resp_doc.createNestedObject("data");
    data["status"] = status;
    (void)cmd;
    resp_doc["time"] = unix_time;

    // Tính auth cho phản hồi
    String id_des_str = String(id_des);
    String opcode_str = String(resp_opcode);
    String time_str = String(unix_time);

    String data_json;
    serializeJson(resp_doc["data"], data_json);

    String combined = id_des_str + opcode_str + data_json + time_str + SECRET_KEY;
    String auth_hash = calculateMD5(combined);

    resp_doc["auth"] = auth_hash;

    String output;
    serializeJson(resp_doc, output);
    // Serial.print(F("TX PC RESP: "));
    Serial.println(output);
    // Serial.println(output);
}

#endif
