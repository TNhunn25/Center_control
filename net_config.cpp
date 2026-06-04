#include "net_config.h"
#include "config.h"
#include <cstring>
#include <Preferences.h>

namespace
{
const char *kPortalAuthUserKey = "p_user";
const char *kPortalAuthPassKey = "p_pass";
const char *kLegacyPortalAuthUserKey = "web_user";
const char *kLegacyPortalAuthPassKey = "web_pass";
const char *kPrefsNs = "netcfg";
const char *kValidKey = "valid";
const char *kIpKey = "ip";
const char *kMaskKey = "mask";
const char *kGwKey = "gw";
const char *kDns1Key = "dns1";
const char *kDns2Key = "dns2";
const char *kMqttHostKey = "m_host";
const char *kMqttPortKey = "m_port";
const char *kMqttDeviceIdKey = "m_devid";
const char *kMqttClientIdKey = "m_cid";
const char *kMqttUserKey = "m_user";
const char *kMqttPassKey = "m_pass";
const char *kMqttStatusTopicKey = "m_stopic";
const char *kMqttCmdTopicKey = "m_topic";
const char *kWiFiSsidKey = "w_ssid";
const char *kWiFiPassKey = "w_pass";
const char *kDeviceOutputCountKey = "dev_out";
const char *kTotpEnabledKey = "totp_en";
const char *kTotpSecretKey = "totp_sec";
const char *kLegacyMqttStatusTopic = "POWER_CTRL/status";
const char *kLegacyMqttStatusTopicUpper = "POWER_CTRL/STATUS";
const char *kLegacyMqttCmdTopic = "POWER_CTRL/command";
const char *kPlaceholderMqttStatusTopic = "POWER_CTRL/{UUID}/status";
const char *kPlaceholderMqttStatusTopicUpper = "POWER_CTRL/{UUID}/STATUS";
const char *kPlaceholderMqttCmdTopic = "POWER_CTRL/{UUID}/command";

bool isDefaultIdPlaceholder(const String &value, const char *defaultPrefix)
{
    return value.length() == 0 ||
           value == defaultPrefix;
}

bool isAllDigits(const String &value)
{
    if (value.length() == 0)
        return false;

    for (size_t i = 0; i < value.length(); i++)
    {
        if (!isDigit(value[i]))
            return false;
    }
    return true;
}

bool isOldGeneratedId(const String &value, const char *defaultPrefix)
{
    const String prefix(defaultPrefix);
    if (!value.startsWith(prefix))
        return false;

    const String suffix = value.substring(prefix.length());
    return suffix.length() != 5 && isAllDigits(suffix);
}

bool isOldGeneratedTopic(const String &value, const char *suffix)
{
    const String prefix("POWER_CTRL/");
    const String end(suffix);
    if (!value.startsWith(prefix) || !value.endsWith(end))
        return false;

    const int uuidStart = prefix.length();
    const int uuidLen = (int)value.length() - uuidStart - (int)end.length();
    if (uuidLen <= 0 || uuidLen == 5)
        return false;

    return isAllDigits(value.substring(uuidStart, uuidStart + uuidLen));
}

uint32_t ipToU32(const IPAddress &ip)
{
    // Chuyển IPAddress thành số 32-bit để lưu xuống Preferences.
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

IPAddress u32ToIp(uint32_t v)
{
    // Khôi phục IPAddress từ giá trị 32-bit đọc trong Preferences.
    return IPAddress((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
}

bool isNonZeroIp(const IPAddress &ip)
{
    // Kiểm tra IP có phải 0.0.0.0 hay không.
    return (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
}

void copyStringField(char *dest, size_t destSize, const String &value)
{
    // Sao chép String sang buffer C-string và luôn chèn ký tự kết thúc chuỗi.
    if (destSize == 0)
        return;

    strncpy(dest, value.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
}
} // namespace

void buildShortDeviceCode(char *out, size_t outSize)
{
    if (!out || outSize == 0)
        return;

    snprintf(out, outSize, "%05lu", (unsigned long)(ESP.getEfuseMac() % 100000ULL));
}

EthStaticConfig defaultEthStaticConfig()
{
    EthStaticConfig cfg;
    cfg.ip = ETH_IP;
    cfg.mask = ETH_SUBNET;
    cfg.gateway = ETH_GATEWAY;
    cfg.dns1 = ETH_DNS;
    cfg.dns2 = ETH_DNS;
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

MqttConfig defaultMqttConfig()
{
    MqttConfig cfg{};
    char uuid[6] = {};
    buildShortDeviceCode(uuid, sizeof(uuid));
    copyStringField(cfg.host, sizeof(cfg.host), MQTT_SERVER);
    cfg.port = MQTT_PORT;
    snprintf(cfg.deviceId, sizeof(cfg.deviceId), "%s%s", MQTT_DEVICE_ID, uuid);
    snprintf(cfg.clientId, sizeof(cfg.clientId), "%s%s", MQTT_CLIENT_ID, uuid);
    copyStringField(cfg.username, sizeof(cfg.username), MQTT_USER);
    copyStringField(cfg.password, sizeof(cfg.password), MQTT_PASSWORD);
    snprintf(cfg.statusTopic, sizeof(cfg.statusTopic), MQTT_STATUS_TOPIC, uuid);
    snprintf(cfg.commandTopic, sizeof(cfg.commandTopic), MQTT_COMMAND_TOPIC, uuid);
    return cfg;
}

bool isValidMqttConfig(const MqttConfig &cfg)
{
    return cfg.host[0] != '\0' &&
           cfg.deviceId[0] != '\0' &&
           cfg.clientId[0] != '\0' &&
           cfg.statusTopic[0] != '\0' &&
           cfg.commandTopic[0] != '\0' &&
           cfg.port > 0;
}

bool loadMqttConfig(MqttConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultMqttConfig();
        return false;
    }

    MqttConfig def = defaultMqttConfig();
    copyStringField(out.host, sizeof(out.host), prefs.getString(kMqttHostKey, def.host));
    out.port = prefs.getUShort(kMqttPortKey, def.port);

    String storedDeviceId = prefs.getString(kMqttDeviceIdKey, "");
    if (isDefaultIdPlaceholder(storedDeviceId, MQTT_DEVICE_ID) ||
        isOldGeneratedId(storedDeviceId, MQTT_DEVICE_ID))
        copyStringField(out.deviceId, sizeof(out.deviceId), def.deviceId);
    else
        copyStringField(out.deviceId, sizeof(out.deviceId), storedDeviceId);

    String storedClientId = prefs.getString(kMqttClientIdKey, "");
    if (isDefaultIdPlaceholder(storedClientId, MQTT_CLIENT_ID) ||
        isOldGeneratedId(storedClientId, MQTT_CLIENT_ID))
        copyStringField(out.clientId, sizeof(out.clientId), out.deviceId);
    else
        copyStringField(out.clientId, sizeof(out.clientId), storedClientId);
    copyStringField(out.username, sizeof(out.username), prefs.getString(kMqttUserKey, def.username));
    copyStringField(out.password, sizeof(out.password), prefs.getString(kMqttPassKey, def.password));
    String storedStatusTopic = prefs.getString(kMqttStatusTopicKey, "");
    if (storedStatusTopic.length() == 0 ||
        storedStatusTopic == kLegacyMqttStatusTopic ||
        storedStatusTopic == kLegacyMqttStatusTopicUpper ||
        storedStatusTopic == kPlaceholderMqttStatusTopic ||
        storedStatusTopic == kPlaceholderMqttStatusTopicUpper ||
        storedStatusTopic == MQTT_STATUS_TOPIC ||
        isOldGeneratedTopic(storedStatusTopic, "/status") ||
        isOldGeneratedTopic(storedStatusTopic, "/STATUS"))
        copyStringField(out.statusTopic, sizeof(out.statusTopic), def.statusTopic);
    else
        copyStringField(out.statusTopic, sizeof(out.statusTopic), storedStatusTopic);
    String storedCommandTopic = prefs.getString(kMqttCmdTopicKey, "");
    if (storedCommandTopic.length() == 0 ||
        storedCommandTopic == kLegacyMqttCmdTopic ||
        storedCommandTopic == kPlaceholderMqttCmdTopic ||
        storedCommandTopic == MQTT_COMMAND_TOPIC ||
        isOldGeneratedTopic(storedCommandTopic, "/command"))
        copyStringField(out.commandTopic, sizeof(out.commandTopic), def.commandTopic);
    else
        copyStringField(out.commandTopic, sizeof(out.commandTopic), storedCommandTopic);
    prefs.end();

    if (!isValidMqttConfig(out))
    {
        out = defaultMqttConfig();
        return false;
    }

    return true;
}

bool saveMqttConfig(const MqttConfig &cfg)
{
    if (!isValidMqttConfig(cfg))
        return false;

    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putString(kMqttHostKey, cfg.host);
    prefs.putUShort(kMqttPortKey, cfg.port);
    prefs.putString(kMqttDeviceIdKey, cfg.deviceId);
    prefs.putString(kMqttClientIdKey, cfg.clientId);
    prefs.putString(kMqttUserKey, cfg.username);
    prefs.putString(kMqttPassKey, cfg.password);
    prefs.putString(kMqttStatusTopicKey, cfg.statusTopic);
    prefs.putString(kMqttCmdTopicKey, cfg.commandTopic);
    prefs.end();
    return true;
}

WiFiConfig defaultWiFiConfig()
{
    WiFiConfig cfg{};
    copyStringField(cfg.ssid, sizeof(cfg.ssid), "Tnhung");
    copyStringField(cfg.password, sizeof(cfg.password), "12345678");
    return cfg;
}

bool isValidWiFiConfig(const WiFiConfig &cfg)
{
    return cfg.ssid[0] != '\0';
}

bool loadWiFiConfig(WiFiConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultWiFiConfig();
        return false;
    }

    WiFiConfig def = defaultWiFiConfig();
    copyStringField(out.ssid, sizeof(out.ssid), prefs.getString(kWiFiSsidKey, def.ssid));
    copyStringField(out.password, sizeof(out.password), prefs.getString(kWiFiPassKey, def.password));
    prefs.end();

    if (!isValidWiFiConfig(out))
    {
        out = def;
        return false;
    }

    return true;
}

bool saveWiFiConfig(const WiFiConfig &cfg)
{
    if (!isValidWiFiConfig(cfg))
        return false;

    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putString(kWiFiSsidKey, cfg.ssid);
    prefs.putString(kWiFiPassKey, cfg.password);
    prefs.end();
    return true;
}

DeviceConfig defaultDeviceConfig()
{
    DeviceConfig cfg{};
    cfg.outputCount = 4;
    return cfg;
}

bool loadDeviceConfig(DeviceConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultDeviceConfig();
        return false;
    }

    out.outputCount = prefs.getUChar(kDeviceOutputCountKey, defaultDeviceConfig().outputCount);
    prefs.end();

    if (out.outputCount < 4 || out.outputCount > 8)
    {
        out = defaultDeviceConfig();
        return false;
    }

    return true;
}

bool saveDeviceConfig(const DeviceConfig &cfg)
{
    if (cfg.outputCount < 4 || cfg.outputCount > 8)
        return false;

    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putUChar(kDeviceOutputCountKey, cfg.outputCount);
    prefs.end();
    return true;
}

TotpConfig defaultTotpConfig()
{
    TotpConfig cfg{};
    cfg.enabled = false;
    cfg.secret[0] = '\0';
    return cfg;
}

bool loadTotpConfig(TotpConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultTotpConfig();
        return false;
    }

    out.enabled = prefs.getBool(kTotpEnabledKey, false);
    copyStringField(out.secret, sizeof(out.secret), prefs.getString(kTotpSecretKey, ""));
    prefs.end();

    if (out.secret[0] == '\0')
        out.enabled = false;

    return true;
}

bool saveTotpConfig(const TotpConfig &cfg)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putBool(kTotpEnabledKey, cfg.enabled && cfg.secret[0] != '\0');
    prefs.putString(kTotpSecretKey, cfg.secret);
    prefs.end();
    return true;
}

//mới thêm
PortalAuthConfig defaultPortalAuthConfig()
{
    PortalAuthConfig cfg{};
    copyStringField(cfg.username, sizeof(cfg.username), PORTAL_AUTH_USER);
    copyStringField(cfg.password, sizeof(cfg.password), PORTAL_AUTH_PASS);
    return cfg;
}

bool isValidPortalAuthConfig(const PortalAuthConfig &cfg)
{
    return cfg.username[0] != '\0' && cfg.password[0] != '\0';
}

bool loadPortalAuthConfig(PortalAuthConfig &out)
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNs, true))
    {
        out = defaultPortalAuthConfig();
        return false;
    }

    PortalAuthConfig def = defaultPortalAuthConfig();

    const bool hasPortalAuth = prefs.isKey(kPortalAuthUserKey) && prefs.isKey(kPortalAuthPassKey);
    const bool hasLegacyPortalAuth = prefs.isKey(kLegacyPortalAuthUserKey) && prefs.isKey(kLegacyPortalAuthPassKey);
    bool loadedStoredAuth = false;

    if (hasPortalAuth)
    {
        PortalAuthConfig stored{};
        copyStringField(stored.username, sizeof(stored.username), prefs.getString(kPortalAuthUserKey, ""));
        copyStringField(stored.password, sizeof(stored.password), prefs.getString(kPortalAuthPassKey, ""));
        if (isValidPortalAuthConfig(stored))
        {
            out = stored;
            loadedStoredAuth = true;
        }
    }

    if (!loadedStoredAuth && hasLegacyPortalAuth)
    {
        PortalAuthConfig stored{};
        copyStringField(stored.username, sizeof(stored.username), prefs.getString(kLegacyPortalAuthUserKey, ""));
        copyStringField(stored.password, sizeof(stored.password), prefs.getString(kLegacyPortalAuthPassKey, ""));
        if (isValidPortalAuthConfig(stored))
        {
            out = stored;
            loadedStoredAuth = true;
        }
    }

    if (!loadedStoredAuth)
    {
        out = def;
    }

    prefs.end();

    if (!isValidPortalAuthConfig(out))
    {
        out = def;
        return false;
    }

    return true;
}

bool savePortalAuthConfig(const PortalAuthConfig &cfg)
{
    if (!isValidPortalAuthConfig(cfg))
        return false;

    Preferences prefs;
    if (!prefs.begin(kPrefsNs, false))
        return false;

    prefs.putString(kPortalAuthUserKey, cfg.username);
    prefs.putString(kPortalAuthPassKey, cfg.password);
    prefs.putString(kLegacyPortalAuthUserKey, cfg.username);
    prefs.putString(kLegacyPortalAuthPassKey, cfg.password);

    prefs.end();
    return true;
}
