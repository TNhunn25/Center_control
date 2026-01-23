#include <Arduino.h>
#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\master.ino"
#include "PC_handler.h"
#include "ethernet_handler.h"
#include "rs485_handler.h"
#include "led_status.h"
#include "get_info.h"
#include "config.h"
#include "config_portal.h"

#include "central/pcf8575_io.h"
#define CENTRAL_CONTROLLER_IMPLEMENTATION
#include "central/central_controller.h"
PCF8575IO pcf;
PCHandler pcHandler;
EthernetUDPHandler ethHandler;
LedStatus led_status;
CentralController centralController;
GetInfoAggregator getInfo;
ConfigPortal configPortal;

// Rs485Handler rs485(Serial1, RS485_RX_PIN, RS485_TX_PIN, 115200);

#line 22 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\master.ino"
void onNewCommand(const MistCommand &cmd);
#line 44 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\master.ino"
void setup();
#line 58 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\master.ino"
void loop();
#line 22 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\master.ino"
void onNewCommand(const MistCommand &cmd) // Dispatch lệnh từ PC tới các module
{
    getInfo.syncUnixFromPc(cmd.unix);
    if (cmd.opcode == 2)
        centralController.handleCommand(cmd);
    if (cmd.opcode == 3)
    {
        // Ingest local IO status before replying 103 to PC.
        centralController.handleCommand(cmd);
        getInfo.onPcGetInfoRequest(cmd.id_des, cmd.unix);
        ethHandler.sendCommand(cmd);
        return;
    }
    if (cmd.opcode == 6)
    {
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
    configPortal.begin(CONFIG_BUTTON_PIN, &ethHandler, &led_status); // định nghĩa button
}
void loop()
{
    pcHandler.update();
    ethHandler.update();
    ethHandler.checkNodeTimeouts();
    led_status.update();
    centralController.update();
    getInfo.update();
    configPortal.update();
}

