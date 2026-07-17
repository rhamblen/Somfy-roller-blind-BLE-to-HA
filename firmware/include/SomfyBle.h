// SomfyBle — connect-on-demand BLE client for Somfy Sonesse2 motors.
//
// Protocol ported from vendor/somfy-sonesse2-ble-calib-tool-esp (magik6k, MIT — see
// docs/decisions/0004-vendoring-credit.md), whose cmdConnect/cmdAuth/cmdGoto/cmdStop
// this mirrors, translated from the stock BLEDevice (Bluedroid) API to NimBLE-Arduino
// (docs/decisions/0002-ble-stack-nimble.md).
//
// Connection model: connect -> authenticate -> one command -> disconnect, never a
// held-open link (docs/decisions/0003-connect-on-demand-ble.md). Each call here is
// blocking with an internal timeout; callers (Mqtt, WebUI) should not call this from
// a context that can't afford to block briefly (a few seconds worst case).
//
// Stub for Phase S (v0.0.1): the API shape below is real and compiles against
// NimBLE-Arduino, but the bodies are TODO until Phase 1 hardware bring-up confirms
// the ported GATT calls actually work against a real motor.
#pragma once
#include <Arduino.h>

namespace SomfyBle {
void begin();   // init the NimBLE stack
void loop();    // pump any pending async BLE work (scan results, etc.)

// Position is 0-32767 per the goto-position characteristic (0 = open, 32767 = closed).
// Each call performs its own connect -> auth -> command -> disconnect cycle and
// returns false on any failure (out of range, auth rejected, GATT write failed,
// timeout) — callers should not assume partial progress.
bool connectAndGoto(const String &mac, const String &pin, int position);
bool connectAndStop(const String &mac, const String &pin);
bool connectAndIdentify(const String &mac, const String &pin);   // jog — useful during pairing

// Scan for nearby BLE devices (used by the Motors pairing UI, Phase 3). Returns a
// JSON array of {mac,name,rssi}. Non-blocking start; poll scanning()/scanResultsJson().
void   startScan(uint32_t durationMs = 5000);
bool   scanning();
String scanResultsJson();
}
