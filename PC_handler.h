#ifndef PC_HANDLER_H
#define PC_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include "config.h"

class PCHandler
{
public:
    using CommandCallback = void (*)(const MistCommand &cmd);

    PCHandler();
    void begin();
    void update();
    void onCommandReceived(CommandCallback cb);
    void sendResponse(int id_des, int resp_opcode, uint32_t unix_time, int status);
    uint32_t getCurrentUnixTime();

private:
    String rxLine;
    StaticJsonDocument<256> doc;
    CommandCallback commandCallback = nullptr;

    void processLine();
};

#endif