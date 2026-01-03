#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"
#include "ethernet_handler.h"
#include "auto_man.h"

// ===================== HW PINS =====================

// Input buttons (MAN) - dùng INPUT_PULLUP, nhấn = LOW
static const uint8_t BTN_PINS[4] = {14, 17, 18, 21};

// Output channels
static const uint8_t OUT_PINS[4] = {10, 11, 12, 13};

// Status LEDs
static const uint8_t LED_OK_PIN = 2;  // xanh
static const uint8_t LED_ERR_PIN = 4; // đỏ

// Relay active low?
static const bool OUT_ACTIVE_LOW = false;

// RS485 UART + DE/RE
static const int RS485_RX_PIN = 33;
static const int RS485_TX_PIN = 14;
static const uint8_t RS485_DE_RE = 15; // DE/RE nối chung (HIGH=TX, LOW=RX)
static const uint32_t RS485_BAUD = 115200;

// UDP
static const uint16_t UDP_PORT = 8888;

PCHandler pcHandler;

// ===================== PROTOCOL =====================
const String SECRET_KEY = "ALTA_MIST_CONTROLLER";

// Nếu bạn đã có nguồn UnixTime thật (RTC/NTP) thì thay hàm này.
// Tự động đồng bộ thời gian dựa trên gói đầu tiên nhận được.
static uint32_t unixTimeBase = 0; // unix time của gói tin đầu tiên
static uint32_t millisBase = 0;   // mốc millis() tương ứng với unixTimeBase

static uint32_t getUnixTime()
{
    // Nếu chưa đồng bộ, trả 0 để báo chưa có thời gian
    if (unixTimeBase == 0)
        return 0;

    // Ước lượng unix time bằng cách cộng thêm số giây đã trôi qua
    uint32_t elapsedSeconds = (millis() - millisBase) / 1000;
    return unixTimeBase + elapsedSeconds;
}

static bool isTimeValid(uint32_t requestUnix)
{
    // Gói đầu tiên dùng để đồng bộ thời gian, luôn hợp lệ
    if (unixTimeBase == 0)
    {
        unixTimeBase = requestUnix;
        millisBase = millis();
        return true;
    }

    uint32_t nowUnix = getUnixTime();
    // Chấp nhận sai lệnh +_60s
    uint32_t diff = (nowUnix > requestUnix) ? (nowUnix - requestUnix) : (requestUnix - nowUnix);
    return diff <= 60;
}

// ===================== STATE =====================
static const uint32_t DEBOUNCE_MS = 35;

struct ButtonState
{
    bool stableLevel;
    bool lastReading;
    uint32_t lastChangeMs;
};

ButtonState btn[4];
bool outState[4] = {false, false, false, false};   // true=ON
bool nutVuaNhan[4] = {false, false, false, false}; // true=just pressed

EthernetUDPHandler eth;

// RX buffer RS485 line-based
String rs485Line;

// ===================== OUTPUT CONTROL =====================
static void writeOutput(uint8_t ch, bool on)
{
    bool prevState = outState[ch]; // lưu trạng thái cũ để chỉ log khi có thay đổi thật
    outState[ch] = on;

    bool pinLevel = on;
    if (OUT_ACTIVE_LOW)
        pinLevel = !pinLevel;

    digitalWrite(OUT_PINS[ch], pinLevel ? HIGH : LOW);

    // Debug: chỉ in log khi trạng thái output thay đổi
    if (prevState != on)
    {
        Serial.printf("OUT%d -> %s\n", ch + 1, on ? "ON" : "OFF");
    }
}

static void applyIOCommand(bool o1, bool o2, bool o3, bool o4)
{
    Serial.printf("CMD OUT: o1=%d o2=%d o3=%d o4=%d\n", o1 ? 1 : 0, o2 ? 1 : 0, o3 ? 1 : 0, o4 ? 1 : 0);
    writeOutput(0, o1);
    writeOutput(1, o2);
    writeOutput(2, o3);
    writeOutput(3, o4);
}

static void toggleOutput(uint8_t ch)
{
    writeOutput(ch, !outState[ch]);
}

// ===================== DEBOUNCE =====================
static bool debouncePress(uint8_t i)
{
    bool reading = digitalRead(BTN_PINS[i]); // nhấn = LOW
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
            if (btn[i].stableLevel == LOW)
                return true; // press event
        }
    }
    return false;
}

// ===================== LED =====================
static void setStatusLed(bool ok, bool err)
{
    digitalWrite(LED_OK_PIN, ok ? HIGH : LOW);
    digitalWrite(LED_ERR_PIN, err ? HIGH : LOW);
}

// ===================== AUTH / PARSE =====================
static bool verifyAuth(const JsonDocument &doc, const String &receivedHash)
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

static String ResponseJson(int id_des, int resp_opcode, uint32_t unix_time, int status)
{
    StaticJsonDocument<320> Json;
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

// Tạo Json lệnh đầy đủ (Kèm auth) để forward xuống node
static String CommandJson(const MistCommand &cmd)
{
    StaticJsonDocument<256> dataDoc;
    if (cmd.opcode == 1)
    {
        // Mist_command
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    else if (cmd.opcode == 2)
    {
        // IO_command
        dataDoc["out1"] = cmd.out1 ? 1 : 0;
        dataDoc["out2"] = cmd.out2 ? 1 : 0;
        dataDoc["out3"] = cmd.out3 ? 1 : 0;
        dataDoc["out4"] = cmd.out4 ? 1 : 0;
    }
    else
    {
        return "";
    }

    String data_json;
    serializeJson(dataDoc, data_json);

    // Auth: id_des + opcode + data_Json + time + SECRET_KEY
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

static String Goi_trangthai(int id_des, int resp_opcode, uint32_t unix_time)
{
    StaticJsonDocument<256> Json;
    Json["id_des"] = id_des;
    Json["opcode"] = resp_opcode;

    JsonObject data = Json.createNestedObject("data");
    data["out1"] = outState[0] ? 1 : 0;
    data["out2"] = outState[1] ? 1 : 0;
    data["out3"] = outState[2] ? 1 : 0;
    data["out4"] = outState[3] ? 1 : 0;

    for (int i = 0; i < 4; i++)
    {
        bool active = (digitalRead(BTN_PINS[i]) == LOW);
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

// ===================== SEND RESP VIA RS485 =====================
static void rs485SendLine(const String &line)
{
    Serial.print(F("TX RS485: "));
    Serial.println(line);
    digitalWrite(RS485_DE_RE, HIGH); // TX enable
    delayMicroseconds(100);

    Serial2.print(line);
    Serial2.print("\n");
    Serial2.flush();

    delayMicroseconds(200);
    digitalWrite(RS485_DE_RE, LOW); // back to RX
}

// ===================== UDP SEND RESP =====================
static void udpSendResponse(IPAddress remoteIp, uint16_t remotePort, const String &line)
{
    Serial.print(F("TX UDP RESP: "));
    Serial.println(line);
    // dùng udp trực tiếp trong handler: class hiện không expose udp,
    // nên ta gửi broadcast lại cũng được.
    // Ở đây: gửi broadcast (đơn giản, đảm bảo PC nhận nếu nó listen cùng port)
    MistCommand dummy{};
    dummy.id_des = 1;
    dummy.opcode = 2;
    dummy.unix = 0;
    (void)remoteIp;
    (void)remotePort;

    // -> cách dễ nhất: print ra Serial để PC đọc qua USB
    // Nếu bạn muốn trả thẳng UDP unicast đúng remoteIp/remotePort,
    // mình sẽ chỉnh class EthernetUDPHandler expose sendRaw().
    Serial.print(F("UDP RESP: "));
    Serial.println(line);
}

// Forward lệnh từ PC xuống các node (RS485 + RJ45)
static void forwardCommandToNodes(const MistCommand &cmd)
{
    Serial.println(F("PC: forward command to nodes"));
    MistCommand forwardCmd = cmd;
    if (cmd.opcode == 1 && cmd.node_id > 0)
    {
        // opcode 1: gửi đúng node_id đích
        forwardCmd.id_des = cmd.node_id;
    }
    else if (cmd.opcode == 2 && cmd.id_des == 1)
    {
        // opcode 2: lệnh từ center -> broadcast xuống node
        forwardCmd.id_des = 0;
    }

    // build payload Json có auth để gửi xuống node
    String payload = CommandJson(forwardCmd);
    if (payload.length() == 0)
    {
        Serial.println(F("Forward: invalid opcode"));
    }

    // Gửi cả RS485 và UDP
    rs485SendLine(payload);
    bool updOk = eth.sendCommand(forwardCmd);

    Serial.print(F("Forward payload: "));
    Serial.print(payload);
    Serial.print(F(" | UDP = "));
    Serial.println(updOk ? F("OK") : F("FAIL"));
}

// ===================== HANDLE ONE JSON COMMAND =====================
static void handleJsonCommand(const char *json, size_t len,
                              bool fromRs485,
                              IPAddress udpIp = IPAddress(0, 0, 0, 0),
                              uint16_t udpPort = 0)
{
    Serial.print(F("RX "));
    Serial.print(fromRs485 ? F("RS485") : F("UDP"));
    Serial.print(F(": "));
    Serial.write(json, len);
    Serial.println();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json, len);
    if (err)
        return;

    int id_des = doc["id_des"].as<int>();
    int opcode = doc["opcode"].as<int>();
    uint32_t unix_time = doc["time"].as<uint32_t>();
    String receivedHash = doc["auth"].as<String>();

    int resp_opcode = opcode + 100;

    // chỉ nhận cho center id_des=1 (hoặc id_des=0 broadcast)
    if (!(id_des == 1 || id_des == 0))
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }

    // opcode chỉ xử lý IO_COMMAND=2 và GET_INFO=3 cho center_control
    if (opcode != 2 && opcode != 3)
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }

    // check time validity (nếu bạn dùng)
    if (!isTimeValid(unix_time))
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 2);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }

    // auth
    if (!verifyAuth(doc, receivedHash))
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 1);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }

    // MAN mode: không cho remote điều khiển
    if (opcode == 2 && !isAutoMode())
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }

    // opcode == 3: trả trạng thái
    if (opcode == 3)
    {
        String Json = Goi_trangthai(id_des, resp_opcode, unix_time);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);

        for (int i = 0; i < 4; i++)
            nutVuaNhan[i] = false;
        return;
    }

    // opcode == 2: Parse out1..out4 (0/1 hoặc bool đều ok)
    JsonObjectConst dataObj = doc["data"].as<JsonObjectConst>();
    if (dataObj.isNull() || !dataObj.containsKey("out1") || !dataObj.containsKey("out2") ||
        !dataObj.containsKey("out3") || !dataObj.containsKey("out4"))
    {
        String Json = ResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(Json);
        else
            udpSendResponse(udpIp, udpPort, Json);
        return;
    }
    bool o1 = (dataObj["out1"].as<int>() != 0);
    bool o2 = (dataObj["out2"].as<int>() != 0);
    bool o3 = (dataObj["out3"].as<int>() != 0);
    bool o4 = (dataObj["out4"].as<int>() != 0);

    applyIOCommand(o1, o2, o3, o4);

    String Json = ResponseJson(id_des, resp_opcode, unix_time, 0);
    if (fromRs485)
        rs485SendLine(Json);
    else
        udpSendResponse(udpIp, udpPort, Json);
}

// ===================== UDP CALLBACK =====================
static void onUdpPacket(const char *payload, int len, IPAddress rip, uint16_t rport)
{
    handleJsonCommand(payload, (size_t)len, false, rip, rport);
}

// ===================== RS485 UPDATE =====================
static void updateRs485()
{
    while (Serial2.available())
    {
        char c = (char)Serial2.read();
        if (c == '\n')
        {
            rs485Line.trim();
            if (rs485Line.length() > 0)
            {
                handleJsonCommand(rs485Line.c_str(), rs485Line.length(), true);
            }
            rs485Line = "";
        }
        else
        {
            if (rs485Line.length() < 512)
                rs485Line += c;
        }
    }
}

static void onPcCommand(const MistCommand &cmd)
{
    Serial.print(F("RX PC: opcode="));
    Serial.print(cmd.opcode);
    Serial.print(F(" id_des="));
    Serial.println(cmd.id_des);
    // Khi nhận lệnh từ PC:
    //-Opcode 2: Các cổng output điều khiển các node
    //-opcode 1: forward xuống các node khác
    if (cmd.opcode == 3)
    {
        String Json = Goi_trangthai(cmd.id_des, cmd.opcode + 100, cmd.unix);
        Serial.println(Json);
        for (int i = 0; i < 4; i++)
            nutVuaNhan[i] = false;
        return;
    }
    if (cmd.opcode == 2)
    {
        // Lệnh từ PC luôn xử lý như AUTO (MAN chỉ áp dụng cho thao tác tay)
        applyIOCommand(cmd.out1, cmd.out2, cmd.out3, cmd.out4);
        return;
    }
    forwardCommandToNodes(cmd);
}

// ===================== SETUP/LOOP =====================
void setup()
{
    Serial.begin(115200);
    delay(100);

    // MODE
    setupAutoManMode();
    pcHandler.begin();
    pcHandler.onCommandReceived(onPcCommand);
    // Inputs
    for (int i = 0; i < 4; i++)
    {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
        btn[i].stableLevel = digitalRead(BTN_PINS[i]);
        btn[i].lastReading = btn[i].stableLevel;
        btn[i].lastChangeMs = millis();
    }

    // Outputs
    for (int i = 0; i < 4; i++)
    {
        pinMode(OUT_PINS[i], OUTPUT);
        writeOutput(i, false);
    }

    // LEDs
    pinMode(LED_OK_PIN, OUTPUT);
    pinMode(LED_ERR_PIN, OUTPUT);
    setStatusLed(true, false);

    // RS485
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW); // RX
    Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Ethernet UDP
    eth.begin(); // default pins (CS=6,RST=5,SCK=7,MISO=8,MOSI=9)
    eth.onReceive(onUdpPacket);

    Serial.println(F("{\"center_control\":\"started\"}"));
}

void loop()
{
    // 1) UDP + RS485 luôn chạy song song
    pcHandler.update();
    eth.update();
    updateRs485();

    // 2) MAN mode: nút cơ toggle output  (AUTO thì chặn)
    if (!isAutoMode())
    {
        for (int i = 0; i < 4; i++)
        {
            if (debouncePress(i))
            {
                Serial.printf("MAN: Button %d pressed -> toggle out%d\n", i + 1, i + 1);
                nutVuaNhan[i] = true;
                toggleOutput(i);
            }
        }
    }

    // 3) LED status
    bool anyOn = false;
    for (int i = 0; i < 4; i++)
        if (outState[i])
            anyOn = true;

    // OK LED = có output ON, ERR LED = link ethernet down (chỉ báo)
    bool err = !eth.isLinkUp();
    setStatusLed(anyOn, err);

    delay(5);
}