#include "PC_handler.h"
const String SECRET_KEY = "ALTA_MIST_CONTROLLER";

PCHandler::PCHandler()
{
    rxLine.reserve(256);
}

void PCHandler::begin()
{
    Serial.begin(115200);
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

// Hàm tính MD5 bằng MD5Builder (chuẩn ESP32)
String calculateMD5(const String &input)
{
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString(); // Trả về hex lowercase 32 ký tự
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
    StaticJsonDocument<512> doc; 
    DeserializationError error = deserializeJson(doc, rxLine);
    if (error)
    {
        Serial.println(F("{\"error\":\"Json parse error\"}"));
        return;
    }

    // Kiểm tra các trường bắt buộc
    if (!doc.containsKey("id_des") || !doc.containsKey("opcode") || !doc.containsKey("data") ||
        !doc.containsKey("time") || !doc.containsKey("auth"))
    {
        Serial.println(F("{\"error\":\"Missing required fields\"}"));
        return;
    }

    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();
    String receivedHash = doc["auth"].as<String>();

    if (id_des < 0 || (opcode != 1 && opcode != 2))
    {
        sendResponse(id_des, opcode + 100, unix_time, 255);
        return;
    }

    // Kiểm tra thời gian hết hạn (±60 giây)
    uint32_t current_time = getCurrentUnixTime(); // <-- BẠN CẦN IMPLEMENT HÀM NÀY
    if (abs((int32_t)(current_time - unix_time)) > 60)
    {
        sendResponse(id_des, opcode + 100, unix_time, 2); // Invalid time
        return;
    }

    // Xác thực MD5
    if (!verifyAuth(doc, receivedHash))
    {
        sendResponse(id_des, opcode + 100, unix_time, 1); // Auth failed
        return;
    }

    // === AUTH OK → Xử lý lệnh ===
    MistCommand cmd{};
    cmd.unix = unix_time;
    cmd.id_des = (int8_t)id_des;

    if (opcode == 1)
    { // MIST_COMMAND
        auto dataObj = doc["data"].as<JsonObject>();
        if (!dataObj.containsKey("node_id") || !dataObj.containsKey("time_phase1") || !dataObj.containsKey("time_phase2"))
        {
            sendResponse(id_des, opcode + 100, unix_time, 255);
            return;
        }
        cmd.opcode = 1;
        cmd.node_id = dataObj["node_id"].as<int8_t>();
        cmd.time_phase1 = dataObj["time_phase1"].as<uint16_t>();
        cmd.time_phase2 = dataObj["time_phase2"].as<uint16_t>();
    }
    else if (opcode == 2)
    { // IO_COMMAND
        auto dataObj = doc["data"].as<JsonObject>();
        cmd.opcode = 2;
        cmd.out1 = dataObj["out1"] | false;
        cmd.out2 = dataObj["out2"] | false;
        cmd.out3 = dataObj["out3"] | false;
        cmd.out4 = dataObj["out4"] | false;
    }

    if (commandCallback)
    {
        commandCallback(cmd);
    }

    // Phản hồi thành công
    sendResponse(id_des, opcode + 100, unix_time, 0);
}

void PCHandler::sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status)
{
    StaticJsonDocument<384> resp_doc;
    resp_doc["id_des"] = id_des;
    resp_doc["opcode"] = resp_opcode;
    JsonObject data = resp_doc.createNestedObject("data");
    data["status"] = status;
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
    Serial.println(output);
}

// Placeholder – bạn cần implement thực tế (RTC hoặc NTP)
uint32_t getCurrentUnixTime()
{
    // Ví dụ tạm (chỉ để test): return 1760000000UL;
    // Thực tế: dùng NTPClient hoặc DS3231 RTC
    return time(nullptr); // Nếu dùng <time.h> và config NTP
}