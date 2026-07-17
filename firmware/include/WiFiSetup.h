// WiFiSetup — on-device WiFi provisioning (WiFiManager AP + captive portal).
//
// First boot (or after forgetAndReboot) raises the "Somfy-BLE-Setup" access
// point with a captive portal; the chosen network is stored in NVS and survives
// OTA updates.
#pragma once
#include <Arduino.h>

namespace WiFiSetup {
void connect();          // blocks until connected (opens the portal if unprovisioned)
void forget();           // clear saved WiFi from NVS (does NOT restart — caller reboots)
void forgetAndReboot();  // clear saved WiFi from NVS and restart into the setup AP

// Switch to a different network in place (no reboot). Persists the new credentials
// on success; on failure reverts to the previous network. Returns true if joined.
bool connectTo(const String &ssid, const String &pass);
}
