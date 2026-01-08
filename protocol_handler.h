#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// External state owned by the main sketch.
extern const String SECRET_KEY;
extern bool outState[OUT_COUNT];
extern bool nutVuaNhan[IN_COUNT];
extern const uint8_t IN_PINS[IN_COUNT];

inline bool verifyAuth(const JsonDocument &doc, const String &receivedHash)
{
    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();

    String data_json;
    serializeJson(doc["data"], data_json); // compact

    String combined = String(id_des) + String(opcode) + data_json + String(unix_time) + SECRET_KEY;
    String computedHash = calculateMD5(combined);

    return computedHash.equalsIgnoreCase(receivedHash);
}

inline String ResponseJson(int id_des, int resp_opcode, uint32_t unix_time, int status)
{
    StaticJsonDocument<520> Json;
    Json["id_des"] = id_des;
    Json["opcode"] = resp_opcode;

    JsonObject data = Json.createNestedObject("data");
    data["status"] = status;
    data["out1"] = outState[0] ? 1 : 0;
    data["out2"] = outState[1] ? 1 : 0;
    data["out3"] = outState[2] ? 1 : 0;
    data["out4"] = outState[3] ? 1 : 0;
    Json["time"] = unix_time;

    String data_json;
    serializeJson(Json["data"], data_json);

    String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
    Json["auth"] = calculateMD5(combined);

    String out;
    serializeJson(Json, out);
    return out;
}

// Build a JSON command with auth for forwarding to nodes.
inline String CommandJson(const MistCommand &cmd)
{
    StaticJsonDocument<256> dataDoc;
    switch (cmd.opcode)
    {
    case MIST_COMMAND:
    {
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    break;
    case IO_COMMAND:
    {
        dataDoc["out1"] = cmd.out1 ? 1 : 0;
        dataDoc["out2"] = cmd.out2 ? 1 : 0;
        dataDoc["out3"] = cmd.out3 ? 1 : 0;
        dataDoc["out4"] = cmd.out4 ? 1 : 0;
    }
    break;
    case MOTOR_COMMAND:
{
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        if ((cmd.motor_mask & (1u << i)) == 0)
            continue;

        JsonObject m = dataDoc.createNestedObject(String("m") + String(i + 1));
        m["run"] = cmd.motors[i].run;
        m["dir"] = cmd.motors[i].dir;
        m["speed"] = cmd.motors[i].speed;
    }
}
break;
    default:
        return "";
    }

    String data_json;
    serializeJson(dataDoc, data_json);

    String combined = String(cmd.id_des) + String(cmd.opcode) + data_json + String(cmd.unix) + SECRET_KEY;
    String auth = calculateMD5(combined);

    StaticJsonDocument<512> doc;
    doc["id_des"] = cmd.id_des;
    doc["opcode"] = cmd.opcode;
    doc["data"] = dataDoc;
    doc["time"] = cmd.unix;
    doc["auth"] = auth;

    String out;
    serializeJson(doc, out);
    return out;
}

inline String Goi_trangthai(int id_des, int resp_opcode, uint32_t unix_time)
{
    StaticJsonDocument<256> Json;
    Json["id_des"] = id_des;
    Json["opcode"] = resp_opcode;

    JsonObject data = Json.createNestedObject("data");
    data["out1"] = outState[0] ? 1 : 0;
    data["out2"] = outState[1] ? 1 : 0;
    data["out3"] = outState[2] ? 1 : 0;
    data["out4"] = outState[3] ? 1 : 0;

    for (int i = 0; i < IN_COUNT; i++)
    {
        bool active = (digitalRead(IN_PINS[i]) == LOW);
        int value = nutVuaNhan[i] ? 2 : (active ? 1 : 0);
        data[String("in") + String(i + 1)] = value;
    }

    Json["time"] = unix_time;

    String data_json;
    serializeJson(Json["data"], data_json);

    String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
    Json["auth"] = calculateMD5(combined);

    String out;
    serializeJson(Json, out);
    return out;
}

#endif
