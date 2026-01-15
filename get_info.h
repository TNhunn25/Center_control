#ifndef GET_INFO_H
#define GET_INFO_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Ethernet.h>
#include "md5.h"
#include "config.h"

struct MotorState
{
    uint8_t run = 0, dir = 0, speed = 0;
};
struct SabanSnapshot
{
    MotorState m[5];
    uint8_t in[4] = {0, 0, 0, 0};
    bool hasMotor = false;
    uint32_t lastUpdateMs = 0;
};

struct SensorVocSnapshot
{
    float temp = 0, humi = 0, nh3 = 0, co = 0, no2 = 0;
    bool has = false;
    uint32_t lastUpdateMs = 0;
};

struct IoControllerSnapshot
{
    uint8_t out[4] = {0, 0, 0, 0};
    uint8_t in[4] = {0, 0, 0, 0};
    bool has = false;
    uint32_t lastUpdateMs = 0;
};

class GetInfoAggregator
{
public:
    static constexpr int OPCODE_GET_INFO = 103;

    // Gắn output PC (Serial)
    void attachPcStream(Stream &s) { _pcStream = &s; }

    // PC gửi opcode 3 -> trả ngay + bật auto push
    bool onPcGetInfoRequest(int id_des, uint32_t unix_time);

    // gọi mỗi loop() để auto push khi dirty
    void update();

    // ingest từ packet node
    void ingestFromNodeDoc(JsonObjectConst root, const IPAddress &srcIp, uint16_t srcPort);
    bool ingestFromNodeJson(const char *json, const IPAddress &srcIp, uint16_t srcPort);

    // update IO local (tuỳ chọn)
    void updateIoController(uint8_t out1, uint8_t out2, uint8_t out3, uint8_t out4,
                            uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4);

    void setMinPushIntervalMs(uint32_t ms) { _minPushIntervalMs = ms; }
    void setAlwaysPush(bool en) { _alwaysPush = en; }
    void syncUnixFromPc(uint32_t unix_time);

private:
    void ingestDataObject(JsonObjectConst dataObj);
    void ingestSabanMotor(JsonObjectConst dataObj);
    void ingestSensorVoc(JsonObjectConst dataObj);
    bool ingestOneVoc(JsonObjectConst obj, SensorVocSnapshot &dst); // helper mới
    void ingestIoController(JsonObjectConst dataObj);

    bool sendAggregatedToPC(Stream &outStream, int id_des, uint32_t unix_time);

    bool buildResponseDoc(StaticJsonDocument<1024> &doc,
                          int id_des, uint32_t unix_time,
                          char *outDataJson, size_t outDataJsonCap);

    // unix “ước lượng” dựa trên lần PC request cuối
    uint32_t nowUnix() const;
    bool _pushPending = false; // có event trigger cần push

    SabanSnapshot _saban{};
    SensorVocSnapshot _voc[2];
    IoControllerSnapshot _io{};

    // --- auto push ---
    Stream *_pcStream = nullptr;
    bool _autoPushEnabled = false;
    uint32_t _minPushIntervalMs = 200;
    uint32_t _lastPushMs = 0;
    bool _alwaysPush = true;
    int _pcIdDes = 1;
    uint32_t _unixBase = 0;
    uint32_t _millisBase = 0;
};
#endif