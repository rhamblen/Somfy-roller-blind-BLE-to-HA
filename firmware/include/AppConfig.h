// AppConfig — persisted device settings (NVS via Preferences).
//
// The single home for configuration that must survive reboots AND OTA updates
// (NVS is a separate flash partition from the app). WiFi credentials are NOT here
// — those are owned by WiFiManager in its own NVS namespace. Paired motors are NOT
// here either — see Motors, which owns its own namespace the same way the Shutter
// Hub's Shutters module does, so a config reset never wipes hard-won calibration.
#pragma once
#include <Arduino.h>

namespace AppConfig {
void     begin();                              // open NVS, load settings, bump boot count
String   deviceName();                         // used for hostname / mDNS (default "somfy-ble")
void     setDeviceName(const String &name);
uint32_t bootCount();                          // total boots (diagnostics)

// Last OTA flash record (what + when + result), persisted across reboots/OTA.
String   lastFlashType();                       // "firmware" | "filesystem" | "none"
bool     lastFlashOk();
uint32_t lastFlashEpoch();                      // unix time, 0 if the clock wasn't synced
void     recordFlash(const String &type, bool ok, uint32_t epoch);

// ---- MQTT / Home Assistant ----
// Broker connection + a base topic. clientId defaults to "somfy-ble-XXXX" (MAC
// suffix) when left blank. haDiscovery publishes Home Assistant MQTT discovery.
bool     mqttEnabled();
String   mqttHost();
uint16_t mqttPort();                            // default 1883
String   mqttClientId();                        // resolved (never blank)
String   mqttUser();
String   mqttPass();
String   mqttBaseTopic();                       // default "somfy-ble"; LWT = <base>/status
bool     mqttHaDiscovery();
void     setMqtt(bool enabled, const String &host, uint16_t port, const String &clientId,
                 const String &user, const String &pass, const String &baseTopic, bool haDiscovery);

// ---- Web interface authentication (Security tab) ----
bool     authEnabled();
String   authUser();
String   authPass();
void     setAuth(bool enabled, const String &user, const String &pass);

// ---- HomeKit / Apple Home (System > HomeKit tab) ----
// Bridge name defaults to the device name. Setup code is 8 digits; the default is
// 748-88-377 — "SHUTTERS" on a phone keypad, carried over unchanged from the Shutter
// Hub (this project doesn't need its own mnemonic; any distinct non-trivial code works).
bool     hkEnabled();
String   hkBridgeName();                        // resolved (never blank)
String   hkSetupCode();                         // digits only, e.g. "74888377"
void     setHomeKit(bool enabled, const String &name, const String &code);

// Clear all app settings (device name, MQTT, auth, HomeKit) to defaults. Does NOT touch
// WiFi credentials (WiFiManager's own NVS namespace) or paired motors (Motors' own
// namespace). Caller reboots.
void     factoryReset();
}
