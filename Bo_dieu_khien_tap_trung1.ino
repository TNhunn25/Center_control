#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "PC_handler.h"
#include "auto_man.h"
#include "eeprom_state.h"
#include "serial_control.h"
#include "pcf8575_io.h"
#include "led_status.h"

struct VocReading;
static String Goi_VOC(int id_des, int resp_opcode, uint32_t unix_time, const VocReading &voc);

// ===================== HW PINS =====================

// Input buttons (MAN) - dùng INPUT_PULLUP, nhấn = LOW
PCF8575IO pcf;

// Relay active low?
static const bool OUT_ACTIVE_LOW = false;

PCHandler pcHandler;

// ===================== PROTOCOL =====================
// const String SECRET_KEY = "ALTA_MIST_CONTROLLER";
extern const String SECRET_KEY;
// Last PC packet time for auto-push correlation.
static uint32_t lastPcUnixTime = 0;
static bool pendingPushAfterAck = false;
static bool autoPushEnabled = false;
static String lastTrangthaiDataJson;
bool serialTextPushRequested = false;
bool serialTextPushForce = false;
// static String Goi_VOC(int id_des, int resp_opcode, uint32_t unix_time);

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

ButtonState btn[IN_COUNT];
bool outState[OUT_COUNT] = {false, false, false, false};  // true=ON
bool nutVuaNhan[IN_COUNT] = {false, false, false, false}; // true=just pressed

static MotorCommand motorState[MOTOR_COUNT] = {};

static VocReading vocReading = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// EthernetUDPHandler eth;

// RX buffer RS485 line-based
// String rs485Line;

// ===================== OUTPUT CONTROL =====================
static void writeOutput(uint8_t ch, bool on)
{
    bool prevState = outState[ch]; // lưu trạng thái cũ để chỉ log khi có thay đổi thật
    outState[ch] = on;

    bool pinLevel = on;
    if (OUT_ACTIVE_LOW)
        pinLevel = !pinLevel;

    pcf.writeOutput(ch, pinLevel ? HIGH : LOW);

    if (prevState != on)
        eepromStateMarkDirty();

    // // Debug: chỉ in log khi trạng thái output thay đổi
    // Serial.printf("OUT%d -> %s\n", ch + 1, on ? "ON" : "OFF");
}

void applyIOCommand(bool o1, bool o2, bool o3, bool o4)
{
    // Serial.printf("CMD OUT: o1=%d o2=%d o3=%d o4=%d\n", o1 ? 1 : 0, o2 ? 1 : 0, o3 ? 1 : 0, o4 ? 1 : 0);
    writeOutput(0, o1);
    writeOutput(1, o2);
    writeOutput(2, o3);
    writeOutput(3, o4);

    // log 1 lần duy nhất cho 1 gói
    // Serial.printf("RX OUT PACKET: OUT1= %s OUT2= %s OUT3= %s OUT4= %s\n",
    //               o1 ? "ON" : "OFF",
    //               o2 ? "ON" : "OFF",
    //               o3 ? "ON" : "OFF",
    //               o4 ? "ON" : "OFF");
}

static void applyMotorCommand(const MistCommand &cmd)
{
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        if ((cmd.motor_mask & (1u << i)) == 0)
            continue;

        motorState[i] = cmd.motors[i];

        // TODO: map motorState[i] -> pins (run/dir/speed)
    }
}

static void toggleOutput(uint8_t ch)
{
    writeOutput(ch, !outState[ch]);
}

// ===================== DEBOUNCE =====================
static bool debounceUpdate(uint8_t i, bool &stableLevel)
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

// Tạo Json lệnh đầy đủ (Kèm auth) để forward xuống node
static String CommandJson(const MistCommand &cmd)
{
    StaticJsonDocument<256> dataDoc;
    switch (cmd.opcode)
    {
    case MIST_COMMAND:
    {
        // Mist_command
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    break;
    case IO_COMMAND:
    {
        // IO_command
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
    case SENSOR_VOC:
    {
        // SENSOR_VOC has empty data payload.
    }
    break;
    default:
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

String Goi_VOC(int id_des, int resp_opcode, uint32_t unix_time, const VocReading &voc)
{
    StaticJsonDocument<256> Json;
    Json["id_des"] = id_des;
    Json["opcode"] = resp_opcode;

    JsonObject data = Json.createNestedObject("data");
    data["temp"] = voc.temp;
    data["humi"] = voc.humi;
    data["nh3"] = voc.nh3;
    data["co"] = voc.co;
    data["no2"] = voc.no2;

    Json["time"] = unix_time;

    String data_json;
    serializeJson(Json["data"], data_json);

    String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
    Json["auth"] = calculateMD5(combined);

    String out;
    serializeJson(Json, out);
    return out;
}

static String buildTrangthaiDataJson()
{
    StaticJsonDocument<512> dataDoc;
    JsonObject data = dataDoc.to<JsonObject>();

    JsonObject sanban = data.createNestedObject("sanban");
    JsonObject motor = sanban.createNestedObject("motor");
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        JsonObject m = motor.createNestedObject(String("m") + String(i + 1));
        m["run"] = motorState[i].run;
        m["dir"] = motorState[i].dir;
        m["speed"] = motorState[i].speed;
    }

    JsonObject input = sanban.createNestedObject("input");
    for (int i = 0; i < IN_COUNT; i++)
    {
        bool active = !pcf.readInput(i);
        input[String("in") + String(i + 1)] = active ? 1 : 0;
    }

    JsonObject sensor_voc = data.createNestedObject("sensor_voc");
    sensor_voc["temp"] = vocReading.temp;
    sensor_voc["humi"] = vocReading.humi;
    sensor_voc["nh3"] = vocReading.nh3;
    sensor_voc["co"] = vocReading.co;
    sensor_voc["no2"] = vocReading.no2;

    JsonObject io_command = data.createNestedObject("io_command");
    io_command["out1"] = outState[0] ? 1 : 0;
    io_command["out2"] = outState[1] ? 1 : 0;
    io_command["out3"] = outState[2] ? 1 : 0;
    io_command["out4"] = outState[3] ? 1 : 0;
    for (int i = 0; i < IN_COUNT; i++)
    {
        bool active = !pcf.readInput(i);
        io_command[String("in") + String(i + 1)] = active ? 1 : 0;
    }

    String data_json;
    serializeJson(data, data_json);
    return data_json;
}

static String Goi_trangthai(int id_des, int resp_opcode, uint32_t unix_time, const String &data_json)
{
    StaticJsonDocument<768> Json;
    Json["id_des"] = id_des;
    Json["opcode"] = resp_opcode;

    DeserializationError err = deserializeJson(Json["data"], data_json);
    if (err)
    {
        Json["data"] = serialized(data_json);
    }

    Json["time"] = unix_time;

    String combined = String(id_des) + String(resp_opcode) + data_json + String(unix_time) + SECRET_KEY;
    Json["auth"] = calculateMD5(combined);

    String out;
    serializeJson(Json, out);
    return out;
}

static bool sendTrangthaiIfChanged(int id_des, int resp_opcode, uint32_t unix_time)
{
    String data_json = buildTrangthaiDataJson();
    if (data_json == lastTrangthaiDataJson)
        return false;

    Serial.println(Goi_trangthai(id_des, resp_opcode, unix_time, data_json));
    lastTrangthaiDataJson = data_json;
    return true;
}

static void sendTrangthaiForce(int id_des, int resp_opcode, uint32_t unix_time)
{
    String data_json = buildTrangthaiDataJson();
    Serial.println(Goi_trangthai(id_des, resp_opcode, unix_time, data_json));
    lastTrangthaiDataJson = data_json;
}

static void onPcCommand(const MistCommand &cmd)
{
    // Serial.print(F("RX PC: opcode="));
    // Serial.print(cmd.opcode);

    // Serial.print(F(" id_des="));
    // Serial.println(cmd.id_des);
    // Khi nhận lệnh từ PC:
    //- Opcode 2: Điều khiển output + trả trạng thái qua Serial
    //- Opcode khác: bỏ qua (serial- only)

    switch (cmd.opcode)
    {
    case GET_INFO:
    {
        // Enable auto-push when PC requests status.
        lastPcUnixTime = cmd.unix;
        autoPushEnabled = true;
        luuMocTrangThai();
        sendTrangthaiIfChanged(cmd.id_des, cmd.opcode + 100, cmd.unix);
        for (int i = 0; i < IN_COUNT; i++)
            nutVuaNhan[i] = false;
        return;
    }
    case IO_COMMAND:
    {
        lastPcUnixTime = cmd.unix; // reuse latest PC time for auto-push id

        if (isAutoMode())
        {
            // Lệnh từ PC chỉ áp dụng khi AUTO
            applyIOCommand(cmd.out1, cmd.out2, cmd.out3, cmd.out4);
            pendingPushAfterAck = true;
        }
        else
        {
            // MAN: bỏ qua lệnh PC, không phản hồi để tránh chặn loop
        }
        return;
    }
    case MOTOR_COMMAND:
    {
        lastPcUnixTime = cmd.unix;
        applyMotorCommand(cmd);
        autoPushEnabled = true;
        pendingPushAfterAck = true;
        return;
    }
    case SENSOR_VOC:
    {
        lastPcUnixTime = cmd.unix;
        autoPushEnabled = true;
        String js = Goi_VOC(cmd.id_des, SENSOR_VOC + 100, cmd.unix, vocReading); // 105
        Serial.println(js);
        pendingPushAfterAck = true;
        return;
    }
    default:
        break;
    }
}

// ===================== AUTO PUSH STATUS TO PC =====================
// "mốc trạng thái" để so sánh thay đổi
static bool mocTrangThaiOut[OUT_COUNT] = {false, false, false, false};
static bool mocTrangThaiIn[IN_COUNT] = {false, false, false, false};

// chống spam push
static uint32_t lanDayLenPC_ms = 0;
static const uint32_t CHONG_SPAM_MS = 120;

// opcode center tự phát gửi trạng thái về PC
static const int OPCODE_DAY_TRANGTHAI = GET_INFO + 100;

// Nếu chưa sync NTP/RTC (getUnixTime()=0) thì dùng millis/1000 tạm
static uint32_t layThoiGianGui()
{
    // Use the last PC time to keep packet ids consistent.
    // If PC time is unknown, return 0 to signal unsynced.
    if (lastPcUnixTime != 0)
        return lastPcUnixTime;
    return 0;
}

// Lưu "mốc trạng thái" hiện tại để lần sau so sánh
static void luuMocTrangThai()
{
    for (int i = 0; i < OUT_COUNT; i++)
    {
        mocTrangThaiOut[i] = outState[i];
        mocTrangThaiIn[i] = !pcf.readInput(i); // active LOW
    }
}

// Đẩy trạng thái về PC (tự phát)
static void dayTrangThaiVePC()
{
    uint32_t t = layThoiGianGui();
    if (t == 0)
        return;

    // Dùng lại hàm đóng gói trạng thái bạn đang có:
    // Goi_trangthai(id_des, opcode, time)
    // id_des: tùy bạn, ở đây giữ 1 cho PC
    sendTrangthaiIfChanged(1, OPCODE_DAY_TRANGTHAI, t);

    // clear cờ "nút vừa nhấn" để tránh gửi lặp đi lặp lại trạng thái=2
    for (int i = 0; i < IN_COUNT; i++)
        nutVuaNhan[i] = false;
}

// Kiểm tra thay đổi và tự push
static void tuDongDayNeuThayDoi()
{
    if (!autoPushEnabled)
        return;
    bool thayDoi = false;

    // 1) OUT thay đổi?
    for (int i = 0; i < OUT_COUNT; i++)
    {
        if (outState[i] != mocTrangThaiOut[i])
        {
            thayDoi = true;
            break;
        }
    }

    // 2) IN active thay đổi?
    if (!thayDoi)
    {
        for (int i = 0; i < IN_COUNT; i++)
        {
            bool inDangActive = !pcf.readInput(i);
            if (inDangActive != mocTrangThaiIn[i])
            {
                thayDoi = true;
                break;
            }
        }
    }

    // 3) Có nút vừa nhấn? (để gửi trạng thái in=2)
    if (!thayDoi)
    {
        for (int i = 0; i < IN_COUNT; i++)
        {
            if (nutVuaNhan[i])
            {
                thayDoi = true;
                break;
            }
        }
    }

    if (!thayDoi)
        return;

    // chống spam
    uint32_t nowMs = millis();
    if (nowMs - lanDayLenPC_ms < CHONG_SPAM_MS)
    {
        // vẫn cập nhật mốc để khỏi bị push dồn
        luuMocTrangThai();
        return;
    }

    lanDayLenPC_ms = nowMs;
    dayTrangThaiVePC();
    luuMocTrangThai();
}

bool swModeOverride = false;
bool swAutoMode = false;
inline bool getEffectiveAutoMode()
{
    return swModeOverride ? swAutoMode : isAutoMode();
}

// ===================== SETUP/LOOP =====================
void setup()
{
    Serial.begin(115200);
    delay(100);

    // serialTextControlInit();

    // MODE
    pcf.begin();
    pcf.prepareInputs();
    setupAutoManMode();
    pcHandler.begin();
    pcHandler.onCommandReceived(onPcCommand);
    eepromStateBegin();
    for (int i = 0; i < OUT_COUNT; i++)
    {
        writeOutput(i, false);
    }
    eepromStateLoad(writeOutput);
    luuMocTrangThai();

    // LEDs
    ledStatusBegin();
    setStatusLed(true, false);

    // Serial.println(F("{\"center_control\":\"started\"}"));
}

void loop()
{
    // 1) Chỉ nhận lệnh từ PC qua Serial
    pcHandler.update();
    if (pendingPushAfterAck)
    {
        pendingPushAfterAck = false;
        dayTrangThaiVePC();
        luuMocTrangThai();
    }
    if (serialTextPushRequested)
    {
        serialTextPushRequested = false;
        autoPushEnabled = true;
        if (lastPcUnixTime == 0)
            lastPcUnixTime = getUnixTime();
        if (serialTextPushForce)
        {
            serialTextPushForce = false;
            sendTrangthaiForce(1, OPCODE_DAY_TRANGTHAI, lastPcUnixTime);
        }
        else
        {
            sendTrangthaiIfChanged(1, OPCODE_DAY_TRANGTHAI, lastPcUnixTime);
        }
        luuMocTrangThai();
        for (int i = 0; i < IN_COUNT; i++)
            nutVuaNhan[i] = false;
    }
    // eth.update();
    // updateRs485();

    // 2) MAN mode: nút cơ toggle output  (AUTO thì chặn)
    if (!isAutoMode())
    {
        for (int i = 0; i < IN_COUNT; i++)
        {
            bool stableLevel = LOW;
            if (debounceUpdate(i, stableLevel))
            {
                bool pressed = (stableLevel == HIGH);
                if (pressed)
                    nutVuaNhan[i] = true;
                writeOutput(i, pressed);
                Serial.printf("MAN: Button %d %s -> out%d %s\n",
                              i + 1,
                              pressed ? "pressed" : "released",
                              i + 1,
                              pressed ? "ON" : "OFF"); // released OFF Pressed ON
            }
        }
    }

    // Auto-push status changes to PC in both AUTO and MAN when enabled.
    tuDongDayNeuThayDoi();

    // 3) LED status
    ledStatusUpdate(pcf);

    eepromStateUpdate(outState);

    delay(5);
}
