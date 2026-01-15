#line 1 "D:\\phunsuong\\master\\master\\rs485_handler.cpp"
#include "rs485_handler.h"
#include <ArduinoJson.h>
#include "md5.h"
#include "config.h"
static const uint16_t TX_BUF_SZ = 512;

Rs485Handler::Rs485Handler(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baudRate)
    : rs485(serial), rxPin(rxPin), txPin(txPin), baudRate(baudRate)
{
    memset(rxBuf, 0, sizeof(rxBuf));
}

void Rs485Handler::begin()
{
    // Start RS485 serial
    rs485.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

// Send command to nodes via RS485 - similar protocol to Ethernet
bool Rs485Handler::sendCommand(const MistCommand &cmd)
{
    // Create data JSON
    StaticJsonDocument<256> dataDoc;
    if (cmd.opcode == 1)
    {
        dataDoc["node_id"] = cmd.node_id;
        dataDoc["time_phase1"] = cmd.time_phase1;
        dataDoc["time_phase2"] = cmd.time_phase2;
    }
    else
    {
        return false;
    }
    // Stringify data for auth
    String dataJson;
    dataJson.reserve(256);
    serializeJson(dataDoc, dataJson);

    // Auth: id_des + opcode + dataJson + time + SECRET_KEY
    String combined;
    combined.reserve(64 + dataJson.length());
    combined = String(cmd.id_des) + String(cmd.opcode) + dataJson + String(cmd.unix) + SECRET_KEY;

    String auth = calculateMD5(combined); // Assuming this function exists (from your project)

    // Full JSON document
    StaticJsonDocument<512> doc;
    doc["id_des"] = cmd.id_des;
    doc["opcode"] = cmd.opcode;
    doc["data"] = dataDoc;
    doc["time"] = cmd.unix;
    doc["auth"] = auth;

    // Serialize to buffer
    char msg[TX_BUF_SZ];
    size_t n = serializeJson(doc, msg, sizeof(msg));
    if (n == 0 || n >= sizeof(msg))
        return false;

    rs485.println(msg);
    return true;
}