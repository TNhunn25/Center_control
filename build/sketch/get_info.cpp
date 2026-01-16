#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\phunsuong\\master\\master\\get_info.cpp"
#include "get_info.h"
#include <math.h>

static inline uint8_t clampU8(int v, int lo, int hi)
{
    if (v < lo)
        v = lo;
    if (v > hi)
        v = hi;
    return (uint8_t)v;
}
static inline float safeFloat(JsonVariantConst v, float fallback)
{
    if (v.is<float>() || v.is<double>())
        return (float)v.as<float>();
    if (v.is<int>() || v.is<long>())
        return (float)v.as<long>();
    return fallback;
}
// so sánh float an toàn (node VOC thường đã làm tròn 1 chữ số)
static inline bool floatChanged(float a, float b, float eps = 0.05f)
{
    return fabsf(a - b) >= eps;
}

uint32_t GetInfoAggregator::nowUnix() const
{
    if (_unixBase != 0)
    {
        uint32_t elapsed = (millis() - _millisBase) / 1000;
        return _unixBase + elapsed;
    }
    // fallback: dùng millis/1000 để nhỏ hơn (tránh quá lớn)
    return (uint32_t)(millis() / 1000);
}

void GetInfoAggregator::syncUnixFromPc(uint32_t unix_time)
{
    if (unix_time == 0)
        return;
    _unixBase = unix_time;
    _millisBase = millis();
}
bool GetInfoAggregator::onPcGetInfoRequest(int id_des, uint32_t unix_time)
{
    if (!_pcStream)
        return false;

    _pcIdDes = id_des;
    _autoPushEnabled = true;

    // set base time để auto push có time tăng dần
    _unixBase = unix_time;
    _millisBase = millis();

    // trả ngay opcode 3 (tổng hợp)
    bool ok = sendAggregatedToPC(*_pcStream, _pcIdDes, unix_time);

    // sau khi trả, xem như sạch (chờ thay đổi tiếp)
    _lastPushMs = millis();
    return ok;
}

void GetInfoAggregator::update()
{
    if (!_pcStream)
        return;
    if (!_pushPending)
        return;

    if (!(_alwaysPush || _autoPushEnabled))
        return;

    uint32_t nowMs = millis();
    if (_minPushIntervalMs > 0 && (uint32_t)(nowMs - _lastPushMs) < _minPushIntervalMs)
        return;

    uint32_t t = nowUnix();
    if (sendAggregatedToPC(*_pcStream, _pcIdDes, t))
    {
        _pushPending = false;
        _lastPushMs = nowMs;
    }
}

bool GetInfoAggregator::ingestFromNodeJson(const char *json, const IPAddress &srcIp, uint16_t srcPort)
{
    if (!json || !json[0])
        return false;

    StaticJsonDocument<768> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    ingestFromNodeDoc(doc.as<JsonObjectConst>(), srcIp, srcPort);
    return true;
}

void GetInfoAggregator::ingestFromNodeDoc(JsonObjectConst root, const IPAddress &, uint16_t)
{
    int opcode = root["opcode"] | 0;
    if (opcode != 103) 
    {
        return;
    }
    JsonObjectConst dataObj = root["data"].as<JsonObjectConst>();
    if (dataObj.isNull())
        return;
    ingestDataObject(dataObj);
    _pushPending = true;
}

void GetInfoAggregator::ingestDataObject(JsonObjectConst dataObj)
{
    if (dataObj.containsKey("motor") && dataObj["motor"].is<JsonObjectConst>())
    {
        ingestSabanMotor(dataObj);
        return;
    }
    // ===== VOC: hỗ trợ cả schema cũ + schema mới =====
    if (dataObj.containsKey("sensor_id") || 
        dataObj.containsKey("nh3") || dataObj.containsKey("co") || dataObj.containsKey("no2") ||
        dataObj.containsKey("temp") || dataObj.containsKey("humi"))
    {
        ingestSensorVoc(dataObj);
        return;
    }
    if (dataObj.containsKey("out1") || dataObj.containsKey("in1"))
    {
        ingestIoController(dataObj);
        return;
    }
}

void GetInfoAggregator::ingestSabanMotor(JsonObjectConst dataObj)
{
    JsonObjectConst motor = dataObj["motor"].as<JsonObjectConst>();
    if (motor.isNull())
        return;

    bool changed = false;
    static const char *keys[5] = {"m1", "m2", "m3", "m4", "m5"};

    for (int i = 0; i < 5; i++)
    {
        JsonObjectConst m = motor[keys[i]].as<JsonObjectConst>();
        if (m.isNull())
            continue;

        uint8_t run = clampU8(m["run"] | (int)_saban.m[i].run, 0, 1);
        uint8_t dir = clampU8(m["dir"] | (int)_saban.m[i].dir, 0, 1);
        uint8_t speed = clampU8(m["speed"] | (int)_saban.m[i].speed, 0, 100);

        if (run != _saban.m[i].run || dir != _saban.m[i].dir || speed != _saban.m[i].speed)
            changed = true;

        _saban.m[i].run = run;
        _saban.m[i].dir = dir;
        _saban.m[i].speed = speed;
    }
    // ===== input (in1..in4) =====
    JsonObjectConst input = dataObj["input"].as<JsonObjectConst>();
    if (!input.isNull())
    {
        uint8_t inNew[4] = {
            clampU8(input["in1"] | (int)_saban.in[0], 0, 2),
            clampU8(input["in2"] | (int)_saban.in[1], 0, 2),
            clampU8(input["in3"] | (int)_saban.in[2], 0, 2),
            clampU8(input["in4"] | (int)_saban.in[3], 0, 2),
        };

        for (int k = 0; k < 4; k++)
        {
            if (inNew[k] != _saban.in[k])
                changed = true;
            _saban.in[k] = inNew[k];
        }
    }
    _saban.hasMotor = true;
    _saban.lastUpdateMs = millis();

    _pushPending = true;
}
bool GetInfoAggregator::ingestOneVoc(JsonObjectConst obj, SensorVocSnapshot &dst)
{
    bool changed = false;

    float temp = safeFloat(obj["temp"], dst.temp);
    float humi = safeFloat(obj["humi"], dst.humi);
    float nh3 = safeFloat(obj["nh3"], dst.nh3);
    float co = safeFloat(obj["co"], dst.co);
    float no2 = safeFloat(obj["no2"], dst.no2);

    if (floatChanged(temp, dst.temp))
        changed = true;
    if (floatChanged(humi, dst.humi))
        changed = true;
    if (floatChanged(nh3, dst.nh3))
        changed = true;
    if (floatChanged(co, dst.co))
        changed = true;
    if (floatChanged(no2, dst.no2))
        changed = true;

    dst.temp = temp;
    dst.humi = humi;
    dst.nh3 = nh3;
    dst.co = co;
    dst.no2 = no2;

    dst.has = true;
    dst.lastUpdateMs = millis();

    return changed;
}

void GetInfoAggregator::ingestSensorVoc(JsonObjectConst dataObj)
{
    int sid = dataObj["sensor_id"] | 1;
    if (sid < 1)
        sid = 1;
    if (sid > 2)
        sid = 2;

    int idx = sid - 1; // 1->0 (sensor_1), 2->1 (sensor_2)

    // ingest kiểu phẳng vào đúng sensor
    (void)ingestOneVoc(dataObj, _voc[idx]);

    _pushPending = true;
}

void GetInfoAggregator::ingestIoController(JsonObjectConst dataObj)
{
    bool changed = false;

    uint8_t o[4] = {
        clampU8(dataObj["out1"] | (int)_io.out[0], 0, 1),
        clampU8(dataObj["out2"] | (int)_io.out[1], 0, 1),
        clampU8(dataObj["out3"] | (int)_io.out[2], 0, 1),
        clampU8(dataObj["out4"] | (int)_io.out[3], 0, 1),
    };
    uint8_t i[4] = {
        clampU8(dataObj["in1"] | (int)_io.in[0], 0, 1),
        clampU8(dataObj["in2"] | (int)_io.in[1], 0, 1),
        clampU8(dataObj["in3"] | (int)_io.in[2], 0, 1),
        clampU8(dataObj["in4"] | (int)_io.in[3], 0, 1),
    };

    for (int k = 0; k < 4; k++)
    {
        if (o[k] != _io.out[k])
            changed = true;
        if (i[k] != _io.in[k])
            changed = true;
        _io.out[k] = o[k];
        _io.in[k] = i[k];
    }

    _io.has = true;
    _io.lastUpdateMs = millis();
    if (changed)
        _pushPending = true;
}

void GetInfoAggregator::updateIoController(uint8_t out1, uint8_t out2, uint8_t out3, uint8_t out4,
                                           uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
{
    bool changed = false;

    uint8_t o[4] = {clampU8(out1, 0, 2), clampU8(out2, 0, 2), clampU8(out3, 0, 2), clampU8(out4, 0, 2)};
    uint8_t i[4] = {clampU8(in1, 0, 2), clampU8(in2, 0, 2), clampU8(in3, 0, 2), clampU8(in4, 0, 2)};

    for (int k = 0; k < 4; k++)
    {
        if (o[k] != _io.out[k])
            changed = true;
        if (i[k] != _io.in[k])
            changed = true;
        _io.out[k] = o[k];
        _io.in[k] = i[k];
    }

    _io.has = true;
    _io.lastUpdateMs = millis();
}

// ---- build + send (giống bản trước) ----
bool GetInfoAggregator::buildResponseDoc(StaticJsonDocument<1024> &doc,
                                         int id_des, uint32_t unix_time,
                                         char *outDataJson, size_t outDataJsonCap)
{
    doc.clear();
    doc["id_des"] = id_des;
    doc["opcode"] = OPCODE_GET_INFO;

    JsonObject data = doc.createNestedObject("data");

    JsonObject saban = data.createNestedObject("saban");
    JsonObject motor = saban.createNestedObject("motor");
    static const char *keys[5] = {"m1", "m2", "m3", "m4", "m5"};
    for (int i = 0; i < 5; i++)
    {
        JsonObject m = motor.createNestedObject(keys[i]);
        m["run"] = _saban.m[i].run;
        m["dir"] = _saban.m[i].dir;
        m["speed"] = _saban.m[i].speed;
    }
    JsonObject sabanInput = saban.createNestedObject("input");
    sabanInput["in1"] = _saban.in[0];
    sabanInput["in2"] = _saban.in[1];
    sabanInput["in3"] = _saban.in[2];
    sabanInput["in4"] = _saban.in[3];
    JsonObject voc = data.createNestedObject("sensor_voc");

    JsonObject s1 = voc.createNestedObject("sensor_1");
    s1["temp"] = _voc[0].temp;
    s1["humi"] = _voc[0].humi;
    s1["nh3"] = _voc[0].nh3;
    s1["co"] = _voc[0].co;
    s1["no2"] = _voc[0].no2;

    JsonObject s2 = voc.createNestedObject("sensor_2");
    s2["temp"] = _voc[1].temp;
    s2["humi"] = _voc[1].humi;
    s2["nh3"] = _voc[1].nh3;
    s2["co"] = _voc[1].co;
    s2["no2"] = _voc[1].no2;

    JsonObject io = data.createNestedObject("io_controller");
    io["out1"] = _io.out[0];
    io["out2"] = _io.out[1];
    io["out3"] = _io.out[2];
    io["out4"] = _io.out[3];
    io["in1"] = _io.in[0];
    io["in2"] = _io.in[1];
    io["in3"] = _io.in[2];
    io["in4"] = _io.in[3];

    doc["time"] = unix_time;

    size_t n = serializeJson(doc["data"], outDataJson, outDataJsonCap);
    if (n == 0 || n >= outDataJsonCap)
        return false;

    String combined;
    combined.reserve(64 + n);
    combined += String(id_des);
    combined += String(OPCODE_GET_INFO);
    combined += outDataJson;
    combined += String(unix_time);
    combined += SECRET_KEY;

    doc["auth"] = calculateMD5(combined);
    return true;
}

bool GetInfoAggregator::sendAggregatedToPC(Stream &outStream, int id_des, uint32_t unix_time)
{
    StaticJsonDocument<1024> doc;
    char dataJson[700];

    if (!buildResponseDoc(doc, id_des, unix_time, dataJson, sizeof(dataJson)))
    {
        // gói lỗi tối thiểu
        StaticJsonDocument<256> err;
        err["id_des"] = id_des;
        err["opcode"] = OPCODE_GET_INFO;
        JsonObject d = err.createNestedObject("data");
        d["status"] = 255;
        err["time"] = unix_time;

        char dj[64];
        serializeJson(err["data"], dj, sizeof(dj));
        String combined = String(id_des) + String(OPCODE_GET_INFO) + String(dj) + String(unix_time) + SECRET_KEY;
        err["auth"] = calculateMD5(combined);

        serializeJson(err, outStream);
        outStream.println();
        return false;
    }
    char out[1400]; // nếu sau này bạn thêm saban.input thì tăng lên 1400
    size_t n = serializeJson(doc, out, sizeof(out));
    if (n == 0 || n >= sizeof(out))
    {
        return false; // buffer không đủ -> tăng out[]
        Serial.println("Buffer overlow!");
    }

    outStream.println(out);

    return true;
}
