#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\config_portal.cpp"
#include "config_portal.h"
#include <ArduinoJson.h>
#include <WiFi.h>

#include "central/central_controller.h"
#include "central/eeprom_state.h"

namespace
{
    const char *kApSsid = "CENTER-CONFIG";
    const char *kApPass = "altacenter";
    const IPAddress kApIp(192, 168, 4, 1);
    const IPAddress kApGw(192, 168, 4, 1);
    const IPAddress kApMask(255, 255, 255, 0);

    const uint32_t kLongPressMs = 5000;
    const uint32_t kStopDelayMs = 5000;
    const uint32_t kIdleTimeoutMs = 5UL * 60UL * 1000UL; //time out về trạng thái bình thường 5p

    ConfigPortal *gPortal = nullptr;

    const char kIndexHtml[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Ethernet Config</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; color: #111; }
    .card { max-width: 520px; padding: 16px; border: 1px solid #ddd; border-radius: 8px; }
    label { display: block; margin-top: 12px; font-weight: 600; }
    input { width: 100%; padding: 8px; font-size: 14px; }
    button { margin-top: 16px; padding: 10px 14px; font-size: 14px; }
    .status { margin-top: 12px; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Ethernet IPv4 Config</h2>
    <label>IP</label><input id="ip" placeholder="192.168.1.100"/>
    <label>Mask</label><input id="mask" placeholder="255.255.255.0"/>
    <label>Gateway</label><input id="gw" placeholder="192.168.1.1"/>
    <label>DNS 1</label><input id="dns1" placeholder="8.8.8.8"/>
    <label>DNS 2</label><input id="dns2" placeholder="1.1.1.1"/>
    <h3>Sensor Thresholds</h3>
    <label>Temp ON</label><input id="temp_on" placeholder="55"/>
    <label>Temp OFF</label><input id="temp_off" placeholder="45"/>
    <label>Humi ON</label><input id="humi_on" placeholder="85"/>
    <label>Humi OFF</label><input id="humi_off" placeholder="80"/>
    <label>NH3 ON</label><input id="nh3_on" placeholder="50"/>
    <label>NH3 OFF</label><input id="nh3_off" placeholder="45"/>
    <label>CO ON</label><input id="co_on" placeholder="50"/>
    <label>CO OFF</label><input id="co_off" placeholder="45"/>
    <label>NO2 ON</label><input id="no2_on" placeholder="5"/>
    <label>NO2 OFF</label><input id="no2_off" placeholder="4"/>
    <button onclick="save()">Apply</button>
    <div class="status" id="status"></div>
  </div>
  <script>
    let ws;
    function log(msg){ document.getElementById('status').textContent = msg; }
    const defaults = {
      temp_on: 55, temp_off: 45,
      humi_on: 85, humi_off: 80,
      nh3_on: 50, nh3_off: 45,
      co_on: 50, co_off: 45,
      no2_on: 5, no2_off: 4
    };
    function setVals(msg){
      if(!msg) return;
      const eth = msg.eth || {};
      ip.value = eth.ip || '';
      mask.value = eth.mask || '';
      gw.value = eth.gateway || '';
      dns1.value = eth.dns1 || '';
      dns2.value = eth.dns2 || '';
      const t = msg.thresholds || defaults;
      temp_on.value = (t.temp_on ?? defaults.temp_on);
      temp_off.value = (t.temp_off ?? defaults.temp_off);
      humi_on.value = (t.humi_on ?? defaults.humi_on);
      humi_off.value = (t.humi_off ?? defaults.humi_off);
      nh3_on.value = (t.nh3_on ?? defaults.nh3_on);
      nh3_off.value = (t.nh3_off ?? defaults.nh3_off);
      co_on.value = (t.co_on ?? defaults.co_on);
      co_off.value = (t.co_off ?? defaults.co_off);
      no2_on.value = (t.no2_on ?? defaults.no2_on);
      no2_off.value = (t.no2_off ?? defaults.no2_off);
    }
    function num(id, fallback){
      const v = parseFloat(document.getElementById(id).value);
      return Number.isNaN(v) ? fallback : v;
    }
    function connect(){
      ws = new WebSocket(`ws://${location.hostname}:81/`);
      ws.onopen = () => ws.send(JSON.stringify({type:"get"}));
      ws.onmessage = (e) => {
        try {
          const msg = JSON.parse(e.data);
          if(msg.type === "config") setVals(msg);
          if(msg.type === "ack") log(msg.message || (msg.ok ? "OK" : "Error"));
        } catch (err) { log("Parse error"); }
      };
      ws.onclose = () => setTimeout(connect, 1500);
    }
    function save(){
      const payload = {
        type: "set",
        eth: {
          ip: ip.value.trim(),
          mask: mask.value.trim(),
          gateway: gw.value.trim(),
          dns1: dns1.value.trim(),
          dns2: dns2.value.trim()
        },
        thresholds: {
          temp_on: num("temp_on", defaults.temp_on),
          temp_off: num("temp_off", defaults.temp_off),
          humi_on: num("humi_on", defaults.humi_on),
          humi_off: num("humi_off", defaults.humi_off),
          nh3_on: num("nh3_on", defaults.nh3_on),
          nh3_off: num("nh3_off", defaults.nh3_off),
          co_on: num("co_on", defaults.co_on),
          co_off: num("co_off", defaults.co_off),
          no2_on: num("no2_on", defaults.no2_on),
          no2_off: num("no2_off", defaults.no2_off)
        }
      };
      ws.send(JSON.stringify(payload));
    }
    connect();
  </script>
</body>
</html>
)html";

    bool parseIpField(JsonObject obj, const char *key, IPAddress &out)
    {
        const char *val = obj[key];
        if (!val || !*val)
            return false;
        return out.fromString(val);
    }

    void onWsEventStatic(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
    {
        if (gPortal)
            gPortal->handleWsEvent(num, type, payload, length);
    }

    bool updateThresholdsFromJson(JsonObject thresholds, Thresholds &t)
    {
        bool any = false;
        auto setIf = [&](const char *key, float &field)
        {
            if (thresholds.containsKey(key))
            {
                field = thresholds[key].as<float>();
                any = true;
            }
        };

        setIf("temp_on", t.temp_on);
        setIf("temp_off", t.temp_off);
        setIf("humi_on", t.humi_on);
        setIf("humi_off", t.humi_off);
        setIf("nh3_on", t.nh3_on);
        setIf("nh3_off", t.nh3_off);
        setIf("co_on", t.co_on);
        setIf("co_off", t.co_off);
        setIf("no2_on", t.no2_on);
        setIf("no2_off", t.no2_off);
        return any;
    }

    bool loadCurrentThresholds(Thresholds &t, CentralController *controller)
    {
        if (controller)
        {
            t = controller->getThresholds();
            return true;
        }
        return eepromThresholdsLoad(t);
    }
} // namespace

ConfigPortal::ConfigPortal()
    : server_(80), ws_(81)
{
}

void ConfigPortal::initPortal(uint8_t buttonPin, EthernetUDPHandler *ethHandler)
{
    gPortal = this; // con trỏ tới đối tượng khi gọi initPortal
    buttonPin_ = buttonPin;
    ethHandler_ = ethHandler;

    pinMode(buttonPin_, INPUT_PULLUP);

    server_.on("/", HTTP_GET, [this]()
               { server_.send_P(200, "text/html", kIndexHtml); });
    ws_.onEvent(onWsEventStatic);
}

void ConfigPortal::begin(uint8_t buttonPin, EthernetUDPHandler *ethHandler, LedStatus *ledStatus, CentralController *centralController)
{
    initPortal(buttonPin, ethHandler);
    centralController_ = centralController;
}

void ConfigPortal::update()
{
    handleButton();
    if (!active_)
        return;

    server_.handleClient();
    ws_.loop();

    if (pendingStop_ && (int32_t)(millis() - stopAtMs_) >= 0)
    {
        ws_.disconnect(stopClientId_);
        pendingStop_ = false;
        stopPortal();
        return;
    }

    if (kIdleTimeoutMs > 0 && (uint32_t)(millis() - lastActivityMs_) > kIdleTimeoutMs)
        stopPortal();
}

bool ConfigPortal::isActive() const
{
    return active_;
}

void ConfigPortal::startPortal()
{
    if (active_)
        return;

    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(kApIp, kApGw, kApMask);
    WiFi.softAP(kApSsid, kApPass);

    server_.begin();
    ws_.begin();

    lastActivityMs_ = millis();
    active_ = true;
    has_mode_config = false;
    has_mode_config_on = true;
    Serial.println(F("[CFG] Config mode ON"));
    Serial.print(F("[CFG] AP IP: "));
    Serial.println(WiFi.softAPIP());
}

void ConfigPortal::stopPortal()
{
    if (!active_)
        return;

    server_.stop();
    ws_.close();
    WiFi.mode(WIFI_OFF);   // tắt Wi-Fi stack
    delay(100);

    active_ = false;
    pendingStop_ = false;
    has_mode_config_on = false;
    has_mode_config = false;
    Serial.println(F("[CFG] Config mode OFF"));
}

void ConfigPortal::handleButton()
{
    if (buttonPin_ == 255)
        return;

    const bool isDown = (digitalRead(buttonPin_) == LOW);
    const uint32_t now = millis();

    if (isDown)
    {
        if (!buttonWasDown_)
        {
            buttonWasDown_ = true;
            pressHandled_ = false;
            pressStartMs_ = now;
            has_mode_config = true;
        }
        else if (!pressHandled_ && (uint32_t)(now - pressStartMs_) >= kLongPressMs)
        {
            pressHandled_ = true;
            if (active_)
                stopPortal();
            else
                startPortal();
        }
    }
    else
    {
        buttonWasDown_ = false;
        has_mode_config = false;
    }
}

void ConfigPortal::handleWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    lastActivityMs_ = millis();
    if (type == WStype_CONNECTED)
    {
        sendConfig(num);
        return;
    }
    if (type == WStype_TEXT)
    {
        handleWsText(num, payload, length);
        return;
    }
}

void ConfigPortal::handleWsText(uint8_t num, const uint8_t *payload, size_t length)
{
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err)
    {
        sendAck(num, false, "Invalid JSON");
        return;
    }

    const char *type = doc["type"];
    if (!type)
    {
        sendAck(num, false, "Missing type");
        return;
    }

    if (strcmp(type, "get") == 0)
    {
        sendConfig(num);
        return;
    }

    if (strcmp(type, "set") == 0)
    {
        bool hasThresholds = false;
        bool thresholdsApplied = false;
        JsonObject thresholds = doc["thresholds"].as<JsonObject>();
        if (!thresholds.isNull())
        {
            hasThresholds = true;
            Thresholds nextThresholds{};
            if (!loadCurrentThresholds(nextThresholds, centralController_))
            {
                sendAck(num, false, "Thresholds unavailable");
                return;
            }
            if (updateThresholdsFromJson(thresholds, nextThresholds))
            {
                if (centralController_)
                    centralController_->setThresholds(nextThresholds, true);
                else
                    eepromThresholdsSave(nextThresholds);
                thresholdsApplied = true;
            }
            String raw((const char *)payload, length);
            Serial.println(raw);
            hasThresholds = true; // chuỗi in ngưỡng để so sánh
        }

        JsonObject eth = doc["eth"];
        if (eth.isNull())
        {
            if (hasThresholds && thresholdsApplied)
            {
                sendAck(num, true, "Applied");
                sendConfig(num);
                return;
            }
            sendAck(num, false, "Missing eth");
            return;
        }

        EthStaticConfig cfg;
        loadEthStaticConfig(cfg);
        if (!parseIpField(eth, "ip", cfg.ip) ||
            !parseIpField(eth, "mask", cfg.mask) ||
            !parseIpField(eth, "gateway", cfg.gateway) ||
            !parseIpField(eth, "dns1", cfg.dns1))
        {
            sendAck(num, false, "Invalid IP");
            return;
        }

        parseIpField(eth, "dns2", cfg.dns2);

        if (!saveEthStaticConfig(cfg))
        {
            sendAck(num, false, "Save failed");
            return;
        }

        const bool applied = ethHandler_ ? ethHandler_->applyStaticConfig(cfg) : false;
        if (hasThresholds && thresholdsApplied && !applied)
            sendAck(num, false, "Thresholds saved; ETH apply failed");
        else
            sendAck(num, applied, applied ? "Applied" : "Apply failed");
        sendConfig(num);
        if (applied)
        {
            pendingStop_ = true;
            stopAtMs_ = millis() + kStopDelayMs;
            stopClientId_ = num;
        }
        return;
    }

    sendAck(num, false, "Unknown type");
}

void ConfigPortal::sendConfig(uint8_t num)
{
    EthStaticConfig cfg;
    loadEthStaticConfig(cfg);

    StaticJsonDocument<384> doc;
    doc["type"] = "config";
    JsonObject eth = doc.createNestedObject("eth");
    eth["ip"] = cfg.ip.toString();
    eth["mask"] = cfg.mask.toString();
    eth["gateway"] = cfg.gateway.toString();
    eth["dns1"] = cfg.dns1.toString();
    eth["dns2"] = cfg.dns2.toString();

    Thresholds t{};
    if (loadCurrentThresholds(t, centralController_))
    {
        JsonObject thresholds = doc.createNestedObject("thresholds");
        thresholds["temp_on"] = t.temp_on;
        thresholds["temp_off"] = t.temp_off;
        thresholds["humi_on"] = t.humi_on;
        thresholds["humi_off"] = t.humi_off;
        thresholds["nh3_on"] = t.nh3_on;
        thresholds["nh3_off"] = t.nh3_off;
        thresholds["co_on"] = t.co_on;
        thresholds["co_off"] = t.co_off;
        thresholds["no2_on"] = t.no2_on;
        thresholds["no2_off"] = t.no2_off;
    }

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(num, out);
}

void ConfigPortal::sendAck(uint8_t num, bool ok, const char *message)
{
    StaticJsonDocument<128> doc;
    doc["type"] = "ack";
    doc["ok"] = ok;
    doc["message"] = message;

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(num, out);
}
