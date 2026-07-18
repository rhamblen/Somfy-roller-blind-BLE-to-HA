#include "WiFiSetup.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <WiFi.h>
#include <WiFiManager.h>

static const char    *AP_NAME          = "Somfy-BLE-Setup";
static const uint16_t  PORTAL_TIMEOUT_S = 180;  // reboot & retry if left unconfigured

namespace WiFiSetup {

void connect() {
  String host = AppConfig::deviceName();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(host.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);            // disable modem power-save — keeps portal + web UI snappy

  WiFiManager wm;
  wm.setHostname(host.c_str());
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setAPClientCheck(true);       // pause the timeout while a phone is connected
  wm.setWiFiAPChannel(1);          // fixed channel — avoids slow hopping on the portal
  wm.setMinimumSignalQuality(15);  // drop very weak APs -> shorter scan list
  wm.setRemoveDuplicateAPs(true);
  wm.setShowInfoUpdate(false);
  wm.setCustomMenuHTML(
    "<div style='padding:8px;margin-top:8px;border:1px solid #8886;border-radius:8px;"
    "opacity:.85;font-size:.9em'>Tap <b>Configure WiFi</b>, then allow a few seconds "
    "while the hub scans for nearby networks &mdash; this only happens during setup.</div>");
  wm.setAPCallback([](WiFiManager *) {
    WiFi.setSleep(false);
    LOGI("wifi", "setup AP \"%s\" up — join it and open http://192.168.4.1", AP_NAME);
  });

  if (!wm.autoConnect(AP_NAME)) {
    LOGW("wifi", "not configured / timed out — rebooting to retry");
    delay(3000);
    ESP.restart();
  }
  // WiFi is set: tear the setup AP down and stay station-only.
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  // Modem sleep back ON now that the portal is done. A classic single-radio ESP32
  // shares its antenna between WiFi and BLE via a coexistence scheduler that depends
  // on WiFi power-save being available — leaving sleep off (as we do above, for a
  // snappy portal) makes SomfyBle::begin()'s NimBLEDevice::init() abort in
  // coex_core_enable the moment it tries to enable the BT controller.
  WiFi.setSleep(true);
  LOGI("wifi", "connected to \"%s\": %s (RSSI %d dBm)",
       WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

bool connectTo(const String &ssid, const String &pass) {
  String oldSsid = WiFi.SSID();
  String oldPsk  = WiFi.psk();
  LOGI("wifi", "switching to \"%s\"…", ssid.c_str());

  WiFi.begin(ssid.c_str(), pass.c_str());   // disconnects old, persists new creds on success
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000UL) delay(200);

  if (WiFi.status() == WL_CONNECTED) {
    LOGI("wifi", "now on \"%s\": %s", ssid.c_str(), WiFi.localIP().toString().c_str());
    return true;
  }

  LOGW("wifi", "could not join \"%s\" — reverting to \"%s\"", ssid.c_str(), oldSsid.c_str());
  WiFi.begin(oldSsid.c_str(), oldPsk.c_str());
  start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000UL) delay(200);
  return false;
}

void forget() {
  LOGW("wifi", "clearing saved WiFi credentials");
  WiFiManager wm;
  wm.resetSettings();
}

void forgetAndReboot() {
  forget();
  delay(200);
  ESP.restart();
}

}  // namespace WiFiSetup
