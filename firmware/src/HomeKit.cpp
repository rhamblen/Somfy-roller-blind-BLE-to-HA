#include "HomeKit.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "Motors.h"
#include "SomfyBle.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "HomeSpan.h"

// A HomeSpan bridge that mirrors the MQTT cover integration (Phase 4) — one Apple Home
// "Window Covering" accessory per paired motor. See HomeKit.h for the coexistence
// contract with WiFiManager / mDNS / the web server, and for why the position model here
// (shared Motors::lastPct "assumed state") differs from the Shutter Hub's live servo feed.

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

namespace {
bool          g_started      = false;   // bridge actually begun (enabled + set up)
volatile bool g_paired       = false;   // at least one controller paired (pair callback)
volatile bool g_resetPending = false;   // web UI asked to erase pairings
unsigned long g_resetAt      = 0;       // when to run it (lets the HTTP response flush)
volatile bool g_hapUp        = false;   // HomeSpan finished post-connect init (connect callback fired)
unsigned long g_hapWatchAt   = 0;       // when to WARN if the HAP server never came up
bool          g_hapWarned    = false;   // stall WARN already emitted?

// Strings handed to HomeSpan must outlive begin() — keep our own copies.
String g_bridgeName, g_code, g_ssid, g_psk, g_host;
String g_names[Motors::MAX];

// Position maths (raw 0-32767 <-> HA-convention percent) now live in Motors::pctToPos/
// posToPct — single source of truth shared with the web UI's calibrated Goto (Phase 2).
// Uncalibrated motors still fall back to the full protocol range there, so a motor stays
// operable from Apple Home before it's calibrated (same MVP the Shutter Hub made).

// ---- One HomeKit Window Covering, bound to a motor index ----------------------------
// No live position feed exists (connect-on-demand BLE — see HomeKit.h), so there is no
// loop() override here: Current/Target are only ever changed by a successful update().
struct DevMotor : Service::WindowCovering {
  int idx;
  SpanCharacteristic *current;
  SpanCharacteristic *target;

  DevMotor(int i) : Service::WindowCovering() {
    idx = i;
    int pct = Motors::lastPct(Motors::idAt(i));
    if (pct == Motors::UNSET) pct = 0;          // never moved yet -> assume closed
    current = new Characteristic::CurrentPosition(pct);
    target  = new Characteristic::TargetPosition(pct);
  }

  boolean update() override {
    String id  = Motors::idAt(idx);
    String mac = Motors::macAt(idx);
    String pin = Motors::pinFor(id);
    int    tp  = target->getNewVal();
    int    pos = Motors::pctToPos(id, tp);
    LOGD("homekit", "%s: target %d%% -> goto %d%s", id.c_str(), tp, pos,
         Motors::calibratedAt(idx) ? "" : " (default envelope — not calibrated)");
    if (!SomfyBle::connectAndGoto(mac, pin, pos)) {
      LOGW("homekit", "%s: BLE command failed — reverting Home app tile", id.c_str());
      return false;                             // reject: Home app reverts the tile
    }
    Motors::setLastPct(id, tp);
    current->setVal(tp);
    return true;
  }
};

// ---- HomeSpan callbacks ------------------------------------------------------
void onConnection(int count) {
  if (count) g_hapUp = true;            // reached the END of HomeSpan's post-connect init (HAP up)
  LOGI("homekit", "HomeSpan network %s", count ? "connected" : "disconnected");
}
void onPair(boolean isPaired) {
  g_paired = isPaired;
  LOGI("homekit", "%s", isPaired ? "paired with a controller" : "no controllers paired");
}
void onController() {
  LOGI("homekit", "controller list changed (%d paired)", HomeKit::controllers());
}
void onStatus(HS_STATUS s) {
  LOGD("homekit", "HomeSpan state: %s", homeSpan.statusString(s));
}
}  // namespace

namespace HomeKit {

void begin() {
  if (!AppConfig::hkEnabled()) { LOGI("homekit", "disabled"); return; }

  g_bridgeName = AppConfig::hkBridgeName();
  g_code       = AppConfig::hkSetupCode();
  g_host       = AppConfig::deviceName();
  g_ssid       = WiFi.SSID();
  g_psk        = WiFi.psk();

  LOGI("homekit", "starting HomeSpan bridge '%s' (setup %s) — HAP on port 1201",
       g_bridgeName.c_str(), g_code.c_str());

  homeSpan.setPortNum(1201);              // leave port 80 to the async web server
  homeSpan.setQRID("SOMF");               // must match the web UI's X-HM:// QR payload
  homeSpan.setHostNameSuffix("");         // hostName == deviceName -> keep <name>.local
  homeSpan.setSketchVersion(FW_VERSION);
  homeSpan.setLogLevel(0);
  homeSpan.setSerialInputDisable(true);   // don't consume the serial console we log to
  homeSpan.setWifiCallbackAll(onConnection);
  homeSpan.setPairCallback(onPair);
  homeSpan.setControllerCallback(onController);
  homeSpan.setStatusCallback(onStatus);

  // mDNS is already up: WebUI::begin() initialised the shared responder (hostname + _http._tcp)
  // on the main thread before we got here (see HomeKit.h coexistence contract). homeSpan.begin()
  // below calls mdns_init() too, but that's a cheap no-op with the responder already running —
  // it just ADDS _hap._tcp. We deliberately do NOT touch mDNS from this side.
  homeSpan.begin(Category::Bridges, g_bridgeName.c_str(), g_host.c_str(), "Somfy BLE Hub");

  homeSpan.setPairingCode(g_code.c_str(), false);
  homeSpan.setWifiCredentials(g_ssid.c_str(), g_psk.c_str());

  // Bridge accessory (AID 1).
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(g_bridgeName.c_str());

  // One Window Covering accessory per paired motor.
  int n = min(Motors::count(), (int)Motors::MAX);
  for (int i = 0; i < n; i++) {
    g_names[i] = Motors::nameAt(i);
    new SpanAccessory();
      new Service::AccessoryInformation();
        new Characteristic::Identify();
        new Characteristic::Name(g_names[i].c_str());
      new DevMotor(i);
    LOGI("homekit", "  accessory '%s' -> %s, %s",
         g_names[i].c_str(), Motors::macAt(i).c_str(),
         Motors::calibratedAt(i) ? "calibrated" : "default envelope (operable, calibrate for accuracy)");
  }
  LOGI("homekit", "bridge up — %d motor accessory(ies)", n);

  const char *c = g_code.c_str();
  LOGI("homekit", "PAIR WITH THIS CODE: %.3s-%.2s-%.3s (bridge '%s', QR id SOMF on port 1201)",
       c, c + 3, c + 5, g_bridgeName.c_str());

  // Run HomeSpan on its OWN FreeRTOS task (autoPoll) — NOT from the Arduino loop(): HAP/pairing
  // crypto can hold the CPU for long stretches and would otherwise stall MQTT/the web server.
  homeSpan.autoPoll(16384, 1, 1);        // stackSize, priority, cpu core

  g_started    = true;
  g_hapWatchAt = millis() + 10000;       // WARN if HAP init hasn't completed by 10 s (see loop())
}

void loop() {
  if (!g_started) return;

  if (!g_hapWarned && !g_hapUp && millis() >= g_hapWatchAt) {
    g_hapWarned = true;
    LOGW("homekit", "HAP server did not come up within 10s — HomeSpan poll task stalled during "
                    "network init; bridge NOT discoverable, port 1201 closed");
  }

  if (g_resetPending && millis() >= g_resetAt) {
    g_resetPending = false;
    LOGW("homekit", "erasing HomeKit pairing data and restarting");
    homeSpan.processSerialCommand("H");   // erases pairing data, then reboots
  }
}

bool running() { return g_started; }
// Authoritative: read the actual controller list (persisted in NVS), NOT the g_paired event
// flag, which only fires on a pairing *change* and so is stale after a reboot/OTA.
bool paired()  { return controllers() > 0; }

int controllers() {
  if (!g_started) return 0;
  int c = 0;
  for (auto it = homeSpan.controllerListBegin(); it != homeSpan.controllerListEnd(); ++it) c++;
  return c;
}

bool resetPairings() {
  if (!g_started) return false;
  g_resetPending = true;
  g_resetAt = millis() + 400;             // let the HTTP response flush before the reboot
  return true;
}

}  // namespace HomeKit
