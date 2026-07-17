// Mqtt — MQTT client + Home Assistant integration.
//
// Config lives in AppConfig (broker, credentials, base topic, HA discovery on/off).
// Phase S (v0.0.1): connects, publishes availability (LWT) + hub diagnostic sensors
// (rssi, uptime) via HA discovery, same pattern as the Shutter Hub's Mqtt module
// (docs/decisions/0001-framework-reuse.md). Per-motor `cover` discovery and command
// handling is Phase 4, once Motors + SomfyBle are real — see
// docs/decisions/0003-connect-on-demand-ble.md for why that design is still open.
//
// Uses PubSubClient (blocking). Safe because the web UI is served by the async
// web server on its own task — a stalled broker never freezes the interface.
#pragma once
#include <Arduino.h>

namespace Mqtt {
void   begin();           // load config; connection is established lazily in loop()
void   loop();            // pump the client + reconnect + publish hub state
void   reconfigure();     // re-read AppConfig and reconnect (call after the MQTT page applies)
void   motorsChanged();   // flag a motor config mutation — refresh discovery on the next loop()
                          // (safe to call from the web task; no-op until Phase 4 wires it up)

bool   connected();
String stateText();       // "disabled" | "connecting" | "connected" | "error: ..."
}
