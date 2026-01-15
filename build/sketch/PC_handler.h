#line 1 "D:\\phunsuong\\master\\master\\PC_handler.h"
#ifndef PC_HANDLER_H
#define PC_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "md5.h"
#include "config.h"
#include "MICS6814.h"

class PCHandler
{
public:
    using CommandCallback = void (*)(const MistCommand &cmd);

    PCHandler();
    void begin();
    void update();
    void onCommandReceived(CommandCallback cb);
    void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status);

private:
    String rxLine;
    StaticJsonDocument<512> doc;
    CommandCallback commandCallback = nullptr;
    // ===== Timeout packet  =====
    uint32_t lastRxMs = 0;
    uint32_t timeoutMs = 30000;
    bool inTimeout = false;
    int last_hast_state = false;
    void processLine();
    // void checkPacketTimeout();
};

#endif