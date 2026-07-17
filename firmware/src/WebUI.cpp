#include "WebUI.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "Ota.h"
#include "Mqtt.h"
#include "Motors.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

// WebUI (Phase S) — the sidebar single-page app is static files in LittleFS
// (data/index.html, style.css, app.js). The firmware serves those plus a JSON/REST
// API and a WebSocket log feed. If the filesystem image hasn't been flashed yet a
// tiny embedded bootstrap page still offers OTA, so a device can't strand itself.
// Pattern carried over from the Shutter Hub's WebUI — see docs/decisions/0001.

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws/logs");
static bool   pendingWifiConnect = false;   // in-place network switch (not a reboot)
static String connSsid, connPass;

// escape a string for embedding in JSON
static String jesc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// Web-auth gate: true if the request may proceed; otherwise a 401 is sent.
static bool guard(AsyncWebServerRequest *r) {
  if (!AppConfig::authEnabled()) return true;
  if (!r->authenticate(AppConfig::authUser().c_str(), AppConfig::authPass().c_str())) {
    r->requestAuthentication();
    return false;
  }
  return true;
}

// ---- JSON builders ----------------------------------------------------------

static String dashInfoJson() {
  String j = "{";
  j += "\"fw\":\"" FW_VERSION "\",";
  j += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  j += "\"device\":\"" + jesc(AppConfig::deviceName()) + "\",";
  j += "\"host\":\"" + jesc(AppConfig::deviceName()) + ".local\",";
  j += "\"uptime\":\"" + Diagnostics::humanUptime() + "\",";
  j += "\"boot_count\":" + String(AppConfig::bootCount()) + ",";
  j += "\"reset_reason\":\"" + Diagnostics::resetReason() + "\",";
  j += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"min_free_heap\":" + String(ESP.getMinFreeHeap()) + ",";
  j += "\"wifi\":{";
  j += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"ssid\":\"" + jesc(WiFi.SSID()) + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"mac\":\"" + WiFi.macAddress() + "\"},";
  j += "\"mqtt\":{\"enabled\":" + String(AppConfig::mqttEnabled() ? "true" : "false") +
       ",\"connected\":" + String(Mqtt::connected() ? "true" : "false") +
       ",\"state\":\"" + jesc(Mqtt::stateText()) + "\"},";
  j += "\"last_flash\":{\"type\":\"" + jesc(AppConfig::lastFlashType()) +
       "\",\"ok\":" + String(AppConfig::lastFlashOk() ? "true" : "false") +
       ",\"epoch\":" + String(AppConfig::lastFlashEpoch()) + "},";
  j += "\"motors\":[";
  for (int i = 0; i < Motors::count(); i++) {
    if (i) j += ",";
    j += "{\"name\":\"" + jesc(Motors::nameAt(i)) + "\",\"mac\":\"" + jesc(Motors::macAt(i)) +
         "\",\"calibrated\":" + String(Motors::calibratedAt(i) ? "true" : "false") + "}";
  }
  j += "]";
  j += "}";
  return j;
}

static String mqttConfigJson() {
  String j = "{";
  j += "\"enabled\":" + String(AppConfig::mqttEnabled() ? "true" : "false") + ",";
  j += "\"host\":\"" + jesc(AppConfig::mqttHost()) + "\",";
  j += "\"port\":" + String(AppConfig::mqttPort()) + ",";
  j += "\"clientId\":\"" + jesc(AppConfig::mqttClientId()) + "\",";
  j += "\"user\":\"" + jesc(AppConfig::mqttUser()) + "\",";
  j += "\"hasPass\":" + String(AppConfig::mqttPass().length() ? "true" : "false") + ",";
  j += "\"base\":\"" + jesc(AppConfig::mqttBaseTopic()) + "\",";
  j += "\"haDiscovery\":" + String(AppConfig::mqttHaDiscovery() ? "true" : "false") + ",";
  j += "\"connected\":" + String(Mqtt::connected() ? "true" : "false") + ",";
  j += "\"state\":\"" + jesc(Mqtt::stateText()) + "\"";
  j += "}";
  return j;
}

// ---- Embedded fallback (only used when data/ isn't flashed) ------------------

static String bootstrapPage() {
  String h = F("<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Somfy BLE Hub — recovery</title>"
    "<body style='font-family:system-ui;max-width:34rem;margin:2rem auto;padding:0 1rem'>"
    "<h1>Somfy BLE Hub</h1>"
    "<p style='color:#c0392b'><b>Web UI filesystem not found.</b> Upload the LittleFS image "
    "(and/or firmware) below, then reboot. Firmware v" FW_VERSION ".</p>"
    "<p><label>Firmware (.bin) <input type=file id=fw accept='.bin'></label></p>"
    "<p><label>Filesystem (.bin) <input type=file id=fs accept='.bin'></label></p>"
    "<button id=go>Upload</button> <span id=s></span>"
    "<script>"
    "function up(t,f){return new Promise(function(res,rej){var x=new XMLHttpRequest();"
    "x.open('POST','/api/ota?target='+t);x.onload=function(){(x.status==200)?res():rej(x.responseText);};"
    "x.onerror=function(){rej('network');};var d=new FormData();d.append('file',f);x.send(d);});}"
    "document.getElementById('go').onclick=function(){var s=document.getElementById('s');"
    "var fw=document.getElementById('fw').files[0],fs=document.getElementById('fs').files[0];"
    "var p=Promise.resolve();if(fs)p=p.then(function(){s.textContent='fs\\u2026';return up('filesystem',fs);});"
    "if(fw)p=p.then(function(){s.textContent='fw\\u2026';return up('firmware',fw);});"
    "p.then(function(){s.textContent='done — reboot';}).catch(function(e){s.textContent='failed: '+e;});};"
    "</script>");
  return h;
}

// ---- WebSocket log feed -----------------------------------------------------

static void broadcastLog(const String &jsonLine) {
  if (ws.count() == 0) return;
  ws.textAll(jsonLine);
}

static void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    c->text(Diagnostics::logHistoryJson());   // catch the client up on buffered lines
  }
}

// ---- Public API -------------------------------------------------------------

namespace WebUI {

void begin() {
  String host = AppConfig::deviceName();

  if (MDNS.begin(host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    LOGI("mdns", "http://%s.local", host.c_str());
  } else {
    LOGW("mdns", "start failed");
  }

  // Live log feed: register the sink so every LOG* line streams to WS clients.
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  Diagnostics::setLogSink(broadcastLog);

  // ---- Read-only info ----
  server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", dashInfoJson());
  });
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *r) {   // legacy raw diagnostics
    r->send(200, "application/json", Diagnostics::infoJson());
  });
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", Diagnostics::logHistoryJson());
  });
  server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "ok");
  });

  // ---- Motors (read-only this pass — pairing/calibration UI is Phase 3) ----
  server.on("/api/motors", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", Motors::listJson());
  });

  // ---- MQTT config ----
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", mqttConfigJson());
  });
  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    auto p = [&](const char *k, const String &d) {
      return r->hasParam(k, true) ? r->getParam(k, true)->value() : d;
    };
    bool     en   = p("enabled", "false") == "true" || p("enabled", "") == "1";
    String   host = p("host", AppConfig::mqttHost());
    uint16_t port = (uint16_t)p("port", String(AppConfig::mqttPort())).toInt();
    String   cid  = p("clientId", "");
    String   user = p("user", AppConfig::mqttUser());
    // Blank password field => keep the stored password (we never echo it back).
    String   pass = r->hasParam("pass", true) && r->getParam("pass", true)->value().length()
                      ? r->getParam("pass", true)->value() : AppConfig::mqttPass();
    String   base = p("base", AppConfig::mqttBaseTopic());
    bool     ha   = p("haDiscovery", "true") == "true" || p("haDiscovery", "") == "1";
    AppConfig::setMqtt(en, host, port, cid, user, pass, base, ha);
    Mqtt::reconfigure();
    r->send(200, "application/json", mqttConfigJson());
  });

  // ---- System quick actions ----
  // Each sends its response first, then schedules the restart via a high-priority
  // esp_timer (Diagnostics::scheduleReboot) so it fires even if the main loop is stalled.
  server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
    Diagnostics::scheduleReboot(600);
  });
  server.on("/api/system/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"WiFi cleared — restarting into Somfy-BLE-Setup\"}");
    WiFiSetup::forget();                 // clear creds now; the timer restarts us shortly
    Diagnostics::scheduleReboot(600);
  });
  server.on("/api/system/reset-config", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"settings cleared — rebooting (WiFi + motors kept)\"}");
    AppConfig::factoryReset();           // wipe app settings now; timer restarts us shortly
    Diagnostics::scheduleReboot(600);
  });

  // ---- Security (web auth) ----
  server.on("/api/auth", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      String("{\"enabled\":") + (AppConfig::authEnabled() ? "true" : "false") +
      ",\"user\":\"" + jesc(AppConfig::authUser()) + "\"}");
  });
  server.on("/api/auth", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    bool   en   = r->hasParam("enabled", true) &&
                  (r->getParam("enabled", true)->value() == "true" ||
                   r->getParam("enabled", true)->value() == "1");
    String user = r->hasParam("user", true) ? r->getParam("user", true)->value() : AppConfig::authUser();
    String pass = r->hasParam("pass", true) && r->getParam("pass", true)->value().length()
                    ? r->getParam("pass", true)->value() : AppConfig::authPass();
    AppConfig::setAuth(en, user, pass);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // ---- WiFi (feeds the System > WiFi sub-tab) ----
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) { r->send(202, "application/json", "{\"scanning\":true}"); return; }
    if (n == WIFI_SCAN_FAILED)  { WiFi.scanNetworks(true); r->send(202, "application/json", "{\"scanning\":true}"); return; }
    String j = "[";
    for (int i = 0; i < n; i++) {
      if (i) j += ",";
      j += "{\"ssid\":\"" + jesc(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
           ",\"lock\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
    }
    j += "]";
    WiFi.scanDelete();
    r->send(200, "application/json", j);
  });
  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("ssid", true)) { r->send(400, "application/json", "{\"error\":\"missing ssid\"}"); return; }
    connSsid = r->getParam("ssid", true)->value();
    connPass = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "";
    pendingWifiConnect = true;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"Connecting to '" + jesc(connSsid) + "' — if the password is right the hub "
      "moves to that network (reconnect at " + AppConfig::deviceName() + ".local). A wrong password reverts.\"}");
  });

  Ota::begin(server);   // /api/ota (firmware + filesystem)

  // ---- Static SPA from LittleFS, with an embedded recovery fallback ----
  if (LittleFS.exists("/index.html")) {
    // no-cache (not no-store): browsers must revalidate every load instead of trusting their
    // heuristic cache. LittleFS files report an epoch Last-Modified, so without this a browser
    // can cache index.html/app.js/style.css indefinitely — after reflashing a new FS image the
    // UI keeps running the OLD JS with old button wiring until a hard refresh clears it.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache");
    LOGI("http", "serving web UI from LittleFS (no-cache: always revalidates)");
  } else {
    LOGW("http", "no /index.html in LittleFS — serving recovery page (flash the FS image)");
    server.onNotFound([](AsyncWebServerRequest *r) { r->send(200, "text/html", bootstrapPage()); });
  }

  server.begin();
  LOGI("http", "server up — http://%s.local or the device IP", host.c_str());
}

void loop() {
  Ota::loop();
  ws.cleanupClients();
  if (pendingWifiConnect) {
    pendingWifiConnect = false;
    WiFiSetup::connectTo(connSsid, connPass);   // blocks ~12–24s; async server keeps serving
  }
}

}  // namespace WebUI
