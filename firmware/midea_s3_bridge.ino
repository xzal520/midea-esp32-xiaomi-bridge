// ESP32-S3-MINI: full Midea/MIoT/HomeKit bridge with PSRAM log storage.
// MIoT RX GPIO7 / TX GPIO8.
// Midea TX -> GPIO17 (UART2 RX); Midea RX <- GPIO16 (UART2 TX); common GND.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <sodium.h>
#include <HomeSpan.h>
#include <HAP.h>
#include "secrets.h"
#if __has_include("pairing_qr.h")
#include "pairing_qr.h"
#else
static const char PAIRING_QR_SVG[] PROGMEM = R"QR(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 80"><rect width="320" height="80" fill="white"/><text x="10" y="35" font-size="14">Generate pairing_qr.h from the HomeSpan payload</text></svg>)QR";
#endif
#include <Preferences.h>
#include "responsive_web_server.h"
#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"
#include "miot_bridge.h"
#include "midea_uart_state.h"
#include "bridge_ble.h"
#include <cJSON.h>
#include "device_metrics.h"
#include "web_pages.h"
#include "wifi_health.h"

static constexpr uint32_t USB_BAUD = 115200;
static constexpr uint32_t MODULE_BAUD = 9600;
static constexpr int MODULE_RX_PIN = 17;
static constexpr int MODULE_TX_PIN = 16;
static constexpr uint32_t MIOT_BAUD = 115200;
static constexpr int MIOT_RX_PIN = 7;
static constexpr int MIOT_TX_PIN = 8;

static const char *DEFAULT_NETWORK_HOSTNAME = "midea-s3";
static String networkHostname=DEFAULT_NETWORK_HOSTNAME;
static String mdnsHostname=DEFAULT_NETWORK_HOSTNAME;

static constexpr size_t MAX_FRAME_BYTES = 256;
static constexpr size_t LOG_HEX_CAPACITY = MAX_FRAME_BYTES * 3 + 1;
static constexpr size_t PSRAM_LOG_CAPACITY = 512;
static constexpr size_t FALLBACK_LOG_CAPACITY = 16;
static constexpr size_t LOG_BATCH_LIMIT = 24;
static constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
static constexpr uint32_t RX_FRAME_IDLE_MS = 35;

static bool saveNetworkCredentials(const char *ssid, const char *password, String &message);

struct LogEntry {
  uint32_t id;
  uint32_t uptimeMs;
  char direction[8];
  char hex[LOG_HEX_CAPACITY];
};

static LogEntry *logEntries = nullptr;
static DRAM_ATTR LogEntry fallbackLogEntries[FALLBACK_LOG_CAPACITY];
static size_t logCapacity = 0;
static bool logStorageInPsram = false;
static size_t logWriteIndex = 0, logCount = 0;
static uint32_t nextLogId = 1;
static constexpr size_t IO16_RX_LOG_CAPACITY=128,IO16_RX_FALLBACK_CAPACITY=8;
static LogEntry *io16RxLogs=nullptr;
static DRAM_ATTR LogEntry io16RxFallback[IO16_RX_FALLBACK_CAPACITY];
static size_t io16RxLogCapacity=0,io16RxLogIndex=0,io16RxLogCount=0;
static uint32_t nextIo16RxLogId=1,io16RxBytes=0,io16RxFrames=0,io16RxOverflows=0,io16RxSignalErrors=0;
static uint8_t io16RxFrame[MAX_FRAME_BYTES];
static size_t io16RxFrameLength=0;
static uint32_t lastIo16RxByteMs=0;
static bool io16RxMode=false;
static QueueHandle_t io16RxQueue=nullptr;
static uint8_t rxFrame[MAX_FRAME_BYTES];
static size_t rxFrameLength = 0;
static uint32_t lastRxByteMs = 0;
static uint8_t usbFrame[MAX_FRAME_BYTES];
static size_t usbFrameLength = 0;
static uint32_t lastUsbByteMs = 0;
static HardwareSerial MiotSerial(1);
static volatile uint32_t miotRxOverflows=0,miotSignalErrors=0;
static MiotMideaBridge miotBridge;
static uint32_t mideaValidFrames = 0, mideaRejectedFrames = 0;
static ResponsiveWebServer server(80);
static uint32_t webMaxHandleMs=0,homekitMaxPollMs=0;
static WifiHealth wifiHealth;
static bool setupApMode = false;
static bool wifiProvisionApActive = false;
static bool wifiOfflineTimerStarted = false;
static uint32_t wifiOfflineSince = 0;
static uint32_t wifiProvisionRetryAt = 0;
static constexpr uint32_t WIFI_PROVISION_DELAY_MS = 90000;
static bool webStarted = false;
static bool webRoutesConfigured = false;
static bool mdnsStarted = false;
static uint32_t wifiStartedAt = 0;
static uint32_t lastReconnectAt = 0;
static Preferences webPrefs;
static String webPassword = DEFAULT_WEB_PASSWORD;
static String webSessionToken;
static String configuredWifiSsid;
static String configuredWifiPassword;
static uint32_t scheduledRestartAt = 0;
static bool firmwareUpdateAuthorized = false;
static bool firmwareUpdateActive = false;
static bool firmwareUpdateFailed = false;
static bool firmwareUpdateReady = false;
static size_t firmwareUpdateBytes = 0;
static int firmwareUpdateHttpStatus = 400;
static char firmwareUpdateError[112] = "";
static bool bleAvailable = false;
static bool bleEnabled = true;
static bool bleAuthenticated = false;
static uint32_t bleSessionSeen = 0;
static uint32_t lastBleStatusAt = 0;
static uint8_t bleAuthFailures = 0;
static uint32_t bleAuthBlockedUntil = 0;
static bool mockMode = false;
static bool statusKnown = false;
static bool statusPower = false;
static MideaCommandFlags mideaCommandFlags;
static uint32_t lastMideaTxAt=0,nextSelfCleanQueryAt=0,nextBuzzerQueryAt=0;
static uint8_t selfCleanSequence=0;
static MideaTimers mideaTimers, requestedTimers;
static bool timerPending=false,timerMiotPending=false;
static uint32_t timerRequestId=0, timerSentAt=0, timerNextQueryAt=0;
static const char *timerResultState="idle";
static String timerResult="";
static uint8_t statusMode = 2;
static float statusTemperature = 23.5f;
static uint8_t statusFan = 0x28;
static uint8_t statusSwing = 0;
static bool statusEco = false;
static bool statusSleep = false;
static bool statusDisplayOn = true;
static bool statusAuxHeat = false;
static String statusRaw = "尚未收到状态回包";
static uint32_t lastStatusAt = 0;
static uint32_t lastQueryAt = 0;
static bool roomTemperatureKnown = false;
static float roomTemperature = 23.0f;
static uint32_t scheduledQueryAt = 0;
static bool httpMdnsAdded = false;
static bool homeKitPaired = false;
static Preferences repairPrefs;
static bool clearStalePairingOnce = false;
static SpanCharacteristic *hkCurrentState = nullptr;
static SpanCharacteristic *hkTargetState = nullptr;
static SpanCharacteristic *hkCurrentTemperature = nullptr;
static SpanCharacteristic *hkTargetTemperature = nullptr;

static String compactJson(cJSON *object) {
  char *encoded = cJSON_PrintUnformatted(object);
  String result = encoded ? encoded : "{}";
  if (encoded) cJSON_free(encoded);
  cJSON_Delete(object);
  return result;
}

static String jsonQuoted(const String &text) {
  cJSON *value = cJSON_CreateString(text.c_str());
  return compactJson(value);
}

static String jsonQuoted(const char *text) {
  cJSON *value = cJSON_CreateString(text ? text : "");
  return compactJson(value);
}

static const char LOGIN_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Midea 登录</title>
<style>body{font:16px system-ui,"Microsoft YaHei",sans-serif;background:#f3f6fa;color:#17212b;margin:0;padding:24px}main{max-width:420px;margin:10vh auto;background:#fff;padding:24px;border:1px solid #dfe5eb;border-radius:12px}input,button{box-sizing:border-box;width:100%;font:inherit;padding:12px;border:1px solid #cbd5df;border-radius:7px;margin:8px 0}button{background:#1769e0;color:#fff;border:0;cursor:pointer}#msg{color:#ba3b36;min-height:1.5em}</style></head>
<body><main><h2>Midea 空调调试登录</h2><p>请输入网页密码，无需用户名。</p><form id="login"><input id="password" type="password" autocomplete="current-password" placeholder="网页密码" required autofocus><button>登录</button></form><div id="msg"></div></main>
<script>document.getElementById('login').addEventListener('submit',async e=>{e.preventDefault();const b=e.currentTarget.querySelector('button'),m=document.getElementById('msg');b.disabled=true;m.textContent='正在登录…';try{const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'text/plain'},body:document.getElementById('password').value});if(!r.ok)throw Error(await r.text());location.replace('/')}catch(err){m.textContent='登录失败：'+err.message}finally{b.disabled=false}});</script></body></html>
)HTML";

static const char FALLBACK_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 调试热点</title>
<style>body{font:16px system-ui,"Microsoft YaHei",sans-serif;background:#f3f6fa;padding:20px;color:#17212b}main{max-width:760px;margin:auto;background:white;padding:20px;border-radius:12px}a{color:#1769e0}</style></head>
<body><main><h2>ESP32 尚未连上路由器</h2><p>当前是备用调试热点模式。请检查固件中的 Wi-Fi 名称和密码，然后重新烧录。</p><p>热点密码请查看本机固件配置。</p><p>UART 仍在后台监听和透传。</p></main></body></html>
)HTML";

static const char WIFI_PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>配置 Wi-Fi</title><style>body{font:16px system-ui,sans-serif;background:#f2f5f3;color:#172b27;padding:20px}main{max-width:430px;margin:5vh auto;background:#fff;padding:24px;border-radius:14px}input,button{box-sizing:border-box;width:100%;font:inherit;padding:12px;margin:6px 0 14px;border:1px solid #bdccc5;border-radius:8px}button{background:#126b5b;color:#fff;border:0}label{font-weight:600}</style></head><body><main><h2>连接家庭 Wi-Fi</h2><p>请输入 2.4 GHz Wi-Fi 信息和当前网页密码。保存后设备会重启。</p><form action="/wifi-setup" method="post"><label>Wi-Fi 名称</label><input name="ssid" required maxlength="32" autocomplete="off"><label>Wi-Fi 密码</label><input type="password" name="password" maxlength="64"><label>网页密码</label><input type="password" name="webPassword" required autocomplete="current-password"><button>保存并重启</button></form></main></body></html>
)HTML";

static String bytesToHex(const uint8_t *data, size_t length) {
  String out;
  out.reserve(length * 3);
  char byteText[4];
  for (size_t i = 0; i < length; ++i) {
    snprintf(byteText, sizeof(byteText), "%02X", data[i]);
    if (i) out += ' ';
    out += byteText;
  }
  return out;
}

static void beginLogStorage() {
  logEntries = static_cast<LogEntry *>(heap_caps_calloc(
      PSRAM_LOG_CAPACITY, sizeof(LogEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (logEntries) {
    logCapacity = PSRAM_LOG_CAPACITY;
    logStorageInPsram = true;
  } else {
    logEntries = fallbackLogEntries;
    logCapacity = FALLBACK_LOG_CAPACITY;
    logStorageInPsram = false;
  }
  Serial.printf("[LOG] %u entries, %u bytes, storage=%s\n",
      static_cast<unsigned>(logCapacity),
      static_cast<unsigned>(logCapacity * sizeof(LogEntry)),
      logStorageInPsram ? "PSRAM" : "internal RAM fallback");
}

static void writeLogEntry(const char *direction, const char *text) {
  if (!logEntries || !logCapacity || !text || !*text) return;
  LogEntry &entry = logEntries[logWriteIndex];
  entry.id = nextLogId++;
  entry.uptimeMs = millis();
  snprintf(entry.direction, sizeof(entry.direction), "%s", direction);
  snprintf(entry.hex, sizeof(entry.hex), "%s", text);
  logWriteIndex = (logWriteIndex + 1) % logCapacity;
  if (logCount < logCapacity) ++logCount;
}

static void addLog(const char *direction, const uint8_t *data, size_t length) {
  if (!length || !logEntries) return;
  LogEntry &entry = logEntries[logWriteIndex];
  entry.id = nextLogId++;
  entry.uptimeMs = millis();
  snprintf(entry.direction, sizeof(entry.direction), "%s", direction);
  size_t used = 0;
  for (size_t i = 0; i < length && used + 4 < sizeof(entry.hex); ++i) {
    const int written = snprintf(entry.hex + used, sizeof(entry.hex) - used,
        i ? " %02X" : "%02X", data[i]);
    if (written <= 0) break;
    used += static_cast<size_t>(written);
  }
  logWriteIndex = (logWriteIndex + 1) % logCapacity;
  if (logCount < logCapacity) ++logCount;
}

static void addTextLog(const char *direction, const char *text) {
  writeLogEntry(direction, text);
}

static void beginIo16RxLogStorage() {
  if(io16RxLogs)return;
  io16RxLogs=static_cast<LogEntry *>(heap_caps_calloc(IO16_RX_LOG_CAPACITY,sizeof(LogEntry),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  io16RxLogCapacity=io16RxLogs?IO16_RX_LOG_CAPACITY:IO16_RX_FALLBACK_CAPACITY;
  if(!io16RxLogs)io16RxLogs=io16RxFallback;
}

static void flushIo16RxFrame() {
  if(!io16RxFrameLength || !io16RxLogs || !io16RxLogCapacity)return;
  LogEntry &entry=io16RxLogs[io16RxLogIndex];
  entry.id=nextIo16RxLogId++;entry.uptimeMs=lastIo16RxByteMs;
  snprintf(entry.direction,sizeof(entry.direction),"IO16RX");
  snprintf(entry.hex,sizeof(entry.hex),"%s",bytesToHex(io16RxFrame,io16RxFrameLength).c_str());
  io16RxLogIndex=(io16RxLogIndex+1)%io16RxLogCapacity;
  if(io16RxLogCount<io16RxLogCapacity)++io16RxLogCount;
  ++io16RxFrames;io16RxFrameLength=0;
}

static void serviceIo16Rx() {
  if(!io16RxMode)return;
  uart_event_t event{};
  for(size_t i=0;io16RxQueue && i<16 && xQueueReceive(io16RxQueue,&event,0)==pdTRUE;++i){
    if(event.type==UART_FIFO_OVF || event.type==UART_BUFFER_FULL)++io16RxOverflows;
    else if(event.type==UART_FRAME_ERR || event.type==UART_PARITY_ERR || event.type==UART_BREAK)++io16RxSignalErrors;
  }
  uint8_t data[64];size_t budget=2048;
  while(budget){
    const size_t batch=budget<sizeof(data)?budget:sizeof(data);
    const int received=uart_read_bytes(UART_NUM_0,data,batch,0);
    if(received<=0)break;
    for(int i=0;i<received;++i){
      if(io16RxFrameLength>=MAX_FRAME_BYTES)flushIo16RxFrame();
      io16RxFrame[io16RxFrameLength++]=data[i];
    }
    io16RxBytes+=uint32_t(received);lastIo16RxByteMs=millis();budget-=size_t(received);
  }
  if(io16RxFrameLength && uint32_t(millis()-lastIo16RxByteMs)>=RX_FRAME_IDLE_MS)flushIo16RxFrame();
}


static bool enterMiotProvision(String &message) {
  return miotBridge.provision(message);
}

static void handleMiotProvision() {
  if (!requireLogin()) return;
  if (server.arg("plain") != "restore") {
    server.send(400, "text/plain; charset=utf-8", "请从网页确认进入米家模组配网。");
    return;
  }
  String message;
  const bool accepted = enterMiotProvision(message);
  server.send(accepted ? 200 : 429, "text/plain; charset=utf-8", message);
}

static bool requireLogin() {
  if (setupApMode) return true;
  const String cookie = server.header("Cookie");
  if (webSessionToken.length() && cookie.indexOf("midea_session=" + webSessionToken) >= 0) return true;
  const String path = server.uri();
  if (path == "/" || path == "/integrations" || path == "/device" ||
      path == "/debug" || path == "/settings") {
    server.sendHeader("Location", "/login");
    server.send(303, "text/plain; charset=utf-8", "请先登录。");
  } else {
    server.send(401, "text/plain; charset=utf-8", "登录已失效，请重新输入网页密码。");
  }
  return false;
}

static void handleLoginPage() {
  if (setupApMode) {
    server.send_P(200, "text/html; charset=utf-8", FALLBACK_HTML);
    return;
  }
  if (webSessionToken.length() && server.header("Cookie").indexOf("midea_session=" + webSessionToken) >= 0) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain; charset=utf-8", "已登录。");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", LOGIN_HTML);
}

static void handleLogin() {
  if (setupApMode) {
    server.send(403, "text/plain; charset=utf-8", "备用热点模式无需网页登录。");
    return;
  }
  if (server.arg("plain") != webPassword) {
    server.send(401, "text/plain; charset=utf-8", "密码错误。");
    return;
  }
  server.sendHeader("Set-Cookie", "midea_session=" + webSessionToken + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", "登录成功。");
}

static bool saveWebPassword(const String &newPassword, String &message) {
  if (newPassword.length() < 6 || newPassword.length() > 64 ||
      newPassword.indexOf('\r') >= 0 || newPassword.indexOf('\n') >= 0) {
    message = "新密码长度须为 6 到 64 个字符，且不能包含换行。";
    return false;
  }
  if (!webPrefs.begin("midea_web", false)) {
    message = "无法打开密码存储。";
    return false;
  }
  const size_t saved = webPrefs.putString("password", newPassword);
  webPrefs.end();
  if (!saved) {
    message = "保存密码失败。";
    return false;
  }
  webPassword = newPassword;
  // Revoke existing browser sessions. BLE stays authenticated for this connection.
  webSessionToken = String(static_cast<uint32_t>(esp_random()), HEX) + String(static_cast<uint32_t>(esp_random()), HEX);
  message = "网页登录密码已更新；App 和网页下次登录请使用新密码。";
  return true;
}

static void handleChangePassword() {
  if (!requireLogin()) return;
  const String body = server.arg("plain");
  const int separator = body.indexOf('\n');
  if (separator < 0) {
    server.send(400, "text/plain; charset=utf-8", "请求格式无效。");
    return;
  }
  const String oldPassword = body.substring(0, separator);
  const String newPassword = body.substring(separator + 1);
  if (oldPassword != webPassword) {
    server.send(403, "text/plain; charset=utf-8", "当前密码不正确。");
    return;
  }
  String message;
  if (!saveWebPassword(newPassword, message)) {
    const int status = newPassword.length() < 6 || newPassword.length() > 64 ||
        newPassword.indexOf('\r') >= 0 || newPassword.indexOf('\n') >= 0 ? 400 : 500;
    server.send(status, "text/plain; charset=utf-8", message);
    return;
  }
  server.sendHeader("Set-Cookie", "midea_session=" + webSessionToken + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", message);
}

static void handleWifiConfig() {
  if (!requireLogin()) return;
  if (scheduledRestartAt || firmwareUpdateActive) {
    server.send(409, "text/plain; charset=utf-8", "设备正在重启或更新，请稍后重试。");
    return;
  }
  const String payload = server.arg("plain");
  cJSON *json = cJSON_Parse(payload.c_str());
  if (!cJSON_IsObject(json)) {
    if (json) cJSON_Delete(json);
    server.send(400, "text/plain; charset=utf-8", "无效 JSON 请求。");
    return;
  }
  const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
  const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
  String message;
  const bool ok = cJSON_IsString(ssid) && cJSON_IsString(password) &&
      saveNetworkCredentials(ssid->valuestring, password->valuestring, message);
  cJSON_Delete(json);
  if (!ok) {
    server.send(400, "text/plain; charset=utf-8", message.length() ? message : "Wi-Fi 参数无效。");
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "Wi-Fi 设置已保存，设备将在 3 秒后自动重启并连接新网络。");
}

static void handleWifiScan() {
  if (!requireLogin()) return;
  if (scheduledRestartAt || firmwareUpdateActive) {
    server.send(409, "text/plain; charset=utf-8", "设备正在维护中。");
    return;
  }
  const int16_t count = WiFi.scanNetworks(false, false);
  cJSON *reply = cJSON_CreateObject();
  cJSON_AddBoolToObject(reply, "ok", count >= 0);
  cJSON *networks = cJSON_AddArrayToObject(reply, "networks");
  for (int16_t i = 0; count >= 0 && i < count && i < 30; ++i) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", ssid.c_str());
    cJSON_AddNumberToObject(item, "rssi", WiFi.RSSI(i));
    cJSON_AddBoolToObject(item, "secure", WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    cJSON_AddItemToArray(networks, item);
  }
  WiFi.scanDelete();
  server.send(200, "application/json; charset=utf-8", compactJson(reply));
}

static bool normalizeDeviceHostname(String &name) {
  name.trim();
  // NetworkManager's 32-byte buffer must hold the complete UTF-8 name and NUL.
  if(!name.length() || name.length()>31)return false;
  for(size_t i=0;i<name.length();){
    const uint8_t first=uint8_t(name[i++]);
    if(first<0x80){if(first<0x20 || first==0x7F)return false;continue;}
    uint32_t code;uint8_t trailing;uint32_t minimum;
    if(first>=0xC2 && first<=0xDF){code=first&0x1F;trailing=1;minimum=0x80;}
    else if(first>=0xE0 && first<=0xEF){code=first&0x0F;trailing=2;minimum=0x800;}
    else if(first>=0xF0 && first<=0xF4){code=first&0x07;trailing=3;minimum=0x10000;}
    else return false;
    if(i+trailing>name.length())return false;
    while(trailing--){
      const uint8_t next=uint8_t(name[i++]);if((next&0xC0)!=0x80)return false;
      code=(code<<6)|(next&0x3F);
    }
    if(code<minimum || code>0x10FFFF || (code>=0xD800 && code<=0xDFFF) || (code>=0x80 && code<=0x9F))return false;
  }
  return true;
}

static bool normalizeMdnsHostname(String &name) {
  name.trim();
  if(!name.length() || name.length()>31)return false;
  for(size_t i=0;i<name.length();++i){
    char c=name[i];if(c>='A' && c<='Z'){c=char(c-'A'+'a');name[i]=c;}
    if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-'))return false;
  }
  return name[0]!='-' && name[name.length()-1]!='-';
}

static bool readNetworkIdentity(cJSON *json,String &hostname,String &mdns) {
  if(!cJSON_IsObject(json))return false;
  const cJSON *hostItem=cJSON_GetObjectItemCaseSensitive(json,"hostname");
  const cJSON *mdnsItem=cJSON_GetObjectItemCaseSensitive(json,"mdnsHostname");
  if(!cJSON_IsString(hostItem) || !cJSON_IsString(mdnsItem))return false;
  hostname=hostItem->valuestring;mdns=mdnsItem->valuestring;
  return normalizeDeviceHostname(hostname) && normalizeMdnsHostname(mdns);
}

static String networkHostnameUrl(const String &name) {
  return String("http://")+name+".local/";
}

static void handleNetworkHostname() {
  if(!requireLogin())return;
  String requested=networkHostname,requestedMdns=mdnsHostname;bool restart=false;
  if(server.method()==HTTP_POST){
    if(firmwareUpdateActive || scheduledRestartAt || miotBridge.pending() || timerPending){
      server.send(409,"text/plain; charset=utf-8","设备正在处理其他操作，请稍后修改主机名。");return;
    }
    const String body=server.arg("plain");
    cJSON *input=cJSON_ParseWithOpts(body.c_str(),nullptr,true);
    const bool valid=readNetworkIdentity(input,requested,requestedMdns);cJSON_Delete(input);
    if(!valid){
      server.send(400,"text/plain; charset=utf-8","主机名支持中文，限 1–31 个 UTF-8 字节，不能含控制字符；访问地址限 1–31 位英文字母、数字或短横线，首尾不能是短横线。");return;
    }
    if(requested!=networkHostname || requestedMdns!=mdnsHostname){
      cJSON *identity=cJSON_CreateObject();
      cJSON_AddStringToObject(identity,"hostname",requested.c_str());
      cJSON_AddStringToObject(identity,"mdnsHostname",requestedMdns.c_str());
      const String encoded=compactJson(identity);
      if(encoded=="{}"){
        server.send(500,"text/plain; charset=utf-8","无法准备网络名称配置。");return;
      }
      if(!webPrefs.begin("midea_web",false)){
        server.send(500,"text/plain; charset=utf-8","无法打开主机名存储。");return;
      }
      // One NVS record keeps the two independent names consistent on power loss.
      const size_t saved=webPrefs.putString("net_identity",encoded);webPrefs.end();
      if(!saved){server.send(500,"text/plain; charset=utf-8","主机名保存失败，请重试。");return;}
      // Keep active names unchanged until reboot; HomeSpan owns mDNS.
      restart=true;scheduledRestartAt=millis()+3000;
    }
  }
  cJSON *json=cJSON_CreateObject();
  cJSON_AddStringToObject(json,"hostname",requested.c_str());
  cJSON_AddStringToObject(json,"mdnsHostname",requestedMdns.c_str());
  cJSON_AddStringToObject(json,"url",networkHostnameUrl(requestedMdns).c_str());
  cJSON_AddStringToObject(json,"ipUrl",WiFi.status()==WL_CONNECTED?(String("http://")+WiFi.localIP().toString()+"/").c_str():"");
  cJSON_AddBoolToObject(json,"restart",restart);
  cJSON_AddStringToObject(json,"message",restart?"网络名称已保存，3 秒后重启；请重新登录。":"设置未改变，无需重启。");
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",compactJson(json));
}

static void addFanMappingStatus(cJSON *json) {
  cJSON *levels=cJSON_AddArrayToObject(json,"levels");
  cJSON *defaults=cJSON_AddArrayToObject(json,"defaults");
  for(uint8_t i=0;i<7;++i){
    cJSON_AddItemToArray(levels,cJSON_CreateNumber(miotBridge.fanMapping().levels[i]));
    cJSON_AddItemToArray(defaults,cJSON_CreateNumber(MiotMideaBridge::defaultFanMapping().levels[i]));
  }
  cJSON_AddNumberToObject(json,"auto",102);
}

static void handleFanMapping() {
  if(!requireLogin())return;
  if(server.method()==HTTP_POST){
    if(firmwareUpdateActive || scheduledRestartAt || miotBridge.pending() || timerPending){
      server.send(409,"text/plain; charset=utf-8","设备正在处理其他操作，请稍后保存映射。");return;
    }
    cJSON *root=cJSON_Parse(server.arg("plain").c_str());
    const cJSON *levels=cJSON_GetObjectItemCaseSensitive(root,"levels");
    MiotMideaBridge::FanMapping mapping;
    bool valid=cJSON_IsObject(root) && cJSON_IsArray(levels) && cJSON_GetArraySize(levels)==7;
    for(uint8_t i=0;valid && i<7;++i){
      const cJSON *item=cJSON_GetArrayItem(levels,i);
      valid=cJSON_IsNumber(item) && isfinite(item->valuedouble) &&
        item->valuedouble>=1 && item->valuedouble<=100 && item->valuedouble==floor(item->valuedouble);
      if(valid)mapping.levels[i]=uint8_t(item->valueint);
    }
    cJSON_Delete(root);
    if(!valid || !mapping.valid()){
      server.send(400,"text/plain; charset=utf-8","请填写七个 1–100 的整数，并按档位严格递增。");return;
    }
    if(memcmp(mapping.levels,miotBridge.fanMapping().levels,sizeof(mapping.levels))!=0){
      if(!webPrefs.begin("midea_web",false)){
        server.send(500,"text/plain; charset=utf-8","无法打开风速映射存储。");return;
      }
      const size_t saved=webPrefs.putBytes("fan_levels",mapping.levels,sizeof(mapping.levels));webPrefs.end();
      if(saved!=sizeof(mapping.levels)){
        server.send(500,"text/plain; charset=utf-8","风速映射保存失败，请重试。");return;
      }
      miotBridge.setFanMapping(mapping);
    }
  }
  cJSON *json=cJSON_CreateObject();addFanMappingStatus(json);
  if(server.method()==HTTP_POST)cJSON_AddStringToObject(json,"message","风速映射已保存，下次调档使用新映射；重启后仍有效。");
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",compactJson(json));
}

static void handleBleConfig() {
  if (!requireLogin()) return;
  const String body = server.arg("plain");
  if (body != "on" && body != "off") {
    server.send(400, "text/plain; charset=utf-8", "请选择启用或关闭蓝牙。");
    return;
  }
  if (firmwareUpdateActive || scheduledRestartAt) {
    server.send(409, "text/plain; charset=utf-8", "设备正在更新或准备重启，请稍后再试。");
    return;
  }
  const bool enabled = body == "on";
  if (enabled == bleEnabled) {
    server.send(200, "text/plain; charset=utf-8", enabled ? "蓝牙已经开启。" : "蓝牙已经关闭。");
    return;
  }
  if (!webPrefs.begin("midea_web", false)) {
    server.send(500, "text/plain; charset=utf-8", "无法打开设置存储。");
    return;
  }
  const size_t saved = webPrefs.putBool("ble_enabled", enabled);
  webPrefs.end();
  if (!saved) {
    server.send(500, "text/plain; charset=utf-8", "蓝牙设置保存失败。");
    return;
  }
  bleEnabled = enabled;
  scheduledRestartAt = millis() + 3000;
  server.send(200, "text/plain; charset=utf-8", enabled ?
      "蓝牙已开启，设备将在 3 秒后重启生效。" :
      "蓝牙已关闭，设备将在 3 秒后重启生效；网页和 Wi-Fi 功能继续可用。");
}

static void handleLogout() {
  server.sendHeader("Set-Cookie", "midea_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", "已退出登录。");
}

static void sendWebAsset(const char *type,const char *content,const char *tag) {
  if(!requireLogin())return;
  // Assets contain no device state. Versioned local URLs can be reused across
  // pages, while HTML and authenticated API responses remain uncached.
  const String etag=String("\"")+BRIDGE_VERSION+"-"+tag+"\"";
  server.sendHeader("Cache-Control","private, max-age=31536000, immutable");
  server.sendHeader("ETag",etag);
  if(server.header("If-None-Match")==etag){
    server.send(304,type,"");return;
  }
  server.sendFlash(200,type,content,strlen_P(content));
}

static void handleRoot() {
  if (wifiProvisionApActive && server.uri()=="/" && server.client().localIP()==WiFi.softAPIP()) {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WIFI_PROVISION_HTML);
    return;
  }
  if (!requireLogin()) return;
  server.sendHeader("Cache-Control", "no-store");
  if (setupApMode) {
    server.send_P(200, "text/html; charset=utf-8", FALLBACK_HTML);
    return;
  }
  const String path = server.uri();
  const char *page = WEB_CONTROL;
  if (path == "/integrations") page = WEB_INTEGRATIONS;
  else if (path == "/device") page = WEB_DEVICE;
  else if (path == "/debug") page = WEB_DEBUG;
  else if (path == "/settings") page = WEB_SETTINGS;
  // Stream only the requested page; do not assemble the HTML in a heap String.
  server.setContentLength(strlen_P(WEB_HEAD)+strlen_P(page)+strlen_P(WEB_FOOT));
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent_P(WEB_HEAD);
  server.sendContent_P(page);
  server.sendContent_P(WEB_FOOT);
  server.sendContent("");
}

static const char *getResetReasonDescription(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "上电开机 (POWERON)";
    case ESP_RST_EXT: return "外部引脚复位 (EXT)";
    case ESP_RST_SW: return "软件重启 (SW)";
    case ESP_RST_PANIC: return "程序异常崩溃 (PANIC)";
    case ESP_RST_INT_WDT: return "中断看门狗复位 (INT_WDT)";
    case ESP_RST_TASK_WDT: return "任务看门狗超时 (TASK_WDT)";
    case ESP_RST_WDT: return "看门狗复位 (WDT)";
    case ESP_RST_DEEPSLEEP: return "深度睡眠唤醒 (DEEPSLEEP)";
    case ESP_RST_BROWNOUT: return "供电电压不稳掉电复位 (BROWNOUT)";
    case ESP_RST_SDIO: return "SDIO复位 (SDIO)";
    default: return "其他/未知原因";
  }
}

static void handleLog() {
  if (!requireLogin()) return;
  uint32_t since = server.hasArg("since") ? static_cast<uint32_t>(strtoul(server.arg("since").c_str(), nullptr, 10)) : 0;
  const uint32_t latestId=nextLogId-1;
  if(since>latestId)since=0; // A still-open browser may retain a pre-reboot cursor.
  String json = "{\"latestId\":"+String(latestId)+",\"entries\":[";
  json.reserve(3072);
  bool first = true;
  size_t emitted = 0;
  const size_t oldest = (logWriteIndex + logCapacity - logCount) % logCapacity;
  for (size_t n = 0; n < logCount; ++n) {
    const LogEntry &entry = logEntries[(oldest + n) % logCapacity];
    if (entry.id <= since) continue;
    if (emitted >= LOG_BATCH_LIMIT) break;
    if (!first) json += ',';
    first = false;
    ++emitted;
    char timeText[20];
    const uint32_t seconds = entry.uptimeMs / 1000;
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu:%02lu.%03lu",
             static_cast<unsigned long>((seconds / 3600) % 100),
             static_cast<unsigned long>((seconds / 60) % 60),
             static_cast<unsigned long>(seconds % 60),
             static_cast<unsigned long>(entry.uptimeMs % 1000));
    json += "{\"id\":" + String(entry.id) + ",\"time\":\"" + timeText +
            "\",\"dir\":\"" + entry.direction + "\",\"hex\":" + jsonQuoted(entry.hex) + "}";
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

static bool parseHex(String input, uint8_t *output, size_t &length) {
  input.replace(',', ' ');
  input.replace(';', ' ');
  input.replace('\r', ' ');
  input.replace('\n', ' ');
  input.trim();
  length = 0;
  int pos = 0;
  while (pos < input.length()) {
    while (pos < input.length() && isspace(static_cast<unsigned char>(input[pos]))) ++pos;
    if (pos >= input.length()) break;
    int start = pos;
    while (pos < input.length() && !isspace(static_cast<unsigned char>(input[pos]))) ++pos;
    String token = input.substring(start, pos);
    if (token.startsWith("0x") || token.startsWith("0X")) token.remove(0, 2);
    if (token.length() == 0 || token.length() > 2 || length >= MAX_FRAME_BYTES) return false;
    for (size_t i = 0; i < token.length(); ++i) {
      if (!isHexadecimalDigit(token[i])) return false;
    }
    output[length++] = static_cast<uint8_t>(strtoul(token.c_str(), nullptr, 16));
  }
  return length > 0;
}

static void startMiotSerialPort() {
  pinMode(MIOT_RX_PIN, INPUT_PULLUP);
  MiotSerial.setRxBufferSize(2048);
  MiotSerial.setTxBufferSize(512);
  MiotSerial.begin(MIOT_BAUD, SERIAL_8N1, MIOT_RX_PIN, MIOT_TX_PIN);
  MiotSerial.onReceiveError([](hardwareSerial_error_t error) {
    if(error==UART_BUFFER_FULL_ERROR || error==UART_FIFO_OVF_ERROR) ++miotRxOverflows;
    else if(error==UART_FRAME_ERROR || error==UART_PARITY_ERROR || error==UART_BREAK_ERROR) ++miotSignalErrors;
  });
}

static void startBridgeSerialPorts() {
  Serial2.setRxBufferSize(2048);
  // Write directly into the TX FIFO. With a software TX ring buffer, write()
  // can return before the ISR starts the UART, so flush() can see a false idle.
  Serial2.setTxBufferSize(0);
  Serial2.begin(MODULE_BAUD, SERIAL_8N1, MODULE_RX_PIN, MODULE_TX_PIN);
  // INPUT disables the output while preserving the UART matrix signal.
  // Do not call gpio_set_direction(OUTPUT): it replaces that signal with GPIO.
  gpio_set_direction(static_cast<gpio_num_t>(MODULE_TX_PIN), GPIO_MODE_INPUT);
  gpio_set_pull_mode(static_cast<gpio_num_t>(MODULE_TX_PIN), GPIO_FLOATING);
  gpio_ll_set_output_enable_ctrl(&GPIO, MODULE_TX_PIN, false, false);
  gpio_ll_output_disable(&GPIO, MODULE_TX_PIN);
  startMiotSerialPort();
}

static bool mideaTxRouteReady() {
  return GPIO.func_out_sel_cfg[MODULE_TX_PIN].func_sel == U2TXD_OUT_IDX &&
         GPIO.func_out_sel_cfg[MODULE_TX_PIN].oen_sel &&
         !GPIO.func_out_sel_cfg[MODULE_TX_PIN].inv_sel &&
         !GPIO.func_out_sel_cfg[MODULE_TX_PIN].oen_inv_sel;
}

static void restoreIo16TxRoute() {
  gpio_set_direction(static_cast<gpio_num_t>(MODULE_TX_PIN),GPIO_MODE_INPUT);
  gpio_set_pull_mode(static_cast<gpio_num_t>(MODULE_TX_PIN),GPIO_FLOATING);
  esp_rom_gpio_connect_out_signal(MODULE_TX_PIN,U2TXD_OUT_IDX,false,false);
  gpio_ll_set_output_enable_ctrl(&GPIO,MODULE_TX_PIN,false,false);
  gpio_ll_output_disable(&GPIO,MODULE_TX_PIN);
}

static bool setIo16RxMode(bool enabled,String &message) {
  if(enabled==io16RxMode){message=enabled?"IO16 已处于独立 RX 模式。":"IO16 已处于 TX 模式。";return true;}
  if(enabled){
    if(uart_is_driver_installed(UART_NUM_0)){message="UART0 已被占用，无法启用 IO16 RX。";return false;}
    beginIo16RxLogStorage();io16RxMode=true;
    Serial2.flush();gpio_ll_output_disable(&GPIO,MODULE_TX_PIN);
    uart_config_t config{};config.baud_rate=MODULE_BAUD;config.data_bits=UART_DATA_8_BITS;
    config.parity=UART_PARITY_DISABLE;config.stop_bits=UART_STOP_BITS_1;
    config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;config.source_clk=UART_SCLK_DEFAULT;
    esp_err_t error=uart_driver_install(UART_NUM_0,2048,0,12,&io16RxQueue,0);
    const bool installed=error==ESP_OK;
    if(error==ESP_OK)error=uart_param_config(UART_NUM_0,&config);
    // IDF RX attachment avoids transferring Arduino's UART2 TX pin ownership.
    // UART2 keeps GPIO17 RX, and UART0 receives GPIO16 without any TX pin.
    if(error==ESP_OK)error=uart_set_pin(UART_NUM_0,UART_PIN_NO_CHANGE,MODULE_TX_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    if(error!=ESP_OK){
      if(installed)uart_driver_delete(UART_NUM_0);
      io16RxQueue=nullptr;io16RxMode=false;restoreIo16TxRoute();
      message="IO16 RX 初始化失败，已恢复 TX 模式。";return false;
    }
    gpio_set_direction(static_cast<gpio_num_t>(MODULE_TX_PIN),GPIO_MODE_INPUT);
    gpio_set_pull_mode(static_cast<gpio_num_t>(MODULE_TX_PIN),GPIO_PULLUP_ONLY);
    gpio_ll_output_disable(&GPIO,MODULE_TX_PIN);uart_flush_input(UART_NUM_0);
    io16RxFrameLength=0;
    if(usbFrameLength){addLog("TX",usbFrame,usbFrameLength);usbFrameLength=0;}
    if(timerPending){
      timerPending=false;timerMiotPending=false;timerResultState="error";
      timerResult="IO16 已切换 RX，尚未确认的倒计时设置已停止等待。";
    }
    miotBridge.setMideaControlEnabled(false);
    message="IO16 已切换为 RX（9600 8N1），向美的发送已暂停；IO17 继续接收。";
  }else{
    serviceIo16Rx();flushIo16RxFrame();
    if(uart_driver_delete(UART_NUM_0)!=ESP_OK){message="接收驱动关闭失败，IO16 仍保持 RX 模式，请重试。";return false;}
    io16RxQueue=nullptr;
    esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT,U0RXD_IN_IDX,false);
    restoreIo16TxRoute();io16RxMode=false;miotBridge.setMideaControlEnabled(true);
    scheduledQueryAt=millis()+200;
    message="IO16 已恢复为 TX；独立 RX 窗口保留已接收记录。";
  }
  addTextLog("SYS",message.c_str());return true;
}

static void addIo16RxStatus(cJSON *json) {
  cJSON_AddBoolToObject(json,"enabled",io16RxMode);
  cJSON_AddNumberToObject(json,"baud",MODULE_BAUD);
  cJSON_AddNumberToObject(json,"bytes",io16RxBytes);cJSON_AddNumberToObject(json,"frames",io16RxFrames);
  cJSON_AddNumberToObject(json,"overflows",io16RxOverflows);cJSON_AddNumberToObject(json,"signalErrors",io16RxSignalErrors);
  cJSON_AddNumberToObject(json,"capacity",io16RxLogCapacity);cJSON_AddNumberToObject(json,"latestId",nextIo16RxLogId-1);
}

static void handleIo16RxMode() {
  if(!requireLogin())return;
  const String body=server.arg("plain");
  if(body!="on" && body!="off"){server.send(400,"text/plain; charset=utf-8","请选择启用或关闭 IO16 RX。");return;}
  if(firmwareUpdateActive){server.send(409,"text/plain; charset=utf-8","固件更新中，请稍后切换。");return;}
  String message;const bool success=setIo16RxMode(body=="on",message);
  server.send(success?200:503,"text/plain; charset=utf-8",message);
}

static void handleIo16RxLog() {
  if(!requireLogin())return;
  const uint32_t since=server.hasArg("since")?uint32_t(strtoul(server.arg("since").c_str(),nullptr,10)):0;
  cJSON *state=cJSON_CreateObject();addIo16RxStatus(state);
  String json="{\"state\":"+compactJson(state)+",\"entries\":[";json.reserve(3072);
  size_t emitted=0;
  if(io16RxMode && io16RxLogs && io16RxLogCapacity){
    const size_t oldest=(io16RxLogIndex+io16RxLogCapacity-io16RxLogCount)%io16RxLogCapacity;
    for(size_t n=0;n<io16RxLogCount && emitted<LOG_BATCH_LIMIT;++n){
      const LogEntry &entry=io16RxLogs[(oldest+n)%io16RxLogCapacity];if(entry.id<=since)continue;
      if(emitted++)json+=',';
      char timeText[20];const uint32_t seconds=entry.uptimeMs/1000;
      snprintf(timeText,sizeof(timeText),"%02lu:%02lu:%02lu.%03lu",static_cast<unsigned long>((seconds/3600)%100),
        static_cast<unsigned long>((seconds/60)%60),static_cast<unsigned long>(seconds%60),static_cast<unsigned long>(entry.uptimeMs%1000));
      json+="{\"id\":"+String(entry.id)+",\"time\":\""+timeText+"\",\"dir\":\"IO16RX\",\"hex\":"+jsonQuoted(entry.hex)+"}";
    }
  }
  json+="]}";server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",json);
}

static void handleIo16RxLogClear() {
  if(!requireLogin())return;
  if(server.arg("plain")!="clear"){
    server.send(400,"text/plain; charset=utf-8","请使用 IO16 窗口的清空按钮。");return;
  }
  // Keep IDs monotonic so an old in-flight response cannot become new history.
  // Drop an unfinished pre-clear batch as well as the retained ring entries.
  io16RxFrameLength=0;io16RxLogCount=0;io16RxLogIndex=0;
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",
    "{\"latestId\":"+String(nextIo16RxLogId-1)+",\"message\":\"IO16 日志已清空。\"}");
}

static bool beginMideaTx() {
  if(io16RxMode){gpio_ll_output_disable(&GPIO,MODULE_TX_PIN);return false;}
  if (!mideaTxRouteReady()) {
    gpio_ll_output_disable(&GPIO, MODULE_TX_PIN);
    addTextLog("SYS", "UART2 TX 路由异常，未驱动 GPIO16；本次发送取消。");
    return false;
  }
  gpio_ll_output_enable(&GPIO, MODULE_TX_PIN);
  return true;
}

static void endMideaTx() {
  // The FIFO is already populated synchronously; wait through the final stop
  // bit, then clear only the pad output-enable bit. RX and routing stay intact.
  Serial2.flush();
  gpio_ll_output_disable(&GPIO, MODULE_TX_PIN);
}

// Keep the UART2 route permanently configured. Gate only GPIO16's output
// buffer, so it is a floating input between transmissions with no rerouting.
static bool sendMideaFrame(const uint8_t *data, size_t length) {
  if (!data || !length) return false;
  if (!beginMideaTx()) return false;
  const size_t written = Serial2.write(data, length);
  endMideaTx();
  lastMideaTxAt=millis();
  return written == length;
}

static void handleSend() {
  if (!requireLogin()) return;
  if(io16RxMode){server.send(409,"text/plain; charset=utf-8","IO16 RX 调试中，请先关闭接收模式再发送。");return;}
  if (setupApMode) {
    server.send(503, "text/plain; charset=utf-8", "Wi-Fi 未连接，发送接口暂不可用。");
    return;
  }
  uint8_t data[MAX_FRAME_BYTES];
  size_t length = 0;
  if (!parseHex(server.arg("plain"), data, length)) {
    server.send(400, "text/plain; charset=utf-8", "HEX 格式无效；每个字节请输入 1 到 2 位十六进制数。");
    return;
  }
  if (!sendMideaFrame(data, length)) {
    server.send(500, "text/plain; charset=utf-8", "美的串口发送失败。");
    return;
  }
  addLog("TX", data, length);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleMiotQuery() {
  if (!requireLogin()) return;
  if (setupApMode) {
    server.send(503, "text/plain; charset=utf-8", "Wi-Fi 未连接，米家模块查询暂不可用。");
    return;
  }
  String command = server.arg("plain");
  command.trim();
  if (!miotBridge.requestQuery(command)) {
    server.send(400, "text/plain; charset=utf-8", "仅支持 model/version/net/mac/time/get_down/sync；也请检查发送队列是否已满。");
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "已排队查询：" + command + "；执行结果请查看串口日志与同步状态。");
}

static void handleMiotTestEnvironment() {
  if (!requireLogin()) return;
  if (setupApMode) {
    server.send(503, "text/plain; charset=utf-8", "Wi-Fi 未连接，米家测试上报暂不可用。");
    return;
  }
  String message;
  if (!miotBridge.queueTestEnvironmentReport(message)) {
    server.send(409, "text/plain; charset=utf-8", message);
    return;
  }
  server.send(200, "text/plain; charset=utf-8", message);
}

static void handleMiotSimulation() {
  if (!requireLogin()) return;
  const String body=server.arg("plain");
  if(body!="on" && body!="off") {
    server.send(400,"text/plain; charset=utf-8","请选择启用或关闭米家扩展属性模拟。"); return;
  }
  const bool enabled=body=="on";
  if(enabled && strcmp(miotBridge.profileName(),"C20")) {
    server.send(409,"text/plain; charset=utf-8","请等待识别到 C20 模块后启用模拟。"); return;
  }
  if(!webPrefs.begin("midea_web",false)) {
    server.send(500,"text/plain; charset=utf-8","无法打开设置存储。"); return;
  }
  const size_t saved=webPrefs.putBool("miot_sim",enabled);
  webPrefs.end();
  if(!saved) {server.send(500,"text/plain; charset=utf-8","模拟开关保存失败。");return;}
  miotBridge.setSimulationEnabled(enabled);
  server.send(200,"text/plain; charset=utf-8",enabled?
    "已启用米家扩展属性模拟。室温无真实读数时为 23°C，湿度 75%，滤芯寿命 100%；基本空调控制仍以美的回包确认。":
    "已关闭米家扩展属性模拟。米家读写恢复为真实桥接策略。");
}

static void handleMockMode() {
  if (!requireLogin()) return;
  const String body = server.arg("plain");
  if (body != "on" && body != "off") {
    server.send(400, "text/plain; charset=utf-8", "请选择启用或关闭脱机模拟调试模式。");
    return;
  }
  const bool enabled = body == "on";
  if (!webPrefs.begin("midea_web", false)) {
    server.send(500, "text/plain; charset=utf-8", "无法打开设置存储。");
    return;
  }
  const size_t saved = webPrefs.putBool("mock_mode", enabled);
  webPrefs.end();
  if (!saved) { server.send(500, "text/plain; charset=utf-8", "脱机模拟开关保存失败。"); return; }
  mockMode = enabled;
  mideaTimers=MideaTimers();
  if(timerPending) {
    timerPending=false;timerResultState="error";
    timerResult="调试模式已切换，本次倒计时设置未确认。";
    if(timerMiotPending){timerMiotPending=false;miotBridge.resolveTimer(false);}
  }
  miotBridge.setMockMode(enabled);
  if (mockMode) {
    statusKnown = true;
    lastStatusAt = millis();
    if (!statusTemperature || statusTemperature < 16) statusTemperature = 26.0f;
    if (!statusMode) statusMode = 2;
    if (!statusFan) statusFan = 102;
    if (!roomTemperatureKnown) {
      roomTemperatureKnown = true;
      roomTemperature = 25.0f;
    }
    MideaAcSnapshot actual;
    actual.known = true;
    actual.receivedAt = millis();
    actual.power = statusPower;
    actual.auxHeat = statusAuxHeat;
    actual.mode = statusMode;
    actual.target = statusTemperature;
    actual.fan = statusFan;
    actual.swing = statusSwing;
    actual.eco = statusEco;
    actual.sleep = statusSleep;
    actual.displayOn = statusDisplayOn;
    actual.roomKnown = true;
    actual.room = roomTemperature;
    miotBridge.updateMidea(actual);
  }
  server.send(200, "text/plain; charset=utf-8", enabled ?
    "已启用脱机模拟调试。ESP32 不再向美的发送或确认，米家与 HomeKit 控制直接生效。" :
    "已关闭脱机模拟调试。恢复为真实美的空调通信。");
}

static uint8_t crc8Midea(const uint8_t *data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 1) ? static_cast<uint8_t>((crc >> 1) ^ 0x8C) : static_cast<uint8_t>(crc >> 1);
  }
  return crc;
}

static void sendStatusQuery() {
  if(io16RxMode)return;
  if (mockMode) {
    lastStatusAt = millis();
    return;
  }
  static const uint8_t query[] = {
    0xAA,0x21,0xAC,0x00,0x00,0x00,0x00,0x00,0x02,0x03,0x41,0x81,0x00,0xFF,0x03,0xFF,
    0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x37,0x2F
  };
  if (!sendMideaFrame(query, sizeof(query))) return;
  addLog("TX", query, sizeof(query));
  lastQueryAt = millis();
}

// Midea display control is a separate 0x41/0x61 toggle command. The 0x40
// set-state packet does not provide an absolute display on/off field.
static void sendDisplayToggle() {
  static uint8_t requestId = 0;
  uint8_t frame[34] = {
    0xAA,0x21,0xAC,0x8D,0x00,0x00,0x00,0x00,0x03,0x03,0x41,0x61,0x00,0xFF,0x02,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  };
  frame[31] = ++requestId;
  frame[11]=uint8_t((frame[11]&~0x40)|(miotBridge.promptToneEnabled()?0x40:0));
  frame[32] = crc8Midea(frame + 10, 22);
  uint8_t sum = 0;
  for (size_t i = 1; i < 33; ++i) sum = static_cast<uint8_t>(sum + frame[i]);
  frame[33] = static_cast<uint8_t>(0 - sum);
  if (!sendMideaFrame(frame, sizeof(frame))) return;
  addLog("TX", frame, sizeof(frame));
}

static void setDisplayFromMiot(bool displayOn) {
  if (displayOn != statusDisplayOn) sendDisplayToggle();
}

static bool sendSetStateWithFeatures(bool power, uint8_t mode, float temperature, uint8_t fan, uint8_t swing, bool eco, bool sleepMode, bool displayOn, const MideaTimers *timerUpdate, const bool *auxHeatUpdate) {
  if(io16RxMode)return false;
  if (mockMode) {
    statusKnown = true;
    lastStatusAt = millis();
    statusPower = power;
    statusMode = mode;
    statusTemperature = temperature;
    statusFan = fan;
    statusSwing = swing;
    statusEco = eco;
    statusSleep = sleepMode;
    statusDisplayOn = displayOn;
    if(auxHeatUpdate)statusAuxHeat=*auxHeatUpdate;
    if (!roomTemperatureKnown) {
      roomTemperatureKnown = true;
      roomTemperature = 25.0f;
    }
    MideaAcSnapshot actual;
    actual.known = true;
    actual.receivedAt = millis();
    actual.power = power;
    actual.mode = mode;
    actual.target = temperature;
    actual.fan = fan;
    actual.swing = swing;
    actual.eco = eco;
    actual.sleep = sleepMode;
    actual.displayOn = displayOn;
    actual.auxHeat = statusAuxHeat;
    actual.roomKnown = true;
    actual.room = roomTemperature;
    miotBridge.updateMidea(actual);
    return true;
  }
  // Match the original Meiju module's captured AA 24 / 0x40 envelope:
  // protocol 0, 24 body bytes, message ID, body CRC and outer checksum.
  // The old AA 22 packet omitted the extra body byte and message ID. Its
  // power setting was accepted, but that does not confirm audible feedback.
  static uint8_t requestId = 0;
  uint8_t frame[37] = {};
  frame[0]=0xAA;frame[1]=sizeof(frame)-1;frame[2]=0xAC;
  frame[9]=0x02;frame[10]=0x40;
  frame[14]=0x7F;frame[15]=0x7F;
  frame[29]=0x80; // Match the original module's byte 29; model-specific meaning unconfirmed.
  int halfDegrees = static_cast<int>(temperature * 2.0f + 0.5f);
  if (halfDegrees < 34) halfDegrees = 34; // 17.0 C
  if (halfDegrees > 60) halfDegrees = 60; // 30.0 C
  const int whole = halfDegrees / 2;
  const bool half = (halfDegrees & 1) != 0;
  // Keep required bit 1 and power bit 0; beep bit 6 follows the last confirmed
  // hardware buzzer switch, so normal controls do not re-enable muted sound.
  frame[11] = static_cast<uint8_t>((power ? 0x01 : 0x00) | 0x02 | (miotBridge.promptToneEnabled()?0x40:0));
  frame[12] = static_cast<uint8_t>((mode << 5) | (half ? 0x10 : 0x00) | (whole - 16));
  frame[13] = fan;
  frame[17] = static_cast<uint8_t>(0x30 | (swing & 0x0F));
  frame[18] = fan == 100 ? 0x20 : 0x00;  // alternate Turbo flag
  frame[19] = eco ? 0x80 : 0x00;        // set-state uses bit 7; status uses bit 4
  // Display is controlled by the separate 0x41/0x61 toggle command below;
  // bit 4 in this full-state frame is not an absolute display setting.
  frame[20] = static_cast<uint8_t>((sleepMode ? 0x01 : 0x00) | (fan == 100 ? 0x02 : 0x00));
  mideaCommandFlags.applyTo(frame);
  if(auxHeatUpdate)MideaCommandFlags::applyAuxHeat(frame,*auxHeatUpdate);
  // Ordinary controls carry the last timer bytes without the timer-write flag.
  // Only an explicit countdown request sets bit 7 of the fan byte.
  (timerUpdate?*timerUpdate:mideaTimers).applyTo(frame,timerUpdate!=nullptr);
  frame[sizeof(frame)-3] = ++requestId;
  frame[sizeof(frame)-2] = crc8Midea(frame + 10, sizeof(frame)-12);
  uint8_t sum = 0;
  for (size_t i = 1; i < sizeof(frame)-1; ++i) sum = static_cast<uint8_t>(sum + frame[i]);
  frame[sizeof(frame)-1] = static_cast<uint8_t>(0 - sum);
  if (!sendMideaFrame(frame, sizeof(frame))) return false;
  addLog("TX", frame, sizeof(frame));
  if (displayOn != statusDisplayOn) sendDisplayToggle();
  return true;
}

static bool sendSetStateWithTimers(bool power, uint8_t mode, float temperature, uint8_t fan, uint8_t swing, bool eco, bool sleepMode, bool displayOn, const MideaTimers *timerUpdate) {
  return sendSetStateWithFeatures(power,mode,temperature,fan,swing,eco,sleepMode,displayOn,timerUpdate,nullptr);
}

static bool setAuxHeatFromMiot(const MideaAcSnapshot &state) {
  return sendSetStateWithFeatures(state.power,state.mode,state.target,state.fan,state.swing,state.eco,state.sleep,state.displayOn,nullptr,&state.auxHeat);
}

static bool sendMideaSwitchCommand(uint16_t property,bool query,bool enabled) {
  uint8_t frame[19];if(++selfCleanSequence==255)selfCleanSequence=1;
  const size_t length=buildMideaSwitchPropertyFrame(frame,sizeof(frame),property,query,enabled,selfCleanSequence);
  if(!length || !sendMideaFrame(frame,length))return false;
  addLog("TX",frame,length);return true;
}

static bool setSelfCleanFromMiot(bool enabled) {
  if(mockMode || timerPending || !statusKnown || uint32_t(millis()-lastStatusAt)>15000)return false;
  if(!sendMideaSwitchCommand(0x0039,false,enabled))return false;
  nextSelfCleanQueryAt=millis()+300;
  addTextLog("SYS",enabled?"米家自清洁启动已发送，等待美的状态查询确认。":"米家自清洁取消已发送，等待美的状态查询确认。");
  return true;
}

static bool setBuzzerFromMiot(bool enabled) {
  if(mockMode || timerPending || !statusKnown || uint32_t(millis()-lastStatusAt)>15000)return false;
  if(!sendMideaSwitchCommand(0x022C,false,enabled))return false;
  nextBuzzerQueryAt=millis()+300;
  addTextLog("SYS",enabled?"米家提示音开启已发送，等待美的状态查询确认。":"米家提示音关闭已发送，等待美的状态查询确认。");
  return true;
}

static void serviceMideaFeatureQueries() {
  const uint32_t now=millis();
  if(io16RxMode || mockMode || !statusKnown || timerPending ||
     uint32_t(now-lastMideaTxAt)<200 || uint32_t(now-lastRxByteMs)<50)return;
  const bool buzzerDue=int32_t(now-nextBuzzerQueryAt)>=0,cleanDue=int32_t(now-nextSelfCleanQueryAt)>=0;
  if(buzzerDue && (miotBridge.buzzerPending() || !cleanDue || !miotBridge.selfCleanPending())){
    nextBuzzerQueryAt=now+(miotBridge.buzzerPending()?1500:5000);sendMideaSwitchCommand(0x022C,true,false);
  }else if(cleanDue){
    nextSelfCleanQueryAt=now+(miotBridge.selfCleanPending()?1500:5000);sendMideaSwitchCommand(0x0039,true,false);
  }
}

static void sendSetState(bool power, uint8_t mode, float temperature, uint8_t fan, uint8_t swing, bool eco, bool sleepMode, bool displayOn) {
  sendSetStateWithTimers(power,mode,temperature,fan,swing,eco,sleepMode,displayOn,nullptr);
}

// C20 writes use "enabled,minutes,targetPower": 0 = OFF, 1 = ON.
// Captured strings include "1,5,1" (five minutes until ON) and
// the scalar "0" used to disable the property. The latter clears both native timers.
static bool setCountdownFromMiot(bool enabled,bool targetPower,uint16_t minutes) {
  const uint32_t now=millis();
  if(mockMode || timerPending || !statusKnown || uint32_t(now-lastStatusAt)>15000 ||
     !mideaTimers.known || uint32_t(now-mideaTimers.receivedAt)>15000) {
    sendStatusQuery();return false;
  }
  if(mideaTimers.absolute && (mideaTimers.on || mideaTimers.off))return false;
  const uint16_t onMinutes=!enabled?0:(targetPower?minutes:mideaTimers.remaining(true,now));
  const uint16_t offMinutes=!enabled?0:(targetPower?mideaTimers.remaining(false,now):minutes);
  requestedTimers.setRelative(onMinutes,offMinutes,now);
  if(!sendSetStateWithTimers(statusPower,statusMode,statusTemperature,statusFan,statusSwing,statusEco,statusSleep,statusDisplayOn,&requestedTimers))return false;
  ++timerRequestId;timerSentAt=millis();timerNextQueryAt=timerSentAt+300;
  timerPending=true;timerMiotPending=true;timerResultState="pending";
  timerResult=enabled?(String(targetPower?"米家倒计时：开机 ":"米家倒计时：关机 ")+String(minutes)+" 分钟，等待美的回包确认。"):
    String("米家倒计时取消，等待美的回包确认。");
  addTextLog("SYS",timerResult.c_str());
  return true;
}

static bool getCountdownForMiot(String &value) {
  const uint32_t now=millis();
  if(mockMode || !statusKnown || uint32_t(now-lastStatusAt)>15000 ||
     !mideaTimers.known || uint32_t(now-mideaTimers.receivedAt)>15000 ||
     (mideaTimers.absolute && (mideaTimers.on || mideaTimers.off)))return false;
  bool enabled=false,targetPower=statusPower;uint16_t minutes=0;
  const uint16_t onMinutes=mideaTimers.on?mideaTimers.remaining(true,now):0;
  const uint16_t offMinutes=mideaTimers.off?mideaTimers.remaining(false,now):0;
  if(onMinutes && (!offMinutes || onMinutes<=offMinutes)){enabled=true;targetPower=true;minutes=onMinutes;}
  else if(offMinutes){enabled=true;targetPower=false;minutes=offMinutes;}
  // Keep the display encoding verified from C0 feedback independently of writes.
  value=String(enabled?1:0)+","+String(minutes)+","+String(enabled && !targetPower?1:0);
  return true;
}

static bool timerValueMatches(bool wantedEnabled,uint16_t wantedMinutes,bool actualEnabled,uint16_t actualMinutes) {
  if(wantedEnabled!=actualEnabled)return false;
  // Minute counters can cross a boundary between sending and receiving.
  return !wantedEnabled || (actualMinutes<=wantedMinutes && wantedMinutes-actualMinutes<=1);
}

static void confirmMideaTimers() {
  if(!timerPending || !mideaTimers.known || mideaTimers.absolute)return;
  // A C0 burst queued before this write must not confirm the new request.
  if(int32_t(lastRxByteMs-timerSentAt)<=0 || uint32_t(millis()-timerSentAt)>=8000)return;
  if(!timerValueMatches(requestedTimers.on,requestedTimers.onMinutes,mideaTimers.on,mideaTimers.onMinutes) ||
     !timerValueMatches(requestedTimers.off,requestedTimers.offMinutes,mideaTimers.off,mideaTimers.offMinutes))return;
  timerPending=false;timerResultState="confirmed";
  timerResult="空调回包已确认倒计时设置。";
  addTextLog("SYS",timerResult.c_str());
  if(timerMiotPending){timerMiotPending=false;miotBridge.resolveTimer(true);}
}

static void serviceMideaTimers() {
  if(!timerPending)return;
  const uint32_t now=millis();
  if(uint32_t(now-timerSentAt)>=8000) {
    timerPending=false;timerResultState="timeout";
    timerResult="8 秒内未收到匹配的倒计时回包；设置未确认，请查看空调回报。";
    addTextLog("SYS",timerResult.c_str());
    if(timerMiotPending){timerMiotPending=false;miotBridge.resolveTimer(false);}
  } else if(int32_t(now-timerNextQueryAt)>=0) {
    timerNextQueryAt=now+1500;
    sendStatusQuery();
  }
}

static void handleTimers() {
  if(!requireLogin())return;
  if(io16RxMode){server.send(409,"text/plain; charset=utf-8","IO16 RX 调试中，倒计时发送已暂停。");return;}
  if(mockMode){server.send(409,"text/plain; charset=utf-8","请先关闭脱机模拟，再查询或设置空调倒计时。");return;}
  const String body=server.arg("plain");
  cJSON *json=cJSON_Parse(body.c_str());
  const cJSON *target=cJSON_GetObjectItemCaseSensitive(json,"target");
  const cJSON *minutes=cJSON_GetObjectItemCaseSensitive(json,"minutes");
  const bool valid=cJSON_IsObject(json) && cJSON_IsString(target) &&
      (!strcmp(target->valuestring,"on") || !strcmp(target->valuestring,"off")) &&
      cJSON_IsNumber(minutes) && isfinite(minutes->valuedouble) &&
      minutes->valuedouble>=0 && minutes->valuedouble<=1440 &&
      minutes->valuedouble==floor(minutes->valuedouble);
  if(!valid){cJSON_Delete(json);server.send(400,"text/plain; charset=utf-8","倒计时须为整数分钟，范围 1 分钟至 24 小时；0 表示取消。");return;}
  const bool turnOn=!strcmp(target->valuestring,"on");
  const uint16_t delayMinutes=uint16_t(minutes->valueint);
  cJSON_Delete(json);
  const uint32_t now=millis();
  if(timerPending || miotBridge.pending()){server.send(409,"text/plain; charset=utf-8","正在等待空调确认上一条设置，请稍后重试。");return;}
  if(!statusKnown || uint32_t(now-lastStatusAt)>15000 || !mideaTimers.known || uint32_t(now-mideaTimers.receivedAt)>15000) {
    sendStatusQuery();server.send(409,"text/plain; charset=utf-8","已查询最新状态，请收到回包后重试。");return;
  }
  if(mideaTimers.absolute && (mideaTimers.on || mideaTimers.off)) {
    server.send(409,"text/plain; charset=utf-8","空调当前回报时刻定时；请先用原控制器取消，再设置倒计时。");return;
  }
  // Preserve the other countdown, using its most recent remaining minutes.
  const uint16_t onMinutes=turnOn?delayMinutes:mideaTimers.remaining(true,now);
  const uint16_t offMinutes=turnOn?mideaTimers.remaining(false,now):delayMinutes;
  requestedTimers.setRelative(onMinutes,offMinutes,now);
  if(!sendSetStateWithTimers(statusPower,statusMode,statusTemperature,statusFan,statusSwing,statusEco,statusSleep,statusDisplayOn,&requestedTimers)) {
    server.send(503,"text/plain; charset=utf-8","串口发送失败，倒计时未发送。");return;
  }
  ++timerRequestId;timerSentAt=millis();timerNextQueryAt=timerSentAt+300;
  timerPending=true;timerResultState="pending";
  timerResult=String(turnOn?"开机":"关机")+(delayMinutes?"倒计时设置":"倒计时取消")+"已发送，等待空调回包确认。";
  addTextLog("SYS",timerResult.c_str());
  server.send(202,"text/plain; charset=utf-8",timerResult);
}

static uint8_t homeKitTargetMode() {
  if (!statusPower) return 0;                 // OFF
  if (statusMode == 4) return 1;              // HEAT
  if (statusMode == 2 || statusMode == 3) return 2; // COOL; DRY is closest native mode
  return 3;                                   // AUTO; also represents fan-only natively
}

static uint8_t homeKitCurrentMode() {
  if (!statusPower) return 0;                 // IDLE
  if (statusMode == 4) return 1;              // HEATING
  if (statusMode == 2 || statusMode == 3) return 2; // COOLING
  return 0;
}

class MideaHomeKitThermostat : public Service::Thermostat {
 public:
  MideaHomeKitThermostat() : Service::Thermostat() {
    hkCurrentState = new Characteristic::CurrentHeatingCoolingState(homeKitCurrentMode());
    hkTargetState = new Characteristic::TargetHeatingCoolingState(homeKitTargetMode());
    hkCurrentTemperature = new Characteristic::CurrentTemperature(statusTemperature);
    hkTargetTemperature = (new Characteristic::TargetTemperature(statusTemperature))->setRange(17, 30, 0.5);
    new Characteristic::TemperatureDisplayUnits(0);  // Celsius
    new Characteristic::ConfiguredName("空调");
  }

  boolean update() override {
    bool power = statusPower;
    uint8_t mode = statusMode;
    float temperature = statusTemperature;
    bool changed = false;

    if (hkTargetState->updated()) {
      const uint8_t requested = hkTargetState->getNewVal<uint8_t>();
      if (requested == 0) power = false;
      else {
        power = true;
        if (requested == 1) mode = 4;       // HEAT
        else if (requested == 2) mode = 2;  // COOL
        else if (requested == 3) mode = 1;  // AUTO
      }
      changed = true;
    }
    if (hkTargetTemperature->updated()) {
      temperature = hkTargetTemperature->getNewVal<float>();
      changed = true;
    }
    if (!changed) return true;
    if (mockMode) {
      statusKnown = true;
      lastStatusAt = millis();
    } else if (miotBridge.pending() || !statusKnown || millis() - lastStatusAt > 15000) return false;

    sendSetState(power, mode, temperature, statusFan, statusSwing, statusEco, statusSleep, statusDisplayOn);
    if (!mockMode) scheduledQueryAt = millis() + 300;
    return true;
  }

  void loop() override {
    static uint32_t lastRefresh = 0;
    if (static_cast<uint32_t>(millis() - lastRefresh) < 300) return;
    lastRefresh = millis();
    if (hkCurrentState) hkCurrentState->setVal(homeKitCurrentMode());
    if (hkTargetState) hkTargetState->setVal(homeKitTargetMode());
    // No valid room-temperature field has been observed in this unit's replies.
    // Use the setpoint as a clearly disclosed temporary placeholder until a sensor is added.
    const float current = roomTemperatureKnown ? roomTemperature : statusTemperature;
    if (hkCurrentTemperature) hkCurrentTemperature->setVal(current);
    if (hkTargetTemperature) hkTargetTemperature->setVal(statusTemperature);
  }
};

static void serviceScheduledQuery() {
  if (io16RxMode || miotBridge.pending()) return;
  if (scheduledQueryAt && static_cast<int32_t>(millis() - scheduledQueryAt) >= 0) {
    scheduledQueryAt = 0;
    sendStatusQuery();
  }
}

static bool executeAcCommand(const String &command, float requestedTemperature, String &message) {
  if(io16RxMode){message="IO16 RX 调试中，请先关闭接收模式恢复向美的发送。";return false;}
  if (command == "query") {
    sendStatusQuery();
    message = mockMode ? "脱机模拟：状态时间戳已刷新。" : "状态查询已发送，等待空调回包。";
    return true;
  }
  if (!statusKnown || static_cast<uint32_t>(millis() - lastStatusAt) > 30000) {
    if (mockMode) {
      statusKnown = true;
      lastStatusAt = millis();
    } else {
      sendStatusQuery();
      message = "尚无新鲜的空调状态，已查询；收到回包后请重试。";
      return false;
    }
  }
  if (!mockMode && miotBridge.pending()) {
    message = "正在等待美的确认米家控制，请稍后重试。";
    return false;
  }
  bool power = statusPower;
  uint8_t mode = statusMode;
  float temperature = statusTemperature;
  uint8_t fan = statusFan;
  uint8_t swing = statusSwing;
  bool eco = statusEco;
  bool sleepMode = statusSleep;
  if (command == "power_on") power = true;
  else if (command == "power_off") power = false;
  else if (command == "mode_next") {
    mode = mode >= 5 ? 1 : static_cast<uint8_t>(mode + 1);
  } else if (command == "mode_auto") mode = 1;
  else if (command == "mode_cool") mode = 2;
  else if (command == "mode_dry") mode = 3;
  else if (command == "mode_heat") mode = 4;
  else if (command == "mode_fan") mode = 5;
  else if (command == "temperature") {
    if (!isfinite(requestedTemperature) || requestedTemperature < 17 || requestedTemperature > 30) {
      message = "温度须为 17–30°C。";
      return false;
    }
    temperature = roundf(requestedTemperature * 2) * 0.5f;
  } else if (command == "temp_up") temperature += 0.5f;
  else if (command == "temp_down") temperature -= 0.5f;
  else if (command == "fan_next") {
    if (fan >= 101) fan = 20;
    else if (fan < 40) fan = 40;
    else if (fan < 60) fan = 60;
    else if (fan < 80) fan = 80;
    else if (fan < 100) fan = 100;
    else fan = 102;
  }
  else if (command == "fan_auto") fan = 102;
  else if (command == "fan_silent") fan = 20;
  else if (command == "fan_low") fan = 40;
  else if (command == "fan_medium") fan = 60;
  else if (command == "fan_high") fan = 80;
  else if (command == "fan_turbo") fan = 100;
  else if (command == "swing_toggle") swing = (swing & 0x0C) ? (swing & 0x03) : (swing | 0x0C);
  else if (command == "swing_vertical_on") swing |= 0x0C;
  else if (command == "swing_vertical_off") swing &= 0x03;
  else if (command == "swing_horizontal_on") swing |= 0x03;
  else if (command == "swing_horizontal_off") swing &= 0x0C;
  else if (command == "eco_toggle") eco = !eco;
  else if (command == "eco_on") eco = true;
  else if (command == "eco_off") eco = false;
  else if (command == "sleep_toggle") sleepMode = !sleepMode;
  else if (command == "sleep_on") sleepMode = true;
  else if (command == "sleep_off") sleepMode = false;
  else { message = "未知空调命令。"; return false; }
  if (temperature < 17.0f) temperature = 17.0f;
  if (temperature > 30.0f) temperature = 30.0f;
  sendSetState(power, mode, temperature, fan, swing, eco, sleepMode, statusDisplayOn);
  if (!mockMode) scheduledQueryAt = millis() + 300;
  message = mockMode ? "【脱机模拟】控制已在内存中生效。" : "控制指令已发送，等待空调状态回报。";
  return true;
}

static void handleControl() {
  if (!requireLogin()) return;
  String command = server.arg("plain");
  command.trim();
  String message;
  const bool accepted = executeAcCommand(command, NAN, message);
  server.send(accepted ? 200 : 409, "text/plain; charset=utf-8", message);
}

static String buildDeviceStatus(bool includeRaw) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "type", "status");
  cJSON_AddNumberToObject(json, "protocol", 1);
  cJSON_AddStringToObject(json, "firmware", BRIDGE_VERSION);
  wifiHealth.addStatus(cJSON_AddObjectToObject(json,"wifiHealth"));
  addIo16RxStatus(cJSON_AddObjectToObject(json,"io16Rx"));
  cJSON_AddStringToObject(json, "deviceName", bridgeBleName().c_str());
  cJSON_AddBoolToObject(json, "bleEnabled", bleEnabled);
  cJSON_AddBoolToObject(json, "known", statusKnown);
  cJSON_AddBoolToObject(json, "fresh", statusKnown && static_cast<uint32_t>(millis() - lastStatusAt) <= 30000);
  cJSON_AddBoolToObject(json, "power", statusPower);
  cJSON_AddNumberToObject(json, "mode", statusMode);
  cJSON_AddNumberToObject(json, "temp", statusTemperature);
  cJSON_AddNumberToObject(json, "fan", statusFan);
  addFanMappingStatus(cJSON_AddObjectToObject(json,"fanMapping"));
  cJSON_AddBoolToObject(json, "swing", (statusSwing & 0x0C) != 0);
  cJSON_AddBoolToObject(json, "horizontalSwing", (statusSwing & 0x03) != 0);
  cJSON_AddBoolToObject(json, "eco", statusEco);
  cJSON_AddBoolToObject(json, "sleep", statusSleep);
  cJSON_AddBoolToObject(json, "displayOn", statusDisplayOn);
  cJSON_AddBoolToObject(json, "auxHeat", statusAuxHeat);
  cJSON *timers=cJSON_AddObjectToObject(json,"timers");
  cJSON_AddBoolToObject(timers,"known",!mockMode && mideaTimers.known);
  cJSON_AddBoolToObject(timers,"fresh",!mockMode && mideaTimers.known && uint32_t(millis()-mideaTimers.receivedAt)<=15000);
  cJSON_AddBoolToObject(timers,"absolute",mideaTimers.absolute);
  cJSON_AddBoolToObject(timers,"on",mideaTimers.on);
  cJSON_AddBoolToObject(timers,"off",mideaTimers.off);
  cJSON_AddNumberToObject(timers,"onMinutes",mideaTimers.onMinutes);
  cJSON_AddNumberToObject(timers,"offMinutes",mideaTimers.offMinutes);
  cJSON_AddNumberToObject(timers,"age",mideaTimers.known?uint32_t(millis()-mideaTimers.receivedAt)/1000:0);
  cJSON_AddBoolToObject(timers,"pending",timerPending);
  cJSON_AddNumberToObject(timers,"requestId",timerRequestId);
  cJSON_AddStringToObject(timers,"resultState",timerResultState);
  cJSON_AddStringToObject(timers,"result",timerResult.c_str());
  cJSON_AddBoolToObject(json, "roomKnown", roomTemperatureKnown);
  cJSON_AddNumberToObject(json, "room", roomTemperature);
  cJSON_AddNumberToObject(json, "age", statusKnown ? (millis() - lastStatusAt) / 1000 : 0);
  cJSON_AddBoolToObject(json, "homekit", homeKitPaired);
  // Pairing information is returned only by the authenticated HTTP or BLE status routes.
  cJSON_AddStringToObject(json, "homekitCode", HOMEKIT_SETUP_CODE);
  cJSON_AddBoolToObject(json, "wifiConnected", connected);
  cJSON_AddStringToObject(json, "ssid", connected ? WiFi.SSID().c_str() : configuredWifiSsid.c_str());
  cJSON_AddStringToObject(json, "ip", connected ? WiFi.localIP().toString().c_str() : "");
  cJSON_AddStringToObject(json, "networkHostname", networkHostname.c_str());
  cJSON_AddStringToObject(json, "mdnsHostname", mdnsHostname.c_str());
  cJSON_AddStringToObject(json, "mac", WiFi.macAddress().c_str());
  cJSON_AddStringToObject(json, "url", networkHostnameUrl(mdnsHostname).c_str());
  cJSON_AddStringToObject(json, "ipUrl", connected ? ("http://" + WiFi.localIP().toString() + "/").c_str() : "");
  cJSON_AddNumberToObject(json, "rssi", connected ? WiFi.RSSI() : 0);
  cJSON_AddBoolToObject(json, "bleConnected", bridgeBleConnected());
  cJSON_AddBoolToObject(json, "miotSeen", miotBridge.seen(millis()));
  cJSON_AddStringToObject(json, "miotModel", miotBridge.model().c_str());
  cJSON_AddStringToObject(json, "miotNet", miotBridge.network().c_str());
  cJSON_AddStringToObject(json, "miotProfile", miotBridge.profileName());
  cJSON_AddNumberToObject(json, "miotMode", miotBridge.displayMode());
  cJSON_AddBoolToObject(json, "miotModeEstimated", miotBridge.modeEstimated());
  cJSON_AddStringToObject(json, "miotVersion", miotBridge.version().c_str());
  cJSON_AddStringToObject(json, "miotResult", miotBridge.lastResult().c_str());
  cJSON_AddBoolToObject(json, "miotInitialized", miotBridge.initialized());
  cJSON_AddBoolToObject(json, "miotPending", miotBridge.pending());
  cJSON_AddNumberToObject(json, "miotReports", miotBridge.reports());
  cJSON_AddNumberToObject(json, "miotReportAge", miotBridge.reportAge(millis()));
  cJSON_AddNumberToObject(json, "miotNetAge", miotBridge.netAge(millis()));
  cJSON_AddNumberToObject(json, "miotConfirmed", miotBridge.confirmed());
  cJSON_AddNumberToObject(json, "miotFailures", miotBridge.failures());
  cJSON_AddNumberToObject(json, "miotTimeouts", miotBridge.timeouts());
  cJSON_AddNumberToObject(json, "miotResponseMs", miotBridge.responseMs());
  cJSON_AddNumberToObject(json, "miotMaxResponseMs", miotBridge.maxResponseMs());
  cJSON_AddNumberToObject(json, "miotMaxServiceGapMs", miotBridge.maxServiceGapMs());
  cJSON_AddNumberToObject(json, "miotSlowServiceGaps", miotBridge.slowServiceGaps());
  cJSON_AddNumberToObject(json, "miotMaxReplyQueueMs", miotBridge.maxReplyQueueMs());
  cJSON_AddNumberToObject(json, "miotUnexpectedReplies", miotBridge.unexpectedReplies());
  cJSON_AddStringToObject(json, "miotLastTimeoutCommand", miotBridge.lastTimeoutCommand().c_str());
  cJSON_AddNumberToObject(json, "miotRxOverflows", miotRxOverflows);
  cJSON_AddNumberToObject(json, "miotSignalErrors", miotSignalErrors);
  cJSON_AddNumberToObject(json, "webMaxHandleMs", webMaxHandleMs);
  cJSON_AddNumberToObject(json, "homekitMaxPollMs", homekitMaxPollMs);
  cJSON_AddNumberToObject(json, "webInputTimeouts", server.inputTimeouts());
  cJSON_AddNumberToObject(json, "webWriteTimeouts", server.writeTimeouts());
  cJSON_AddNumberToObject(json, "miotOtaRequests", miotBridge.otaRequests());
  cJSON_AddBoolToObject(json, "miotSimulationEnabled", miotBridge.simulationEnabled());
  cJSON_AddBoolToObject(json, "miotSimulationActive", miotBridge.simulationActive());
  cJSON_AddNumberToObject(json, "miotSimulatedControls", miotBridge.simulatedControls());
  cJSON_AddBoolToObject(json, "mockMode", mockMode);
  cJSON_AddNumberToObject(json, "mideaValidFrames", mideaValidFrames);
  cJSON_AddNumberToObject(json, "mideaRejectedFrames", mideaRejectedFrames);
  cJSON_AddBoolToObject(json, "mideaTxRouteReady", mideaTxRouteReady());
  cJSON_AddBoolToObject(json, "mideaTxDriving", (GPIO.enable & (1UL << MODULE_TX_PIN)) != 0);
  cJSON_AddNumberToObject(json, "freeHeap", ESP.getFreeHeap());
  if (includeRaw) cJSON_AddStringToObject(json, "raw", statusRaw.c_str());
  return compactJson(json);
}

static void handleStatus() {
  if (!requireLogin()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", buildDeviceStatus(true));
}

static void handleSystemStatus() {
  if (!requireLogin()) return;
  server.sendHeader("Cache-Control", "no-store");
  cJSON *metrics = buildSystemMetrics();
  cJSON *logs = cJSON_AddObjectToObject(metrics, "logBuffer");
  cJSON_AddNumberToObject(logs, "capacity", logCapacity);
  cJSON_AddNumberToObject(logs, "entries", logCount);
  cJSON_AddNumberToObject(logs, "allocatedBytes", logCapacity * sizeof(LogEntry));
  cJSON_AddBoolToObject(logs, "inPsram", logStorageInPsram);
  server.send(200, "application/json; charset=utf-8", compactJson(metrics));
}

static void loadNetworkCredentials() {
  configuredWifiSsid = WIFI_SSID;
  configuredWifiPassword = WIFI_PASSWORD;
  Preferences prefs;
  if (!prefs.begin("midea_net", true)) return;
  const String saved = prefs.getString("credentials", "");
  prefs.end();
  if (!saved.length()) return;
  cJSON *json = cJSON_Parse(saved.c_str());
  if (!json) return;
  const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
  const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
  if (cJSON_IsString(ssid) && strlen(ssid->valuestring) > 0 && strlen(ssid->valuestring) <= 32 &&
      cJSON_IsString(password) && strlen(password->valuestring) <= 64) {
    configuredWifiSsid = ssid->valuestring;
    configuredWifiPassword = password->valuestring;
  }
  cJSON_Delete(json);
}

static bool saveNetworkCredentials(const char *ssid, const char *password, String &message) {
  const size_t ssidLength = strlen(ssid);
  const size_t passwordLength = strlen(password);
  bool hexPsk = passwordLength == 64;
  for (size_t i = 0; i < passwordLength && hexPsk; ++i) hexPsk = isxdigit(static_cast<unsigned char>(password[i]));
  if (!ssidLength || ssidLength > 32 ||
      !(passwordLength == 0 || (passwordLength >= 8 && passwordLength <= 63) || hexPsk)) {
    message = "SSID 须为 1–32 字节；密码须为空、8–63 字节或 64 位十六进制 PSK。";
    return false;
  }
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "ssid", ssid);
  cJSON_AddStringToObject(json, "password", password);
  const String encoded = compactJson(json);
  Preferences prefs;
  if (!prefs.begin("midea_net", false)) { message = "无法打开网络设置。"; return false; }
  const size_t written = prefs.putString("credentials", encoded);
  prefs.end();
  if (!written) { message = "网络配置保存失败。"; return false; }
  // Reinitialize HomeSpan's network on reboot while preserving HAP/NVS pairing.
  scheduledRestartAt = millis() + 3000;
  message = "Wi-Fi 设置已保存，ESP32 将重启并连接新网络；请等待蓝牙重新连接。";
  return true;
}

static void sendBleReply(const char *type, const char *op, int id, bool ok, const String &message) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "type", type);
  cJSON_AddStringToObject(json, "op", op);
  cJSON_AddNumberToObject(json, "id", id);
  cJSON_AddBoolToObject(json, "ok", ok);
  cJSON_AddStringToObject(json, "message", message.c_str());
  if (ok && !strcmp(op, "wifi")) cJSON_AddBoolToObject(json, "restart", true);
  bridgeBleSend(compactJson(json));
}

static bool bleWifiScanPending = false;
static int bleWifiScanRequestId = 0;
static uint32_t bleWifiScanSession = 0;

static void serviceBleWifiScan() {
  if (!bleWifiScanPending) return;
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;
  if (bridgeBleConnected() && bleAuthenticated && bleWifiScanSession == bridgeBleSession()) {
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "type", "wifi_scan");
    cJSON_AddStringToObject(reply, "op", "wifi_scan");
    cJSON_AddNumberToObject(reply, "id", bleWifiScanRequestId);
    cJSON_AddBoolToObject(reply, "ok", count >= 0);
    cJSON_AddStringToObject(reply, "message", count >= 0 ? "扫描完成。" : "Wi-Fi 扫描失败，请重试。");
    cJSON *networks = cJSON_AddArrayToObject(reply, "networks");
    for (int16_t i = 0; count >= 0 && i < count && i < 20; ++i) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      cJSON *network = cJSON_CreateObject();
      cJSON_AddStringToObject(network, "ssid", ssid.c_str());
      cJSON_AddNumberToObject(network, "rssi", WiFi.RSSI(i));
      cJSON_AddBoolToObject(network, "secure", WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      cJSON_AddItemToArray(networks, network);
    }
    // compactJson takes ownership of reply and deletes it after serialization.
    bridgeBleSend(compactJson(reply));
  }
  WiFi.scanDelete();
  bleWifiScanPending = false;
}

static void serviceBleCommands() {
  if (!bleAvailable) return;
  bridgeBlePoll();
  if (bleSessionSeen != bridgeBleSession()) {
    bleSessionSeen = bridgeBleSession();
    bleAuthenticated = false;
    lastBleStatusAt = 0;
  }
  String line;
  while (bridgeBleTakeLine(line)) {
    cJSON *json = cJSON_Parse(line.c_str());
    if (!cJSON_IsObject(json)) {
      if (json) cJSON_Delete(json);
      sendBleReply("error", "", 0, false, "无效 JSON 请求。");
      continue;
    }
    const cJSON *operation = cJSON_GetObjectItemCaseSensitive(json, "op");
    const cJSON *requestId = cJSON_GetObjectItemCaseSensitive(json, "id");
    const int id = cJSON_IsNumber(requestId) ? requestId->valueint : 0;
    const char *op = cJSON_IsString(operation) ? operation->valuestring : "";
    if (!strcmp(op, "auth")) {
      const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
      if (bleAuthBlockedUntil && static_cast<int32_t>(millis() - bleAuthBlockedUntil) < 0) {
        sendBleReply("auth", op, id, false, "尝试过多，请 30 秒后重试。");
      } else if (cJSON_IsString(password) && webPassword == password->valuestring) {
        bleAuthenticated = true;
        bleAuthFailures = 0;
        bleAuthBlockedUntil = 0;
        sendBleReply("auth", op, id, true, "已连接并通过认证。");
      } else {
        bleAuthenticated = false;
        if (++bleAuthFailures >= 5) { bleAuthBlockedUntil = millis() + 30000; bleAuthFailures = 0; }
        sendBleReply("auth", op, id, false, "网页密码不正确。");
      }
    } else if (!bleAuthenticated) {
      sendBleReply("error", op, id, false, "请先用网页密码连接设备。");
    } else if (!strcmp(op, "status")) {
      bridgeBleSend(buildDeviceStatus(false), true);
      lastBleStatusAt = millis();
    } else if (!strcmp(op, "wifi_scan")) {
      if (bleWifiScanPending || WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        sendBleReply("error", op, id, false, "Wi-Fi 扫描正在进行，请稍后重试。");
      } else if (WiFi.scanNetworks(true, true) != WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
        sendBleReply("error", op, id, false, "无法启动 Wi-Fi 扫描，请重试。");
      } else {
        bleWifiScanRequestId = id;
        bleWifiScanSession = bridgeBleSession();
        bleWifiScanPending = true;
      }
    } else if (!strcmp(op, "control")) {
      const cJSON *command = cJSON_GetObjectItemCaseSensitive(json, "cmd");
      const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
      String message;
      const bool ok = cJSON_IsString(command) && executeAcCommand(command->valuestring,
          cJSON_IsNumber(value) ? value->valuedouble : NAN, message);
      if (!message.length()) message = "缺少空调命令。";
      sendBleReply("ack", op, id, ok, message);
    } else if (!strcmp(op, "wifi")) {
      const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
      const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
      String message;
      const bool ok = cJSON_IsString(ssid) && cJSON_IsString(password) &&
          saveNetworkCredentials(ssid->valuestring, password->valuestring, message);
      if (!message.length()) message = "请填写 SSID 和密码。";
      sendBleReply("ack", op, id, ok, message);
    } else if (!strcmp(op, "web_password")) {
      const cJSON *newPassword = cJSON_GetObjectItemCaseSensitive(json, "newPassword");
      String message;
      const bool ok = cJSON_IsString(newPassword) && saveWebPassword(newPassword->valuestring, message);
      if (!message.length()) message = "缺少新密码。";
      sendBleReply("ack", op, id, ok, message);
    } else if (!strcmp(op, "homekit_unpair")) {
      homeSpan.processSerialCommand("U");
      homeKitPaired = false;
      sendBleReply("ack", op, id, true, "HomeKit 配对记录已清除。请在“家庭”App 移除旧配件，再使用此处显示的配对码重新添加。");
    } else if (!strcmp(op, "miot_provision")) {
      String message;
      const bool ok = enterMiotProvision(message);
      sendBleReply("ack", op, id, ok, message);
    } else {
      sendBleReply("error", op, id, false, "不支持的操作。");
    }
    // Passwords and network request bodies are never copied into UART/web logs.
    cJSON_Delete(json);
  }
  serviceBleWifiScan();
  if (bridgeBleConnected() && bleAuthenticated && static_cast<uint32_t>(millis() - lastBleStatusAt) >= 3000) {
    bridgeBleSend(buildDeviceStatus(false), true);
    lastBleStatusAt = millis();
  }
  bridgeBlePoll();
}

static void handleHomeKitUnpair() {
  if (!requireLogin()) return;
  if (server.arg("plain") != "delete") {
    server.send(400, "text/plain; charset=utf-8", "请从网页确认清除 HomeKit 配对。");
    return;
  }
  homeSpan.processSerialCommand("U");
  homeKitPaired = false;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", "HomeKit 配对记录已清除。请在“家庭”App 移除旧配件，再扫描网页二维码重新添加。");
}

static void setFirmwareUpdateError(int httpStatus, const char *message) {
  firmwareUpdateFailed = true;
  firmwareUpdateReady = false;
  firmwareUpdateActive = false;
  firmwareUpdateHttpStatus = httpStatus;
  snprintf(firmwareUpdateError, sizeof(firmwareUpdateError), "%s", message ? message : "固件更新失败。");
}

static bool hasWebSessionForFirmwareUpdate() {
  if (setupApMode || !webSessionToken.length()) return false;
  const String cookie = server.header("Cookie");
  return cookie.indexOf("midea_session=" + webSessionToken) >= 0;
}

static void handleFirmwareUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    firmwareUpdateAuthorized = hasWebSessionForFirmwareUpdate();
    firmwareUpdateActive = false;
    firmwareUpdateFailed = false;
    firmwareUpdateReady = false;
    firmwareUpdateBytes = 0;
    firmwareUpdateHttpStatus = 400;
    firmwareUpdateError[0] = '\0';
    if (!firmwareUpdateAuthorized) {
      setFirmwareUpdateError(setupApMode ? 503 : 401,
          setupApMode ? "备用热点模式下不能更新固件。" : "登录已失效，请重新登录后更新。");
      return;
    }
    if(scheduledRestartAt){
      setFirmwareUpdateError(409,"设备已安排重启，请重新连接后再更新固件。");return;
    }
    String filename = upload.filename;
    filename.toLowerCase();
    if (!filename.endsWith(".bin")) {
      setFirmwareUpdateError(400, "请选择 ESP32 固件 .bin 文件。");
      return;
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
      setFirmwareUpdateError(500, Update.errorString());
      return;
    }
    firmwareUpdateActive = true;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!firmwareUpdateActive || firmwareUpdateFailed) return;
    const size_t written = Update.write(upload.buf, upload.currentSize);
    firmwareUpdateBytes += written;
    if (written != upload.currentSize) {
      Update.printError(Serial);
      char error[sizeof(firmwareUpdateError)];
      snprintf(error, sizeof(error), "%s", Update.errorString());
      Update.abort();
      setFirmwareUpdateError(500, error);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!firmwareUpdateActive || firmwareUpdateFailed) return;
    if (upload.totalSize == 0 || upload.totalSize != firmwareUpdateBytes) {
      Update.abort();
      setFirmwareUpdateError(400, "上传文件不完整，请重新选择固件并上传。");
      return;
    }
    if (!Update.end(true)) {
      Update.printError(Serial);
      setFirmwareUpdateError(500, Update.errorString());
      return;
    }
    firmwareUpdateActive = false;
    firmwareUpdateReady = true;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (firmwareUpdateActive) Update.abort();
    setFirmwareUpdateError(400, "上传中断，当前固件未切换。");
  }
}

static void handleFirmwareUpdateResult() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  if (!firmwareUpdateAuthorized) {
    server.send(firmwareUpdateHttpStatus, "text/plain; charset=utf-8",
                firmwareUpdateError[0] ? firmwareUpdateError : "请先选择固件 .bin 文件。");
    firmwareUpdateAuthorized = false;
    firmwareUpdateFailed = false;
    firmwareUpdateReady = false;
    firmwareUpdateBytes = 0;
    firmwareUpdateError[0] = '\0';
    return;
  }
  if (firmwareUpdateFailed || !firmwareUpdateReady) {
    server.send(firmwareUpdateFailed ? firmwareUpdateHttpStatus : 500,
                "text/plain; charset=utf-8",
                firmwareUpdateError[0] ? firmwareUpdateError : "固件未完成写入，设备未重启。");
    firmwareUpdateAuthorized = false;
    firmwareUpdateFailed = false;
    firmwareUpdateReady = false;
    firmwareUpdateBytes = 0;
    firmwareUpdateError[0] = '\0';
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "固件校验并写入成功，设备即将重启切换到新固件。");
  firmwareUpdateAuthorized = false;
  firmwareUpdateFailed = false;
  firmwareUpdateReady = false;
  firmwareUpdateBytes = 0;
  firmwareUpdateError[0] = '\0';
  scheduledRestartAt = millis() + 1800;
}

static void startWebServer() {
  if (webStarted) return;
  // Restart the listener after recovery without allocating duplicate handlers.
  if(webRoutesConfigured){server.begin();webStarted=true;return;}
  const char *requestHeaders[] = {"Cookie","If-None-Match"};
  server.collectHeaders(requestHeaders, 2);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/logout", HTTP_POST, handleLogout);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/integrations", HTTP_GET, handleRoot);
  server.on("/device", HTTP_GET, handleRoot);
  server.on("/debug", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleRoot);
  server.on("/wifi-setup", HTTP_POST, []() {
    if (!wifiProvisionApActive || server.client().localIP()!=WiFi.softAPIP()) {
      server.send(404, "text/plain; charset=utf-8", "配网页面未启用。"); return;
    }
    if (server.arg("webPassword") != webPassword) {
      server.send(401, "text/plain; charset=utf-8", "网页密码不正确，请返回重试。"); return;
    }
    if (scheduledRestartAt || firmwareUpdateActive) {
      server.send(409, "text/plain; charset=utf-8", "设备正在重启或更新，请稍后重试。"); return;
    }
    String message;
    const String ssid = server.arg("ssid"), password = server.arg("password");
    if (!server.hasArg("ssid") || !server.hasArg("password") ||
        !saveNetworkCredentials(ssid.c_str(), password.c_str(), message)) {
      server.send(400, "text/plain; charset=utf-8", message.length()?message:"请填写 Wi-Fi 名称和密码。"); return;
    }
    server.send(200, "text/html; charset=utf-8", "<!doctype html><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta charset=\"utf-8\"><p>Wi-Fi 已保存，设备正在重启。请将手机连接到新网络，再从路由器设备列表查看 ESP32 的地址。</p>");
  });
  server.on("/assets/app.css", HTTP_GET, [](){sendWebAsset("text/css; charset=utf-8",WEB_STYLE,"css");});
  server.on("/assets/app.js", HTTP_GET, [](){sendWebAsset("application/javascript; charset=utf-8",WEB_SCRIPT,"js");});
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/io16/rx", HTTP_POST, handleIo16RxMode);
  server.on("/api/io16/log", HTTP_GET, handleIo16RxLog);
  server.on("/api/io16/log/clear", HTTP_POST, handleIo16RxLogClear);
  server.on("/api/send", HTTP_POST, handleSend);
  server.on("/api/miot/query", HTTP_POST, handleMiotQuery);
  server.on("/api/miot/test-environment", HTTP_POST, handleMiotTestEnvironment);
  server.on("/api/miot/simulation", HTTP_POST, handleMiotSimulation);
  server.on("/api/mock", HTTP_POST, handleMockMode);
  server.on("/api/miot/provision", HTTP_POST, handleMiotProvision);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/timers", HTTP_POST, handleTimers);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/system", HTTP_GET, handleSystemStatus);
  server.on("/api/password", HTTP_POST, handleChangePassword);
  server.on("/api/wifi", HTTP_POST, handleWifiConfig);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/ble/config", HTTP_POST, handleBleConfig);
  server.on("/api/fan-mapping", HTTP_GET, handleFanMapping);
  server.on("/api/fan-mapping", HTTP_POST, handleFanMapping);
  server.on("/api/network/hostname", HTTP_GET, handleNetworkHostname);
  server.on("/api/network/hostname", HTTP_POST, handleNetworkHostname);
  server.on("/api/homekit/unpair", HTTP_POST, handleHomeKitUnpair);
  server.on("/api/update", HTTP_POST, handleFirmwareUpdateResult, handleFirmwareUpdateUpload);
  server.on("/pairing.svg", HTTP_GET, []() {
    if (!requireLogin()) return;
    server.send_P(200, "image/svg+xml", PAIRING_QR_SVG);
  });
  server.onNotFound([]() { server.send(404, "text/plain; charset=utf-8", "Not found"); });
  webRoutesConfigured=true;
  server.begin();
  webStarted = true;
}

static void startFallbackAp() {
  if (setupApMode) return;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Midea-S3-UART-Setup", FALLBACK_AP_PASSWORD);
  setupApMode = true;
  startWebServer();
}

// Keep HomeSpan's STA reconnect running while a temporary provisioning AP is
// available. The existing project credential store remains the single source
// of truth on the next boot; HomeSpan's blocking A command would use another.
static void serviceWifiProvision(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    wifiOfflineTimerStarted = false;
    if (wifiProvisionApActive) {
      WiFi.softAPdisconnect(false);
      wifiProvisionApActive = false;
      addTextLog("NET", "Wi-Fi 已恢复，配网热点已关闭");
    }
    return;
  }
  if (!wifiOfflineTimerStarted) { wifiOfflineTimerStarted=true; wifiOfflineSince=now; }
  if (wifiProvisionApActive || firmwareUpdateActive || scheduledRestartAt ||
      uint32_t(now-wifiOfflineSince)<WIFI_PROVISION_DELAY_MS ||
      (wifiProvisionRetryAt && int32_t(now-wifiProvisionRetryAt)<0)) return;
  wifiProvisionRetryAt=now+30000;
  if (strlen(FALLBACK_AP_PASSWORD)<8 || strlen(FALLBACK_AP_PASSWORD)>63) {
    addTextLog("NET", "配网热点密码必须为 8–63 字节，无法启动热点"); return;
  }
  if (!WiFi.mode(WIFI_AP_STA) || !WiFi.softAP("Midea-S3-UART-Setup", FALLBACK_AP_PASSWORD)) {
    addTextLog("NET", "Wi-Fi 配网热点启动失败，将重试"); return;
  }
  wifiProvisionApActive=true;
  startWebServer();
  addTextLog("NET", "路由器持续未连接，已开启配网热点 192.168.4.1");
}

static void onHomeKitPairing(bool paired) {
  homeKitPaired = paired;
  Serial.printf("\n[HomeKit] 配对状态：%s\n", paired ? "已配对" : "未配对");
}

static void flushRxFrame() {
  if (!rxFrameLength) return;
  addLog("RX", rxFrame, rxFrameLength);
  // Split bursts by the declared frame length and reject truncated/corrupt data.
  for (size_t offset = 0; offset < rxFrameLength;) {
    if (rxFrame[offset] != 0xAA) { ++offset; continue; }
    if (offset + 2 > rxFrameLength) { ++mideaRejectedFrames; break; }
    const size_t length = size_t(rxFrame[offset + 1]) + 1;
    if (length < 12 || offset + length > rxFrameLength) { ++mideaRejectedFrames; ++offset; continue; }
    bool selfCleanActive=false,buzzerOn=false;
    const bool cleanReport=!mockMode && decodeMideaSelfClean(rxFrame+offset,length,selfCleanActive);
    const bool buzzerReport=!mockMode && decodeMideaBuzzer(rxFrame+offset,length,buzzerOn);
    if(cleanReport || buzzerReport){
      if(cleanReport)miotBridge.updateSelfClean(selfCleanActive,lastRxByteMs);
      if(buzzerReport)miotBridge.updateBuzzer(buzzerOn,lastRxByteMs);
      ++mideaValidFrames;
      offset+=length;continue;
    }
    MideaAcSnapshot actual;
    if (decodeMideaState(rxFrame + offset, length, millis(), actual)) {
      mideaCommandFlags.captureFromValidatedC0(rxFrame + offset, length);
      if(!mockMode) {
        mideaTimers.captureFromValidatedC0(rxFrame+offset,actual.receivedAt);
        confirmMideaTimers();
      }
      ++mideaValidFrames;
      // Ignore off-state fan feedback before updating ANY consumer: HTTP/BLE,
      // HomeKit and MIoT must all keep the last accepted powered-on speed.
      // The unmodified wire frame remains available in statusRaw and RX logs.
      if (!actual.power) actual.fan = statusFan;
      statusKnown = actual.known; statusPower = actual.power; statusMode = actual.mode;
      statusTemperature = actual.target; statusFan = actual.fan; statusSwing = actual.swing;
      statusEco = actual.eco; statusSleep = actual.sleep; statusDisplayOn = actual.displayOn; statusAuxHeat = actual.auxHeat; roomTemperatureKnown = actual.roomKnown;
      roomTemperature = actual.room; lastStatusAt = actual.receivedAt;
      statusRaw = bytesToHex(rxFrame + offset, length);
      miotBridge.updateMidea(actual);
      offset += length;
    } else {
      if (rxFrame[offset + 2] == 0xAC && length > 12 && rxFrame[offset + 10] == 0xC0) ++mideaRejectedFrames;
      ++offset;
    }
  }
  rxFrameLength = 0;
}

static void serviceSerial() {
  serviceIo16Rx();
  while (Serial2.available() > 0) {
    const int value = Serial2.read();
    if (value < 0) break;
    const uint8_t byteValue = static_cast<uint8_t>(value);
    if (rxFrameLength >= MAX_FRAME_BYTES) flushRxFrame();
    rxFrame[rxFrameLength++] = byteValue;
    lastRxByteMs = millis();
  }
  if (rxFrameLength && static_cast<uint32_t>(millis() - lastRxByteMs) >= RX_FRAME_IDLE_MS) flushRxFrame();

  bool usbTxActive = false;
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) break;
    const uint8_t byteValue = static_cast<uint8_t>(value);
    if(io16RxMode)continue; // Never replay USB TX bytes after leaving RX mode.
    if (!usbTxActive) {
      if (!beginMideaTx()) break;
      usbTxActive = true;
    }
    Serial2.write(byteValue);
    if (usbFrameLength >= MAX_FRAME_BYTES) {
      addLog("TX", usbFrame, usbFrameLength);
      usbFrameLength = 0;
    }
    usbFrame[usbFrameLength++] = byteValue;
    lastUsbByteMs = millis();
  }
  if (usbTxActive) {
    endMideaTx();
  }
  if (usbFrameLength) {
    // USB bytes are logged after a short quiet period so bursts appear as one record.
    if (static_cast<uint32_t>(millis() - lastUsbByteMs) >= RX_FRAME_IDLE_MS) {
      addLog("TX", usbFrame, usbFrameLength);
      usbFrameLength = 0;
    }
  }
}

void setup() {
  Serial.begin(USB_BAUD);
  const esp_reset_reason_t bootResetReason = esp_reset_reason();
  Serial.println();
  Serial.println("==================================================");
  Serial.printf(" [SYS] Midea-Bridge v%s (ESP32-S3-MINI)\n", BRIDGE_VERSION);
  Serial.printf(" [SYS] 上电复位原因: %s (code=%d)\n", getResetReasonDescription(bootResetReason), static_cast<int>(bootResetReason));
  Serial.printf(" [SYS] 内部堆可用: %u 字节 (最低历史: %u), PSRAM 可用: %u 字节\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
  Serial.printf(" [SYS] 美的串口: RX=GPIO%d, TX=GPIO%d, 波特率=%u\n", MODULE_RX_PIN, MODULE_TX_PIN, MODULE_BAUD);
  Serial.printf(" [SYS] 米家串口: RX=GPIO%d, TX=GPIO%d, 波特率=%u\n", MIOT_RX_PIN, MIOT_TX_PIN, MIOT_BAUD);
  Serial.println("==================================================");
  beginLogStorage();
  startBridgeSerialPorts();
  miotBridge.begin(MiotSerial, addTextLog, sendSetState, sendStatusQuery, MIOT_PROFILE_OVERRIDE, setDisplayFromMiot, setCountdownFromMiot, getCountdownForMiot, setAuxHeatFromMiot, setSelfCleanFromMiot, setBuzzerFromMiot);
  miotBridge.setDirections(true, true);
  miotBridge.setEnabled(true);
  server.setServiceHook([](){serviceSerial();miotBridge.tick(millis());});

  webPrefs.begin("midea_web", true);
  webPassword = webPrefs.getString("password", DEFAULT_WEB_PASSWORD);
  const bool miotSimulation=webPrefs.getBool("miot_sim",false);
  mockMode = webPrefs.getBool("mock_mode", false);
  bleEnabled = webPrefs.getBool("ble_enabled", true);
  String savedHostname=webPrefs.getString("hostname",DEFAULT_NETWORK_HOSTNAME);
  if(normalizeMdnsHostname(savedHostname))networkHostname=mdnsHostname=savedHostname;
  const String savedIdentity=webPrefs.getString("net_identity","");
  cJSON *identity=cJSON_Parse(savedIdentity.c_str());
  String savedDeviceName,savedMdnsName;
  if(readNetworkIdentity(identity,savedDeviceName,savedMdnsName)){
    networkHostname=savedDeviceName;mdnsHostname=savedMdnsName;
  }
  cJSON_Delete(identity);
  MiotMideaBridge::FanMapping savedFanMapping;
  const bool hasFanMapping=webPrefs.getBytesLength("fan_levels")==sizeof(savedFanMapping.levels) &&
    webPrefs.getBytes("fan_levels",savedFanMapping.levels,sizeof(savedFanMapping.levels))==sizeof(savedFanMapping.levels);
  const bool hasLegacyAppointments=webPrefs.isKey("appointments");
  webPrefs.end();
  // Retired local schedules must not survive as active records for a future load.
  if(hasLegacyAppointments && webPrefs.begin("midea_web",false)){
    webPrefs.remove("appointments");webPrefs.end();
  }
  if(hasFanMapping)miotBridge.setFanMapping(savedFanMapping);
  if(miotSimulation) miotBridge.setSimulationEnabled(true);
  miotBridge.setMockMode(mockMode);
  if (mockMode) {
    statusKnown = true;
    lastStatusAt = millis();
    statusPower = false;
    statusMode = 2;
    statusTemperature = 26.0f;
    statusFan = 102;
    statusSwing = 0;
    statusEco = false;
    statusSleep = false;
    roomTemperatureKnown = true;
    roomTemperature = 25.0f;
  }
  webSessionToken = String(static_cast<uint32_t>(esp_random()), HEX) + String(static_cast<uint32_t>(esp_random()), HEX);
  loadNetworkCredentials();
  wifiHealth.begin(addTextLog);

  // HomeSpan enables STA in its init() hook before setup(). Set both the
  // default and that existing netif's name before HomeSpan starts DHCP.
  WiFi.setHostname(networkHostname.c_str());
  if(!WiFi.STA.setHostname(networkHostname.c_str()))
    Serial.println("[NET] STA hostname not yet applied; Wi-Fi startup will use the configured default.");

  // Keep USB serial available for readable HomeSpan pairing diagnostics. The AC
  // UART stream is shown as HEX on the web page, not mixed into this log console.
  homeSpan.setLogLevel(1).setSerialInputDisable(true).setWifiCredentials(configuredWifiSsid.c_str(), configuredWifiPassword.c_str());
  homeSpan.setHostNameSuffix("").setPortNum(51826).setApSSID("Midea-S3-HomeKit-Setup").setApPassword(FALLBACK_AP_PASSWORD).enableAutoStartAP();
  homeSpan.setPairCallback(onHomeKitPairing);
  homeSpan.setQRID(HOMEKIT_SETUP_ID);
  homeSpan.begin(Category::AirConditioners, "Midea S3 空调", mdnsHostname.c_str(), "Midea S3 UART AC");
  homeSpan.setPairingCode(HOMEKIT_SETUP_CODE, false);
  HapQR pairingPayload;
  Serial.printf("[HomeKit QR] payload=%s\n",
      pairingPayload.get(strtoul(HOMEKIT_SETUP_CODE, nullptr, 10), HOMEKIT_SETUP_ID, static_cast<uint8_t>(Category::AirConditioners)));
  repairPrefs.begin("midea_fix", false);
  clearStalePairingOnce = !repairPrefs.getBool("pairfix", false);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Manufacturer("Midea");
      new Characteristic::Model("UART AC Bridge S3");
      new Characteristic::SerialNumber("MIDEA-ESP32-S3");
    new MideaHomeKitThermostat();
  bleAvailable = bleEnabled && bridgeBleBegin();
  Serial.printf("[BLE] %s; ready=%s\n", bleEnabled ? bridgeBleName().c_str() : "disabled", bleAvailable ? "yes" : "no");
  beginDeviceMetrics();
}

void loop() {
  if (mockMode) {
    lastStatusAt = millis();
  }
  serviceDeviceMetrics();
  serviceSerial();
  miotBridge.tick(millis());
  serviceBleCommands();
  uint32_t phaseStartedAt=millis();
  homeSpan.poll();
  uint32_t phaseMs=millis()-phaseStartedAt;
  if(phaseMs>homekitMaxPollMs)homekitMaxPollMs=phaseMs;
  serviceSerial();
  miotBridge.tick(millis());
  if (clearStalePairingOnce) {
    homeSpan.processSerialCommand("U");
    repairPrefs.putBool("pairfix", true);
    clearStalePairingOnce = false;
  }
  homeKitPaired = HAPClient::nAdminControllers() > 0;
  serviceScheduledQuery();
  serviceMideaTimers();
  serviceMideaFeatureQueries();

  serviceWifiProvision(millis());

  if (WiFi.status() == WL_CONNECTED) {
    if (!webStarted) startWebServer();
    if (!httpMdnsAdded) {
      MDNS.addService("http", "tcp", 80);
      httpMdnsAdded = true;
    }
  }
  if (!io16RxMode && !miotBridge.pending() && static_cast<uint32_t>(millis() - lastQueryAt) >= 5000) sendStatusQuery();
  if (webStarted) {
    phaseStartedAt=millis();server.handleClient();phaseMs=millis()-phaseStartedAt;
    if(phaseMs>webMaxHandleMs)webMaxHandleMs=phaseMs;
  }
  if(!setupApMode && wifiHealth.tick(millis(),server.writeTimeouts(),firmwareUpdateActive || scheduledRestartAt)){
    // HomeSpan receives the disconnect event and performs its normal all-channel
    // reconnect. Preserve pairing, UART operation, credentials and session token.
    server.close();webStarted=false;
    if(!WiFi.disconnectAsync(false,false))addTextLog("NET","Wi-Fi recovery disconnect request was not accepted");
  }
  serviceSerial();
  miotBridge.tick(millis());
  if (scheduledRestartAt && static_cast<int32_t>(millis() - scheduledRestartAt) >= 0) ESP.restart();
  serviceBleCommands();

  static uint32_t lastDiagHeartbeat = 0;
  const uint32_t currentNow = millis();
  if (currentNow - lastDiagHeartbeat >= 5000) {
    lastDiagHeartbeat = currentNow;
    const bool wifiConn = WiFi.status() == WL_CONNECTED;
    Serial.printf("[DIAG %lus] Heap: %uB(min:%uB) | PSRAM: %uB | WiFi: %s(RSSI:%d) IP: %s | Midea: valid=%u reject=%u | MIoT: %s(%s)\n",
        static_cast<unsigned long>(currentNow / 1000),
        ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram(),
        wifiConn ? "已连接" : "未连接", wifiConn ? WiFi.RSSI() : 0,
        wifiConn ? WiFi.localIP().toString().c_str() : "-",
        mideaValidFrames, mideaRejectedFrames,
        miotBridge.model().c_str(), miotBridge.network().c_str());
    if (ESP.getFreeHeap() < 35000) {
      Serial.printf("[WARN] 内存过低! FreeHeap=%u, MinFree=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
  }
}
