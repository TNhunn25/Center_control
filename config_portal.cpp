#include "config_portal.h"

#include <ArduinoJson.h>
#include <cstring>
#include <esp_system.h>
#include <Update.h>
#include <WiFi.h>

#include "Hardware_esp/central_controller.h"
#include "Hardware_esp/eeprom_state.h"
#include "config.h"

namespace
{
    const char *kApSsid = "CENTER-CONFIG";
    const char *kApPass = "altacenter";
    const IPAddress kApIp(192, 168, 4, 1);
    const IPAddress kApGw(192, 168, 4, 1);
    const IPAddress kApMask(255, 255, 255, 0);

    const uint32_t kLongPressMs = 5000;
    const uint32_t kStopDelayMs = 5000;
    const uint32_t kIdleTimeoutMs = 60UL * 1000UL;

    ConfigPortal *gPortal = nullptr;

    const char kLoginHtml[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Login</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #dbeafe, #f8fafc);
      color: #111827;
    }

    .login-card {
      width: 100%;
      max-width: 360px;
      background: white;
      border-radius: 18px;
      padding: 24px;
      box-shadow: 0 12px 32px rgba(15, 23, 42, 0.18);
    }

    h2 {
      margin-top: 0;
      margin-bottom: 18px;
      text-align: center;
      color: #2563eb;
    }

    label {
      display: block;
      margin-top: 12px;
      margin-bottom: 6px;
      font-weight: 700;
      font-size: 14px;
    }

    input {
      width: 100%;
      box-sizing: border-box;
      padding: 12px;
      border: 1px solid #d1d5db;
      border-radius: 10px;
      font-size: 15px;
      outline: none;
    }

    input:focus {
      border-color: #2563eb;
      box-shadow: 0 0 0 3px rgba(37,99,235,0.18);
    }

    button {
      width: 100%;
      margin-top: 18px;
      padding: 12px;
      border: none;
      border-radius: 12px;
      background: #2563eb;
      color: white;
      font-size: 15px;
      font-weight: 700;
      cursor: pointer;
    }

    .forgot {
      display: block;
      margin-top: 14px;
      text-align: center;
      color: #f97316;
      font-weight: 700;
      text-decoration: none;
    }

    .err {
      color: #dc2626;
      text-align: center;
      font-size: 14px;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="login-card">
    <h2>Power Central Login</h2>

    <form method="POST" action="/login">
      <label>Username</label>
      <input name="user" required autocomplete="username"/>

      <label>Password</label>
      <input name="pass" type="password" required autocomplete="current-password"/>

      <button type="submit">Sign In</button>
    </form>

    <a class="forgot" href="/forgot">Forgot Password?</a>
  </div>
</body>
</html>
)html";


    const char kIndexHtml[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Network Config</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; color: #111; }
    .card { max-width: 560px; padding: 16px; border: 1px solid #ddd; border-radius: 8px; margin-bottom: 16px; }
    label { display: block; margin-top: 12px; font-weight: 600; }
    input, textarea { box-sizing: border-box; width: 100%; padding: 8px; font-size: 14px; }
    textarea { min-height: 76px; resize: vertical; font-family: monospace; }
    button { margin-top: 16px; padding: 10px 14px; font-size: 14px; }
    .status { margin-top: 12px; min-height: 18px; }
    .hidden { display: none; }
    .mono { font-family: monospace; letter-spacing: 1px; }
    .note { color: #555; font-size: 13px; line-height: 1.4; }
    .error { color: #dc2626; font-size: 14px; margin-top: 10px; }
    .password-field { position: relative; }
    .password-field input { width: 100%; padding-right: 44px; }
    .password-field button.toggle-password {
      position: absolute;
      right: 10px;
      top: 0;
      bottom: 0;
      transform: none;
      margin: 0;
      width: 34px;
      border: none;
      background: transparent;
      cursor: pointer;
      color: #2563eb;
      font-size: 18px;
      padding: 0;
      line-height: 1;
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100%;
    }
    .password-field button.toggle-password:hover {
      color: #1d4ed8;
    }

    .forgot-link {
    display: block;
    margin-top: 8px;
    margin-bottom: 10px;
    text-align: right;
    color: #f97316;
    font-size: 14px;
    font-weight: 700;
    text-decoration: none;
    }

    .forgot-link:hover {
        text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="card" id="authCard">
    <h2>Config Login</h2>

    <div class="note" id="authMessage">Enter username and password.</div>

    <div id="loginPanel">
      <label>Username</label>
      <input id="loginUser" autocomplete="username" placeholder="admin"/>

      <label>Password</label>
      <div class="password-field">
        <input id="loginPass" type="password" autocomplete="current-password" placeholder="Password"/>
        <button type="button" class="toggle-password" data-target="loginPass">&#128065;</button>
      </div>

      <a class="forgot-link" href="/forgot">Forgot Password?</a>

      <button onclick="loginStep()">Login</button>
      <div class="status error" id="loginError"></div>
    </div>

    <div class="status" id="authStatus"></div>
</div>

  <div class="card hidden" id="configCard">
    <h2>Network Config</h2>
    <label>IP</label><input id="ip" placeholder="192.168.80.196"/>
    <label>Mask</label><input id="mask" placeholder="255.255.255.0"/>
    <label>Gateway</label><input id="gw" placeholder="192.168.80.254"/>
    <label>DNS 1</label><input id="dns1" placeholder="8.8.8.8"/>
    <label>DNS 2</label><input id="dns2" placeholder="1.1.1.1"/>
    <label>WiFi SSID</label><input id="wifiSsid" placeholder="MyWiFi"/>
    <label>WiFi Password</label>
    <div class="password-field">
      <input id="wifiPass" placeholder="12345678" type="password"/>
      <button type="button" class="toggle-password" data-target="wifiPass">&#128065;</button>
    </div>
    <label>MQTT Host</label><input id="mqttHost" placeholder="mqtt.dev.altasoftware.vn"/>
    <label>MQTT Port</label><input id="mqttPort" placeholder="1883"/>
    <label>MQTT Device ID</label><input id="mqttDeviceId" placeholder="LED_12345678901234"/>
    <label>MQTT Client ID</label><input id="mqttClientId" placeholder="PDM_{UUID}"/>
    <label>MQTT Username</label><input id="mqttUser" placeholder="altamedia"/>
    <label>MQTT Password</label>
    <div class="password-field">
      <input id="mqttPass" placeholder="Altamedia@%" type="password"/>
      <button type="button" class="toggle-password" data-target="mqttPass">&#128065;</button>
    </div>
    <label>MQTT Command Topic</label><input id="mqttTopic" placeholder="POWER_CTRL/{UUID}/command"/>
    <label>Output Count</label>
    <select id="outputCount">
      <option value="4">4 Channels</option>
      <option value="8">8 Channels</option>
    </select>
    <div class="button-row">
    <button onclick="save()">Apply</button>
    <button type="button" onclick="openOta()">OTA Update</button>
    </div>
    <div class="status" id="status"></div>
  </div>
  <script>
    const PORTAL_TOKEN = "__PORTAL_TOKEN__";
    let ws;
    let pendingLogin = null;
    let wsConnectTimer = null;
    let wsReconnectTimer = null;
    const WS_CONNECT_TIMEOUT_MS = 2500;
    const WS_RECONNECT_MS = 900;
    const $ = (id) => document.getElementById(id);
    function log(msg){ $('status').textContent = msg || ''; }
    function authLog(msg){ $('authStatus').textContent = msg || ''; }
    function showAuth(msg){
      $('authCard').classList.remove('hidden');
      $('configCard').classList.add('hidden');
      const loginPanelEl = $('loginPanel');
      if (loginPanelEl) loginPanelEl.classList.remove('hidden');
      authLog(msg.message || '');
      $('loginError').textContent = '';
      $('authMessage').textContent = 'Enter username and password.';
    }
    function showConfig(){
      $('authCard').classList.add('hidden');
      $('configCard').classList.remove('hidden');
      authLog('');
      ws.send(JSON.stringify({type:"get"}));
    }
    function setVals(msg){
      if(!msg) return;
      const eth = msg.eth || {};
      ip.value = eth.ip || '';
      mask.value = eth.mask || '';
      gw.value = eth.gateway || '';
      dns1.value = eth.dns1 || '';
      dns2.value = eth.dns2 || '';
      const wifi = msg.wifi || {};
      wifiSsid.value = wifi.ssid || '';
      wifiPass.value = wifi.password || '';
      const mqtt = msg.mqtt || {};
      mqttHost.value = mqtt.host || '';
      mqttPort.value = mqtt.port || 1883;
      mqttDeviceId.value = mqtt.deviceId || '';
      mqttClientId.value = mqtt.clientId || '';
      mqttUser.value = mqtt.username || '';
      mqttPass.value = mqtt.password || '';
      mqttTopic.value = mqtt.commandTopic || '';
      const device = msg.device || {};
      const outputCountEl = $('outputCount');
      if (outputCountEl)
        outputCountEl.value = device.outputCount || 4;
    }
    function setupPasswordToggles() {
      document.querySelectorAll('.toggle-password').forEach(button => {
        button.addEventListener('click', () => {
          const targetId = button.getAttribute('data-target');
          const input = targetId ? document.getElementById(targetId) : null;
          if (!input) return;
          const show = input.type === 'password';
          input.type = show ? 'text' : 'password';
          button.innerHTML = show ? '&#128584;' : '&#128065;';
        });
      });
    }
    function clearWsTimers(){
      if (wsConnectTimer) {
        clearTimeout(wsConnectTimer);
        wsConnectTimer = null;
      }
      if (wsReconnectTimer) {
        clearTimeout(wsReconnectTimer);
        wsReconnectTimer = null;
      }
    }
    function scheduleReconnect(){
      if (wsReconnectTimer) return;
      wsReconnectTimer = setTimeout(() => {
        wsReconnectTimer = null;
        connect(true);
      }, WS_RECONNECT_MS);
    }
    function closeWsQuietly(){
      if (!ws) return;
      try {
        ws.onopen = null;
        ws.onmessage = null;
        ws.onerror = null;
        ws.onclose = null;
        ws.close();
      } catch (err) {}
      ws = null;
    }
    function connect(force){
      if (ws && ws.readyState === WebSocket.OPEN) return;
      if (ws && ws.readyState === WebSocket.CONNECTING && !force) return;

      clearWsTimers();
      if (ws) closeWsQuietly();

      const host = location.hostname || '192.168.4.1';
      ws = new WebSocket(`ws://${host}:81/`);
      authLog('Connecting...');
      wsConnectTimer = setTimeout(() => {
        if (ws && ws.readyState !== WebSocket.OPEN) {
          authLog('Retrying connection...');
          closeWsQuietly();
          scheduleReconnect();
        }
      }, WS_CONNECT_TIMEOUT_MS);
      ws.onopen = () => {
        if (wsConnectTimer) {
          clearTimeout(wsConnectTimer);
          wsConnectTimer = null;
        }
        authLog('Connected');
        ws.send(JSON.stringify({type:"session", portalToken: PORTAL_TOKEN}));
        if (pendingLogin) {
          const login = pendingLogin;
          pendingLogin = null;
          sendWsLogin(login.username, login.password);
        }
      };
      ws.onmessage = (e) => {
        try {
          const msg = JSON.parse(e.data);
          switch (msg.type) {
            case "authRequired":
              showAuth(msg);
              break;
            case "authOk":
              showConfig();
              break;
            case "config":
              setVals(msg);
              break;
            case "ack":
              if (!$('authCard').classList.contains('hidden'))
                authLog(msg.message || (msg.ok ? "OK" : "Error"));
              else
                log(msg.message || (msg.ok ? "OK" : "Error"));
              break;
            case "login_ok": {
              showConfig();
              break;
            }
            case "login_fail": {
              const loginPanelEl = $('loginPanel');
              const authStatusEl = $('authStatus');
              $('loginError').innerText = msg.message || "Username or password wrong";
              if (authStatusEl) authStatusEl.innerText = "";
              if (loginPanelEl) loginPanelEl.classList.remove("hidden");
              break;
            }
            default:
              console.warn('Unknown WS message type', msg.type);
          }
        } catch (err) { authLog("Parse error"); }
      };
      ws.onerror = (error) => {
        authLog('WebSocket error, retrying...');
        console.error('WS error', error);
        closeWsQuietly();
        scheduleReconnect();
      };
      ws.onclose = () => {
        if (wsConnectTimer) {
          clearTimeout(wsConnectTimer);
          wsConnectTimer = null;
        }
        $('configCard').classList.add('hidden');
        $('authCard').classList.remove('hidden');
        authLog(pendingLogin ? 'Reconnecting, login will retry...' : 'Reconnecting...');
        scheduleReconnect();
      };
    }
    function sendWsLogin(username, password) {
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        pendingLogin = { username, password };
        authLog("Connection not ready; retrying...");
        connect(true);
        return false;
      }

      authLog("Checking login...");
      ws.send(JSON.stringify({
        type: "login",
        username,
        password,
        portalToken: PORTAL_TOKEN
      }));
      return true;
    }
    function sendLogin(username, password) {
      $('loginError').innerText = '';
      return sendWsLogin(username, password);
    }
    function save(){
      if(!ws || ws.readyState !== WebSocket.OPEN) { log('WebSocket not ready'); return; }
      const outputCountEl = $('outputCount');
      const outputCountValue = outputCountEl ? Number(outputCountEl.value || 4) : 4;
      ws.send(JSON.stringify({
        type: "set",
        eth: {
          ip: ip.value.trim(),
          mask: mask.value.trim(),
          gateway: gw.value.trim(),
          dns1: dns1.value.trim(),
          dns2: dns2.value.trim()
        },
        wifi: {
          ssid: wifiSsid.value.trim(),
          password: wifiPass.value
        },
        mqtt: {
          host: mqttHost.value.trim(),
          port: Number(mqttPort.value.trim() || 1883),
          deviceId: mqttDeviceId.value.trim(),
          clientId: mqttClientId.value.trim(),
          username: mqttUser.value.trim(),
          password: mqttPass.value,
          commandTopic: mqttTopic.value.trim()
        },
        device: {
          outputCount: outputCountValue
        }
      }));
    }
    setupPasswordToggles();
    connect();


    function openOta() {
        window.location.href = "/ota";
    }

    function loginStep() {
    $('loginError').innerText = '';
    const loginUserEl = $('loginUser');
    const loginPassEl = $('loginPass');
    const authStatusEl = $('authStatus');
    if (authStatusEl) authStatusEl.innerText = "Checking login...";

    sendLogin(
        loginUserEl ? loginUserEl.value.trim() : "",
        loginPassEl ? loginPassEl.value : ""
    );
    }
  </script>
</body>
</html>
)html";

    const char kOtaHtml[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>OTA Update</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; color: #111; }
    .card { max-width: 520px; padding: 16px; border: 1px solid #ddd; border-radius: 8px; }
    button { margin-top: 12px; padding: 10px 14px; font-size: 14px; }
  </style>
  </head>
<body>
  <div class="card">
    <h2>OTA Update</h2>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required />
      <button type="submit">Upload</button>
    </form>
    <p>Upload file firmware .bin, device will reboot after success.</p>
  </div>
</body>
</html>
)html";

const char kForgotHtml[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Reset Web Login</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; color: #111; }
    .card { max-width: 520px; padding: 16px; border: 1px solid #ddd; border-radius: 8px; }
    label { display: block; margin-top: 12px; font-weight: 600; }
    input { box-sizing: border-box; width: 100%; padding: 8px; font-size: 14px; }
    button { margin-top: 16px; padding: 10px 14px; font-size: 14px; }
    .note { color: #555; font-size: 13px; line-height: 1.4; }
    .password-field { position: relative; }
    .password-field input { padding-right: 44px; }
    .password-field button.toggle-password {
      position: absolute;
      right: 8px;
      top: 0;
      bottom: 0;
      width: 34px;
      height: 100%;
      margin: 0;
      padding: 0;
      border: none;
      background: transparent;
      color: #2563eb;
      font-size: 18px;
      line-height: 1;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
    }
  </style>
</head>
<body>
  <div class="card">
    <h2>Reset Web Login</h2>
    <p class="note">Create a new username and password.</p>

    <form method="POST" action="/reset-auth">
      <label>New Username</label>
      <input name="user" required maxlength="31" placeholder="admin"/>

      <label>New Password</label>
      <div class="password-field">
        <input id="resetPass" name="pass" type="password" required maxlength="63" placeholder="new password"/>
        <button type="button" class="toggle-password" onclick="toggleResetPassword()">&#128065;</button>
      </div>

      <button type="submit">Save & Restart ESP</button>
    </form>

    <p><a href="/">Back to config</a></p>
  </div>
  <script>
    function toggleResetPassword() {
      const input = document.getElementById('resetPass');
      const button = document.querySelector('.toggle-password');
      if (!input || !button) return;
      const show = input.type === 'password';
      input.type = show ? 'text' : 'password';
      button.innerHTML = show ? '&#128584;' : '&#128065;';
    }
  </script>
</body>
</html>
)html";

    bool parseIpField(JsonObject obj, const char *key, IPAddress &out)
    {
        // Parse chuỗi IP trong JSON sang kiểu IPAddress.
        const char *val = obj[key];
        if (!val || !*val)
            return false;
        return out.fromString(val);
    }

    void copyJsonStringField(JsonObject obj, const char *key, char *dest, size_t destSize)
    {
        // Sao chép trường string từ JSON sang buffer C-string an toàn.
        if (destSize == 0)
            return;

        const char *val = obj[key] | "";
        strncpy(dest, val, destSize - 1);
        dest[destSize - 1] = '\0';
    }

    void onWsEventStatic(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
    {
        // Hàm trung gian để chuyển callback websocket vào instance ConfigPortal.
        if (gPortal)
            gPortal->handleWsEvent(num, type, payload, length);
    }
} // namespace

ConfigPortal::ConfigPortal()
    : server_(80), ws_(81)
{
}

bool ConfigPortal::authenticatePortal()
{
    PortalAuthConfig auth;
    loadPortalAuthConfig(auth);
    return server_.authenticate(auth.username, auth.password);
}

bool ConfigPortal::authenticateOta()
{
    PortalAuthConfig auth;
    loadPortalAuthConfig(auth);
    return server_.authenticate(auth.username, auth.password);
}

void ConfigPortal::initPortal(uint8_t buttonPin, EthernetUDPHandler *ethHandler)
{
    gPortal = this;
    buttonPin_ = buttonPin;
    ethHandler_ = ethHandler;

    pinMode(buttonPin_, INPUT_PULLUP);

    // server_.on("/", HTTP_GET, [this]()
    // {
    //     // ORIG: root config page was served without HTTP auth.
    // //    if (!server_.authenticate(PORTAL_AUTH_USER, PORTAL_AUTH_PASS))
    // if (!authenticatePortal())
    //     {
    //         server_.requestAuthentication();
    //         return;
    //     }
    //     String page = FPSTR(kIndexHtml);
    //     page.replace("__PORTAL_TOKEN__", portalToken_);
    //     server_.send(200, "text/html", page);
    // });

    server_.on("/", HTTP_GET, [this]()
    {
        String page = FPSTR(kIndexHtml);
        page.replace("__PORTAL_TOKEN__", portalToken_);
        server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        server_.sendHeader("Pragma", "no-cache");
        server_.sendHeader("Expires", "0");
        server_.send(200, "text/html", page);
    });

    server_.on("/forgot", HTTP_GET, [this]()
    {
        server_.send_P(200, "text/html", kForgotHtml);
    });

    server_.on("/reset-auth", HTTP_POST, [this]()
    {
        PortalAuthConfig auth{};

        String user = server_.arg("user");
        String pass = server_.arg("pass");

        user.trim();

        if (user.length() == 0 || pass.length() == 0)
        {
            server_.send(400, "text/plain", "Username/password empty");
            return;
        }

        if (user.length() >= sizeof(auth.username) || pass.length() >= sizeof(auth.password))
        {
            server_.send(400, "text/plain", "Username/password too long");
            return;
        }

        strncpy(auth.username, user.c_str(), sizeof(auth.username) - 1);
        auth.username[sizeof(auth.username) - 1] = '\0';

        strncpy(auth.password, pass.c_str(), sizeof(auth.password) - 1);
        auth.password[sizeof(auth.password) - 1] = '\0';

        if (!savePortalAuthConfig(auth))
        {
            server_.send(500, "text/plain", "Save auth failed");
            return;
        }

        server_.send(200, "text/plain", "Web login saved. Restarting ESP...");
        delay(500);
        ESP.restart();
    });

    server_.on("/login", HTTP_POST, [this]()
    {
        PortalAuthConfig auth;
        loadPortalAuthConfig(auth);

        String user = server_.arg("user");
        String pass = server_.arg("pass");

        if (user == auth.username && pass == auth.password)
        {
            loggedIn_ = true;
            server_.sendHeader("Location", "/");
            server_.send(302, "text/plain", "");
            return;
        }

        server_.send(401, "text/html",
            "<!doctype html><html><body>"
            "<h3>Login failed</h3>"
            "<p>Wrong username or password.</p>"
            "<a href='/'>Back to login</a><br/>"
            "<a href='/forgot'>Forgot Password?</a>"
            "</body></html>");
    });

    server_.on("/login-json", HTTP_POST, [this]()
    {
        PortalAuthConfig auth;
        loadPortalAuthConfig(auth);

        String user = server_.arg("user");
        String pass = server_.arg("pass");
        user.trim();

        StaticJsonDocument<128> doc;
        if (user == auth.username && pass == auth.password)
        {
            loggedIn_ = true;
            doc["ok"] = true;
            doc["message"] = "Login OK";
        }
        else
        {
            loggedIn_ = false;
            doc["ok"] = false;
            doc["message"] = "Username or password wrong";
        }

        String out;
        serializeJson(doc, out);
        server_.send(doc["ok"] ? 200 : 401, "application/json", out);
    });

    server_.on("/ota", HTTP_GET, [this]()
        {
        //    if (!server_.authenticate(OTA_AUTH_USER, OTA_AUTH_PASS))
        if (!authenticateOta())
            {
                server_.requestAuthentication();
                return;
            }
            server_.send_P(200, "text/html", kOtaHtml);
        });

    server_.on(
        "/update",
        HTTP_POST,
        [this]()
        {
            // if (!server_.authenticate(OTA_AUTH_USER, OTA_AUTH_PASS))
            if (!authenticateOta())
            {
                server_.requestAuthentication();
                return;
            }
            const bool ok = !Update.hasError();
            server_.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
            if (ok)
            {
                delay(200);
                ESP.restart();
            }
        },
        [this]()
        {
            if (!server_.authenticate(OTA_AUTH_USER, OTA_AUTH_PASS))
            {
                server_.requestAuthentication();
                return;
            }
            HTTPUpload &upload = server_.upload();
            if (upload.status == UPLOAD_FILE_START)
            {
                Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                    Update.printError(Serial);
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                    Update.printError(Serial);
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                if (Update.end(true))
                    Serial.printf("[OTA] Done: %u bytes\n", upload.totalSize);
                else
                    Update.printError(Serial);
            }
            else if (upload.status == UPLOAD_FILE_ABORTED)
            {
                Update.end();
                Serial.println("[OTA] Aborted");
            }
        });

    ws_.onEvent(onWsEventStatic);
}

void ConfigPortal::begin(uint8_t buttonPin, EthernetUDPHandler *ethHandler, MqttHandler *mqttHandler, LedStatus *ledStatus, CentralController *centralController)
{
    initPortal(buttonPin, ethHandler);
    mqttHandler_ = mqttHandler;
    (void)ledStatus;
    centralController_ = centralController;
    Serial.print(F("[CFG] begin buttonPin="));
    Serial.println(buttonPin);
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

    makePortalToken();
    trustedSessionClients_ = 0;
    authenticatedClients_ = 0;
    loggedIn_ = false;

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
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);
    delay(100);

    active_ = false;
    pendingStop_ = false;
    trustedSessionClients_ = 0;
    authenticatedClients_ = 0;
    loggedIn_ = false;
    has_mode_config_on = false;
    has_mode_config = false;
    Serial.println(F("[CFG] Config mode OFF"));
}

// Public wrappers
void ConfigPortal::openPortal()
{
  startPortal();
}

void ConfigPortal::closePortal()
{
  stopPortal();
}

void ConfigPortal::togglePortal()
{
  if (active_)
    stopPortal();
  else
    startPortal();
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
            Serial.println(F("[CFG] Button pressed"));
        }
        else if (!pressHandled_ && (uint32_t)(now - pressStartMs_) >= kLongPressMs)
        {
            pressHandled_ = true;
            Serial.println(F("[CFG] Long press detected"));
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
        Serial.print(F("[CFG] WebSocket connected client="));
        Serial.println(num);
        setClientSessionTrusted(num, false);
        setClientAuthenticated(num, false);
        sendAuthRequired(num);
        return;
    }
    if (type == WStype_DISCONNECTED)
    {
        setClientSessionTrusted(num, false);
        setClientAuthenticated(num, false);
        // authenticatedClients_[num] = false;
        authenticatedClients_ &= ~(1UL << num);
        loginClients_[num] = false;
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

    Serial.print(F("[CFG] WS text received from client="));
    Serial.print(num);
    Serial.print(F(" length="));
    Serial.println(length);
    StaticJsonDocument<1024> doc;
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

    if (strcmp(type, "session") == 0)
    {
        const char *portalToken = doc["portalToken"] | "";
        if (isPortalTokenValid(portalToken))
        {
            setClientSessionTrusted(num, true);
            sendAuthRequired(num);
        }
        else
        {
            setClientSessionTrusted(num, false);
            sendAuthRequired(num, "Invalid session");
        }
        return;
    }

    if (strcmp(type, "login") == 0)
    {
        const char *username = doc["username"] | "";
        const char *password = doc["password"] | "";

        PortalAuthConfig authCfg;
        loadPortalAuthConfig(authCfg);

        if (strcmp(username, authCfg.username) != 0 || strcmp(password, authCfg.password) != 0)
        {
            loginClients_[num] = false;
            setClientAuthenticated(num, false);

            StaticJsonDocument<128> res;
            res["type"] = "login_fail";
            res["message"] = "Username or password wrong";

            String out;
            serializeJson(res, out);
            ws_.sendTXT(num, out);
            return;
        }

        loginClients_[num] = true;
        setClientSessionTrusted(num, true);
        setClientAuthenticated(num, true);
        loggedIn_ = false;

        sendAuthOk(num);
        sendConfig(num);
        return;
    }

    if (!isClientAuthenticated(num))
    {
        sendAuthRequired(num, "Login required");
        return;
    }

    if (strcmp(type, "get") == 0)
    {
        Serial.println(F("[CFG] WS request type=get"));
        sendConfig(num);
        return;
    }

    if (strcmp(type, "set") == 0)
    {
        Serial.println(F("[CFG] WS request type=set"));
        JsonObject eth = doc["eth"];
        JsonObject wifi = doc["wifi"];
        JsonObject mqtt = doc["mqtt"];
        JsonObject auth = doc["auth"];
        if (eth.isNull())
        {
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
            sendAck(num, false, "Save ETH failed");
            return;
        }

        WiFiConfig wifiCfg;
        loadWiFiConfig(wifiCfg);
        if (!wifi.isNull())
        {
            copyJsonStringField(wifi, "ssid", wifiCfg.ssid, sizeof(wifiCfg.ssid));
            copyJsonStringField(wifi, "password", wifiCfg.password, sizeof(wifiCfg.password));
        }

        if (!saveWiFiConfig(wifiCfg))
        {
            sendAck(num, false, "Save WiFi failed");
            return;
        }

        if (!doc["device"].isNull())
        {
            DeviceConfig deviceCfg;
            loadDeviceConfig(deviceCfg);
            const uint8_t requestedOutputCount = doc["device"]["outputCount"] | deviceCfg.outputCount;
            if (requestedOutputCount >= 4 && requestedOutputCount <= 8)
            {
                deviceCfg.outputCount = requestedOutputCount;
                if (!saveDeviceConfig(deviceCfg))
                {
                    sendAck(num, false, "Save device config failed");
                    return;
                }
            }
            else
            {
                sendAck(num, false, "Invalid output count");
                return;
            }
        }

        MqttConfig mqttCfg;
        loadMqttConfig(mqttCfg);
        if (!mqtt.isNull())
        {
            copyJsonStringField(mqtt, "host", mqttCfg.host, sizeof(mqttCfg.host));
            mqttCfg.port = mqtt["port"] | mqttCfg.port;
            copyJsonStringField(mqtt, "deviceId", mqttCfg.deviceId, sizeof(mqttCfg.deviceId));
            copyJsonStringField(mqtt, "clientId", mqttCfg.clientId, sizeof(mqttCfg.clientId));
            copyJsonStringField(mqtt, "username", mqttCfg.username, sizeof(mqttCfg.username));
            copyJsonStringField(mqtt, "password", mqttCfg.password, sizeof(mqttCfg.password));
            copyJsonStringField(mqtt, "commandTopic", mqttCfg.commandTopic, sizeof(mqttCfg.commandTopic));
        }

        if (!saveMqttConfig(mqttCfg))
        {
            sendAck(num, false, "Save MQTT failed");
            return;
        }

        if (!auth.isNull())
        {
            PortalAuthConfig authCfg;
            loadPortalAuthConfig(authCfg);

            const char *newUser = auth["username"] | "";
            const char *newPass = auth["password"] | "";

            if (newUser && newUser[0] != '\0')
            {
                strncpy(authCfg.username, newUser, sizeof(authCfg.username) - 1);
                authCfg.username[sizeof(authCfg.username) - 1] = '\0';
            }

            // Password để trống nghĩa là giữ password cũ
            if (newPass && newPass[0] != '\0')
            {
                strncpy(authCfg.password, newPass, sizeof(authCfg.password) - 1);
                authCfg.password[sizeof(authCfg.password) - 1] = '\0';
            }

            if (!savePortalAuthConfig(authCfg))
            {
                sendAck(num, false, "Save web login failed");
                return;
            }

        }

        const bool ethApplied = ethHandler_ ? ethHandler_->applyStaticConfig(cfg) : false;
        const bool wifiApplied = reloadWiFiConfig();
        const bool mqttApplied = mqttHandler_ ? mqttHandler_->reloadConfig() : false;
        const bool applied = ethApplied && wifiApplied && mqttApplied;
        Serial.print(F("[CFG] Apply config ethApplied="));
        Serial.print(ethApplied ? F("true") : F("false"));
        Serial.print(F(" wifiApplied="));
        Serial.print(wifiApplied ? F("true") : F("false"));
        Serial.print(F(" mqttApplied="));
        Serial.println(mqttApplied ? F("true") : F("false"));
        sendAck(num, applied, applied ? "Applied" : "Apply failed");
        sendConfig(num);
        PortalAuthConfig authCfg;
        loadPortalAuthConfig(authCfg);

        JsonObject authOut = doc.createNestedObject("auth");
        authOut["username"] = authCfg.username;
        
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

void ConfigPortal::makePortalToken()
{
    static const char hex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < 8; i++)
    {
        const uint8_t value = (uint8_t)(esp_random() & 0xFF);
        portalToken_[i * 2] = hex[(value >> 4) & 0x0F];
        portalToken_[i * 2 + 1] = hex[value & 0x0F];
    }
    portalToken_[16] = '\0';
}

bool ConfigPortal::isPortalTokenValid(const char *token) const
{
    if (!token || !portalToken_[0])
        return false;
    if (strlen(token) != 16)
        return false;

    uint8_t diff = 0;
    for (uint8_t i = 0; i < 16; i++)
        diff |= (uint8_t)(portalToken_[i] ^ token[i]);

    return diff == 0;
}

bool ConfigPortal::isClientSessionTrusted(uint8_t num) const
{
    if (num >= 32)
        return false;
    return (trustedSessionClients_ & (1UL << num)) != 0;
}

void ConfigPortal::setClientSessionTrusted(uint8_t num, bool trusted)
{
    if (num >= 32)
        return;

    const uint32_t mask = (1UL << num);
    if (trusted)
        trustedSessionClients_ |= mask;
    else
        trustedSessionClients_ &= ~mask;
}

bool ConfigPortal::isClientAuthenticated(uint8_t num) const
{
    if (num >= 32)
        return false;
    return (authenticatedClients_ & (1UL << num)) != 0;
}

void ConfigPortal::setClientAuthenticated(uint8_t num, bool authenticated)
{
    if (num >= 32)
        return;

    const uint32_t mask = (1UL << num);
    if (authenticated)
        authenticatedClients_ |= mask;
    else
        authenticatedClients_ &= ~mask;
}

void ConfigPortal::sendAuthRequired(uint8_t num, const char *message)
{
    StaticJsonDocument<160> doc;
    doc["type"] = "authRequired";
    doc["message"] = (message && *message) ? message : "Login required";

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(num, out);
}

void ConfigPortal::sendAuthOk(uint8_t num)
{
    StaticJsonDocument<96> doc;
    doc["type"] = "authOk";

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(num, out);
}

void ConfigPortal::sendConfig(uint8_t num)
{
    EthStaticConfig cfg;
    loadEthStaticConfig(cfg);
    WiFiConfig wifiCfg;
    loadWiFiConfig(wifiCfg);
    MqttConfig mqttCfg;
    loadMqttConfig(mqttCfg);

    StaticJsonDocument<1024> doc;
    doc["type"] = "config";
    JsonObject eth = doc.createNestedObject("eth");
    eth["ip"] = cfg.ip.toString();
    eth["mask"] = cfg.mask.toString();
    eth["gateway"] = cfg.gateway.toString();
    eth["dns1"] = cfg.dns1.toString();
    eth["dns2"] = cfg.dns2.toString();

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = wifiCfg.ssid;
    wifi["password"] = wifiCfg.password;

    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["host"] = mqttCfg.host;
    mqtt["port"] = mqttCfg.port;
    mqtt["deviceId"] = mqttCfg.deviceId;
    mqtt["clientId"] = mqttCfg.clientId;
    mqtt["username"] = mqttCfg.username;
    mqtt["password"] = mqttCfg.password;
    mqtt["commandTopic"] = mqttCfg.commandTopic;

    DeviceConfig deviceCfg;
    loadDeviceConfig(deviceCfg);
    JsonObject device = doc.createNestedObject("device");
    device["outputCount"] = deviceCfg.outputCount;

    String out;
    serializeJson(doc, out);
    Serial.print(F("[CFG] Sending config to client="));
    Serial.println(num);
    ws_.sendTXT(num, out);

    // PortalAuthConfig authCfg;
    // loadPortalAuthConfig(authCfg);

    // JsonObject authOut = doc.createNestedObject("auth");
    // authOut["username"] = authCfg.username;
}

void ConfigPortal::sendAck(uint8_t num, bool ok, const char *message)
{
    StaticJsonDocument<128> doc;
    doc["type"] = "ack";
    doc["ok"] = ok;
    doc["message"] = message;

    String out;
    serializeJson(doc, out);
    Serial.print(F("[CFG] Sending ack to client="));
    Serial.print(num);
    Serial.print(F(" ok="));
    Serial.print(ok ? F("true") : F("false"));
    Serial.print(F(" msg="));
    Serial.println(message);
    ws_.sendTXT(num, out);
}
