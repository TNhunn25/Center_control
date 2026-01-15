#include <Arduino.h>
#line 1 "D:\\phunsuong\\master\\master\\master.ino"
#include "PC_handler.h"
#include "ethernet_handler.h"
#include "rs485_handler.h"
#include "led_status.h"
#include "get_info.h"
#include "config.h"
#define CENTRAL_CONTROLLER_IMPLEMENTATION
#include "central/central_controller.h"
PCHandler pcHandler;
EthernetUDPHandler ethHandler;
LedStatus led_status;
CentralController centralController;
GetInfoAggregator getInfo;
// Rs485Handler rs485(Serial1, RS485_RX_PIN, RS485_TX_PIN, 115200);
#line 15 "D:\\phunsuong\\master\\master\\master.ino"
void onNewCommand(const MistCommand &cmd);
#line 31 "D:\\phunsuong\\master\\master\\master.ino"
void setup();
#line 44 "D:\\phunsuong\\master\\master\\master.ino"
void loop();
#line 15 "D:\\phunsuong\\master\\master\\master.ino"
void onNewCommand(const MistCommand &cmd)
{
    getInfo.syncUnixFromPc(cmd.unix);
    if (cmd.opcode == 2)
        centralController.handleCommand(cmd);
    if (cmd.opcode == 3)
    {
        getInfo.onPcGetInfoRequest(cmd.id_des, cmd.unix);
        ethHandler.sendCommand(cmd);
        centralController.handleCommand(cmd);
        return;
    }
    ethHandler.sendCommand(cmd);
    // rs485.sendCommand(cmd);
}

void setup()
{
    pcHandler.begin();
    ethHandler.begin();
    led_status.begin(33, true);
    centralController.begin();
    getInfo.attachPcStream(Serial);
    getInfo.setAlwaysPush(true);
    getInfo.setMinPushIntervalMs(500);
    led_status.setState(LedStatus ::STATE_NORMAL);
    pcHandler.onCommandReceived(onNewCommand);
    centralController.getInforCommand(getInfo);
}
void loop()
{
    pcHandler.update();
    ethHandler.update();
    ethHandler.checkNodeTimeouts();
    led_status.update();
    centralController.update();
    getInfo.update();
}

