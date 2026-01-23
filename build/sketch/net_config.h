#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\net_config.h"
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct EthStaticConfig
{
    IPAddress ip;
    IPAddress mask;
    IPAddress gateway;
    IPAddress dns1;
    IPAddress dns2;
};

EthStaticConfig defaultEthStaticConfig();
bool loadEthStaticConfig(EthStaticConfig &out);
bool saveEthStaticConfig(const EthStaticConfig &cfg);
bool isValidEthStaticConfig(const EthStaticConfig &cfg);
