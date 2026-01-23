#include "net_config.h"
#include <Preferences.h>

namespace
{
const char *kPrefsNs = "netcfg";
const char *kValidKey = "valid";
const char *kIpKey = "ip";
const char *kMaskKey = "mask";
const char *kGwKey = "gw";
const char *kDns1Key = "dns1";
const char *kDns2Key = "dns2";

uint32_t ipToU32(const IPAddress &ip)
{
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

IPAddress u32ToIp(uint32_t v)
{
    return IPAddress((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
}

bool isNonZeroIp(const IPAddress &ip)
{
    return (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
}
} // namespace

EthStaticConfig defaultEthStaticConfig()
{
    EthStaticConfig cfg;
    cfg.ip = IPAddress(192, 168, 1, 100);
    cfg.mask = IPAddress(255, 255, 255, 0);
    cfg.gateway = IPAddress(192, 168, 1, 1);
    cfg.dns1 = IPAddress(8, 8, 8, 8);
    cfg.dns2 = IPAddress(1, 1, 1, 1);
    return cfg;
}

bool isValidEthStaticConfig(const EthStaticConfig &cfg)
{
    if (!isNonZeroIp(cfg.ip) || !isNonZeroIp(cfg.mask))
        return false;
    if (!isNonZeroIp(cfg.gateway))
        return false;
    return true;
}

bool loadEthStaticConfig(EthStaticConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultEthStaticConfig();
        return false;
    }

    const bool valid = prefs.getBool(kValidKey, false);
    if (!valid)
    {
        prefs.end();
        out = defaultEthStaticConfig();
        return false;
    }

    out.ip = u32ToIp(prefs.getUInt(kIpKey, ipToU32(defaultEthStaticConfig().ip)));
    out.mask = u32ToIp(prefs.getUInt(kMaskKey, ipToU32(defaultEthStaticConfig().mask)));
    out.gateway = u32ToIp(prefs.getUInt(kGwKey, ipToU32(defaultEthStaticConfig().gateway)));
    out.dns1 = u32ToIp(prefs.getUInt(kDns1Key, ipToU32(defaultEthStaticConfig().dns1)));
    out.dns2 = u32ToIp(prefs.getUInt(kDns2Key, ipToU32(defaultEthStaticConfig().dns2)));
    prefs.end();

    if (!isValidEthStaticConfig(out))
    {
        out = defaultEthStaticConfig();
        return false;
    }
    return true;
}

bool saveEthStaticConfig(const EthStaticConfig &cfg)
{
    if (!isValidEthStaticConfig(cfg))
        return false;

    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putBool(kValidKey, true);
    prefs.putUInt(kIpKey, ipToU32(cfg.ip));
    prefs.putUInt(kMaskKey, ipToU32(cfg.mask));
    prefs.putUInt(kGwKey, ipToU32(cfg.gateway));
    prefs.putUInt(kDns1Key, ipToU32(cfg.dns1));
    prefs.putUInt(kDns2Key, ipToU32(cfg.dns2));
    prefs.end();
    return true;
}
