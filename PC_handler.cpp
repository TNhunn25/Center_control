#include "PC_handler.h"
#include "led_status.h"
extern LedStatus led_status;

// Khởi tạo handler, chuẩn bị bộ đệm nhận.
PCHandler::PCHandler()
{
    rxLine.reserve(1024);
}

// Khởi động Serial và đánh dấu thời điểm nhận cuối.
void PCHandler::begin()
{
    Serial.begin(115200);
    lastRxMs = millis();
}

// Đăng ký callback để xử lý lệnh đã parse.
void PCHandler::onCommandReceived(CommandCallback cb)
{
    commandCallback = cb;
}

// Đọc Serial theo dòng, gom dữ liệu đến khi gặp '\n' rồi xử lý.
void PCHandler::update()
{

    // const uint32_t startMs = millis();
    // const uint32_t maxWorkMs = 5;
    while (Serial.available())
    {

        char c = (char)Serial.read();
        if (c == '\n')
        {
            rxLine.trim();

            // Serial.println(rxLine);           // ← đây là chỗ bạn cần nhìn
            if (rxLine.length() == 0)
            {
                rxLine = "";
                continue;
            }
            // lastRxMs = millis(); // test
            // if (inTimeout)
            // {
            //     inTimeout = false;
            // }
            processLine();
            rxLine = "";
        }
        else
        {
            if (rxLine.length() < 1024)
            {
                rxLine += c;
            }
        }
        // checkPacketTimeout(); //new test
        // if ((uint32_t)(millis() - startMs) >= maxWorkMs)
        // {
        //     break;
        // }
    }
    if (has_data_serial && (uint32_t)(millis() - lastRxMs) >= timeoutMs)
    {
        has_data_serial = false;
        inTimeout = true;
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

// Parse JSON, xác thực, tạo command và gọi callback.
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

    if (id_des < 0 || (opcode != 1 && opcode != 2 && opcode != 3 && opcode != 4 && opcode != 5 && opcode != 6))
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
        // auto dataObj = doc["data"].as<JsonObject>();
        JsonObjectConst dataObj = doc["data"].as<JsonObjectConst>();
        if (dataObj.isNull() || !dataObj.containsKey("out1") || !dataObj.containsKey("out2") ||
            !dataObj.containsKey("out3") || !dataObj.containsKey("out4"))
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }

        auto parseOutput = [&](const JsonObjectConst &obj, const char *key, bool &outValue) -> bool
        {
            int raw = obj[key].as<int>();
            if (raw == 0)
            {
                outValue = true;
                return true;
            }
            if (raw == 1)
            {
                outValue = false;
                return true;
            }
            return false;
        };
        cmd.opcode = 2;
        // cmd.out1 = dataObj["out1"].as<int>() != 0;
        // cmd.out2 = dataObj["out2"].as<int>() != 0;
        // cmd.out3 = dataObj["out3"].as<int>() != 0;
        // cmd.out4 = dataObj["out4"].as<int>() != 0;

        if (!parseOutput(dataObj, "out1", cmd.out1) ||
            !parseOutput(dataObj, "out2", cmd.out2) ||
            !parseOutput(dataObj, "out3", cmd.out3) ||
            !parseOutput(dataObj, "out4", cmd.out4))
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }
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
    else if (opcode == 6)
    {
        JsonObjectConst dataObj = doc["data"].as<JsonObjectConst>();
        if (dataObj.isNull())
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }

        cmd.opcode = 6;
        cmd.thresholds.temp_on = dataObj["temp_on"] | NAN;
        cmd.thresholds.humi_on = dataObj["humi_on"] | NAN;
        cmd.thresholds.nh3_on = dataObj["nh3_on"] | NAN;
        cmd.thresholds.co_on = dataObj["co_on"] | NAN;
        cmd.thresholds.no2_on = dataObj["no2_on"] | NAN;
        cmd.thresholds.temp_off = dataObj["temp_off"] | NAN;
        cmd.thresholds.humi_off = dataObj["humi_off"] | NAN;
        cmd.thresholds.nh3_off = dataObj["nh3_off"] | NAN;
        cmd.thresholds.co_off = dataObj["co_off"] | NAN;
        cmd.thresholds.no2_off = dataObj["no2_off"] | NAN;

        if (isnan(cmd.thresholds.temp_on) || isnan(cmd.thresholds.humi_on) ||
            isnan(cmd.thresholds.nh3_on) || isnan(cmd.thresholds.co_on) ||
            isnan(cmd.thresholds.no2_on) || isnan(cmd.thresholds.temp_off) ||
            isnan(cmd.thresholds.humi_off) || isnan(cmd.thresholds.nh3_off) ||
            isnan(cmd.thresholds.co_off) || isnan(cmd.thresholds.no2_off))
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }
    }
    if (commandCallback)
    {
        commandCallback(cmd);
    }
    has_data_serial = true;
    lastRxMs = millis();
    if (inTimeout)
    {
        inTimeout = false;
    }
    // digitalWrite(1, HIGH); // để nháy GPIO1 (UART0 TX)
    // delay(50);
    // digitalWrite(1, LOW);
    // opcode 2/3: CentralController sẽ trả response đúng opcode (2 hoặc 3)
    if (opcode >= 2 && opcode <= 6)
    {
        return;
    }
}

// Gửi phản hồi trạng thái về PC kèm auth.
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
// void PCHandler::checkPacketTimeout() // check gói tin gửi liên tục
// {
//     uint32_t now = millis();

//     if (!inTimeout && (now - lastRxMs >= timeoutMs))
//     {
//         inTimeout = true;
//         led_status.setState(LedStatus::STATE_NORMAL);
//         Serial.println("[PC --> Center Control] Time out!");
//     }
// }
