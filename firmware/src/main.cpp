// Somfy Roller Blind BLE -> HA — firmware framework (v0.0.x, Phase S)
// -----------------------------------------------------------------------------
// Thin entry point. All behaviour lives in modules (see docs/architecture.md):
//   AppConfig  persisted settings (NVS)                          [real]
//   Diagnostics logging, /info, uptime, reboot                   [real]
//   WiFiSetup  WiFiManager AP + captive portal (creds->NVS)      [real]
//   WebUI      tabbed status page + routes + mDNS                [real]
//   Ota        custom firmware + LittleFS update                 [real]
//   Motors     per-motor definitions + calibration (NVS)         [stub, Phase 1-2]
//   Mqtt       HA covers + discovery + state                     [stub, Phase 4]
//   SomfyBle   connect-on-demand BLE client (NimBLE)              [stub, Phase 1]
//
// Reuses the ESP32 Smart Shutter Hub's firmware framework wholesale for everything
// that isn't BLE-specific — see docs/decisions/0001-framework-reuse.md.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <time.h>
#include <LittleFS.h>
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "WebUI.h"
#include "Motors.h"
#include "Mqtt.h"
#include "SomfyBle.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  LOGI("main", "=== Somfy BLE -> HA — firmware v%s ===", FW_VERSION);

  AppConfig::begin();      // load settings + bump boot count (needed before others)
  Diagnostics::begin();    // log boot reason / count
  if (LittleFS.begin(true)) {   // format-on-fail: a blank FS mounts empty (recovery page serves)
    LOGI("fs", "LittleFS mounted");
  } else {
    LOGW("fs", "LittleFS mount failed — web UI falls back to the recovery page");
  }
  WiFiSetup::connect();    // blocks until on WiFi (or opens the setup portal)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC clock for last-flash timestamps
  WebUI::begin();          // mDNS + web UI + OTA

  Motors::begin();         // load paired motors + calibration from NVS (empty for now)
  Mqtt::begin();           // MQTT client (no discovery until motors exist)
  SomfyBle::begin();       // NimBLE init (connect-on-demand — no persistent link)

  LOGI("main", "ready");
}

void loop() {
  WebUI::loop();
  Mqtt::loop();             // pump MQTT client + non-blocking reconnect
  SomfyBle::loop();         // pump any pending async BLE work
  delay(5);
}
