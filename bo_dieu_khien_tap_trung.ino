#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "md5.h"
#include "ethernet_handler.h"

// ===================== HW PINS =====================

// Input buttons (MAN) - dùng INPUT_PULLUP, nhấn = LOW
static const uint8_t BTN_PINS[4] = {16, 17, 18, 19};

// Output channels
static const uint8_t OUT_PINS[4] = {25, 26, 27, 32};

// AUTO/MAN switch
static const uint8_t MODE_PIN = 23; // INPUT_PULLUP: HIGH=AUTO, LOW=MAN (đổi nếu cần)

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

// ===================== PROTOCOL =====================
const String SECRET_KEY = "ALTA_MIST_CONTROLLER";

// Nếu bạn đã có nguồn UnixTime thật (RTC/NTP) thì thay hàm này.
// Tạm thời: không check timeout 60s -> luôn hợp lệ.
static bool isTimeValid(uint32_t /*requestUnix*/)
{
    return true;
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
bool outState[4] = {false, false, false, false}; // true=ON

EthernetUDPHandler eth;

// RX buffer RS485 line-based
String rs485Line;

// ===================== OUTPUT CONTROL =====================
static void writeOutput(uint8_t ch, bool on)
{
    outState[ch] = on;

    bool pinLevel = on;
    if (OUT_ACTIVE_LOW)
        pinLevel = !pinLevel;

    digitalWrite(OUT_PINS[ch], pinLevel ? HIGH : LOW);
}

static void applyIOCommand(bool o1, bool o2, bool o3, bool o4)
{
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

static String buildResponseJson(int id_des, int resp_opcode, uint32_t unix_time, int status)
{
    StaticJsonDocument<256> resp;
    resp["id_des"] = id_des;
    resp["opcode"] = resp_opcode;

    JsonObject data = resp.createNestedObject("data");
    data["status"] = status;

    resp["time"] = unix_time;

    String data_json;
    serializeJson(resp["data"], data_json);

    String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
    resp["auth"] = calculateMD5(combined);

    String out;
    serializeJson(resp, out);
    return out;
}

static bool isAutoMode()
{
    // INPUT_PULLUP: HIGH=AUTO, LOW=MAN
    return digitalRead(MODE_PIN) == HIGH;
}

// ===================== SEND RESP VIA RS485 =====================
static void rs485SendLine(const String &line)
{
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

// ===================== HANDLE ONE JSON COMMAND =====================
static void handleJsonCommand(const char *json, size_t len,
                              bool fromRs485,
                              IPAddress udpIp = IPAddress(0, 0, 0, 0),
                              uint16_t udpPort = 0)
{
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
        String resp = buildResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(resp);
        else
            udpSendResponse(udpIp, udpPort, resp);
        return;
    }

    // opcode chỉ xử lý IO_COMMAND=2 cho center_control
    if (opcode != 2)
    {
        String resp = buildResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(resp);
        else
            udpSendResponse(udpIp, udpPort, resp);
        return;
    }

    // check time validity (nếu bạn dùng)
    if (!isTimeValid(unix_time))
    {
        String resp = buildResponseJson(id_des, resp_opcode, unix_time, 2);
        if (fromRs485)
            rs485SendLine(resp);
        else
            udpSendResponse(udpIp, udpPort, resp);
        return;
    }

    // auth
    if (!verifyAuth(doc, receivedHash))
    {
        String resp = buildResponseJson(id_des, resp_opcode, unix_time, 1);
        if (fromRs485)
            rs485SendLine(resp);
        else
            udpSendResponse(udpIp, udpPort, resp);
        return;
    }

    // MAN mode: không cho remote điều khiển
    if (!isAutoMode())
    {
        String resp = buildResponseJson(id_des, resp_opcode, unix_time, 255);
        if (fromRs485)
            rs485SendLine(resp);
        else
            udpSendResponse(udpIp, udpPort, resp);
        return;
    }

    // Parse out1..out4 (0/1 hoặc bool đều ok)
    JsonObject dataObj = doc["data"].as<JsonObject>();
    bool o1 = (dataObj["out1"].as<int>() != 0);
    bool o2 = (dataObj["out2"].as<int>() != 0);
    bool o3 = (dataObj["out3"].as<int>() != 0);
    bool o4 = (dataObj["out4"].as<int>() != 0);

    applyIOCommand(o1, o2, o3, o4);

    String resp = buildResponseJson(id_des, resp_opcode, unix_time, 0);
    if (fromRs485)
        rs485SendLine(resp);
    else
        udpSendResponse(udpIp, udpPort, resp);
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

// ===================== SETUP/LOOP =====================
void setup()
{
    Serial.begin(115200);
    delay(100);

    // MODE
    pinMode(MODE_PIN, INPUT_PULLUP);

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
    eth.begin();              // default pins (CS=6,RST=5,SCK=7,MISO=8,MOSI=9)
    eth.onReceive(onUdpPacket);

    Serial.println(F("{\"center_control\":\"started\"}"));
}

void loop()
{
    // 1) UDP + RS485 luôn chạy song song
    eth.update();
    updateRs485();

    // 2) MAN mode: nút cơ toggle output
    if (!isAutoMode())
    {
        for (int i = 0; i < 4; i++)
        {
            if (debouncePress(i))
            {
                Serial.printf("MAN: Button %d pressed -> toggle out%d\n", i + 1, i + 1);
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