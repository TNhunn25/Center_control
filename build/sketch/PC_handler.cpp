#line 1 "D:\\phunsuong\\master\\master\\PC_handler.cpp"
#include "PC_handler.h"
#include "led_status.h"
extern LedStatus led_status;
PCHandler::PCHandler()
{
    rxLine.reserve(1024);
}

void PCHandler::begin()
{
    Serial.begin(115200);
    lastRxMs = millis();
}

void PCHandler::onCommandReceived(CommandCallback cb)
{
    commandCallback = cb;
}

void PCHandler::update()
{
    while (Serial.available())
    {

        char c = (char)Serial.read();
        if (c == '\n')
        {
            rxLine.trim();
            if (rxLine.length() == 0)
            {
                rxLine = "";
                continue;
            }
            lastRxMs = millis();
            if (inTimeout)
            {
                inTimeout = false;
            }
            processLine();
            rxLine = "";
        }
        else
        {
            if (rxLine.length() < 512)
            {
                rxLine += c;
            }
        }
    }
}

// Xác thực auth nhận từ PC
bool verifyAuth(const JsonDocument &doc, const String &receivedHash)
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

void PCHandler::processLine()
{

    DeserializationError error = deserializeJson(doc, rxLine);
    if (error)
    {
        return;
    }
    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();
    String receivedHash = doc["auth"].as<String>();

    if (id_des < 0 || (opcode != 1 && opcode != 2 && opcode != 3 && opcode != 4 && opcode != 5))
    {
        sendResponse(id_des, opcode + 100, unix_time, 255);
        return;
    }

    if (!verifyAuth(doc, receivedHash))
    {
        sendResponse(id_des, opcode + 100, unix_time, 1);
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
        auto dataObj = doc["data"].as<JsonObject>();
        cmd.opcode = 2;
        cmd.out1 = dataObj["out1"] | false;
        cmd.out2 = dataObj["out2"] | false;
        cmd.out3 = dataObj["out3"] | false;
        cmd.out4 = dataObj["out4"] | false;
    }
    else if (opcode == 3)
    {
        cmd.opcode = opcode;
    }
    else if (opcode == 4)
    {

        JsonObjectConst dataObj = doc["data"].as<JsonObjectConst>();
        if (dataObj.isNull())
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }

        cmd.opcode = 4;
        cmd.motor_mask = 0;

        auto parseMotor = [&](uint8_t idx)
        {
            String key = String("m") + String(idx + 1);
            if (!dataObj.containsKey(key))
                return;

            JsonVariantConst v = dataObj[key];
            if (v.is<int>())
            {
                cmd.motors[idx].run = (uint8_t)v.as<int>();
                cmd.motors[idx].dir = 0;
                cmd.motors[idx].speed = 0;
                cmd.motor_mask |= (1u << idx);
                return;
            }

            JsonObjectConst m = v.as<JsonObjectConst>();
            if (m.isNull() || !m.containsKey("run") || !m.containsKey("dir") || !m.containsKey("speed"))
                return;

            cmd.motors[idx].run = (uint8_t)m["run"].as<int>();
            cmd.motors[idx].dir = (uint8_t)m["dir"].as<int>();
            cmd.motors[idx].speed = (uint8_t)m["speed"].as<int>();
            cmd.motor_mask |= (1u << idx);
        };

        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
            parseMotor(i);

        if (cmd.motor_mask == 0)
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }
    }
    else if (opcode == 5)
    {
        cmd.opcode = opcode;
    }

    if (commandCallback)
    {
        commandCallback(cmd);
    }
    has_data_serial = true;
    // opcode 2/3: CentralController sẽ trả response đúng opcode (2 hoặc 3)
    if (opcode == 2 || opcode == 3)
        return;
    if (opcode == 3 || opcode == 4 || opcode == 5)
        return;
    sendResponse(id_des, opcode + 100, unix_time, 0);
}

void PCHandler::sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status)
{
    StaticJsonDocument<512> resp_doc;
    resp_doc["id_des"] = id_des;
    resp_doc["opcode"] = resp_opcode;
    JsonObject data = resp_doc.createNestedObject("data");
    data["status"] = status;
    resp_doc["time"] = unix_time;

    // Tính auth cho phản hồirr
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
    Serial.println(output);
}
// void PCHandler::checkPacketTimeout()
// {
//     uint32_t now = millis();

//     if (!inTimeout && (now - lastRxMs >= timeoutMs))
//     {
//         inTimeout = true;
//         led_status.setState(LedStatus::STATE_NORMAL);
//         Serial.println("[PC --> Center Control] Time out!");
//     }
// }