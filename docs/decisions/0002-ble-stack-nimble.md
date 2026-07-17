# 0002 — BLE stack: NimBLE-Arduino, not stock BLEDevice

- **Status:** Accepted
- **Date:** 2026-07-17

## Context

magik6k's protocol-reference tool (`vendor/somfy-sonesse2-ble-calib-tool-esp`) is a single
`.ino` sketch built against the ESP32 Arduino core's stock `BLEDevice.h` (the Bluedroid BLE
stack). It's not a library — it's an interactive serial CLI — so either way its `cmdConnect` /
`cmdAuth` / `cmdGoto` / `cmdStop` logic has to be re-implemented as callable functions in a real
module, not linked in directly. That re-implementation has to target *some* BLE client API, and
the choice isn't free: this project's non-BLE half (WiFiManager + ESPAsyncWebServer + LittleFS +
PubSubClient + a custom OTA updater) already occupies a similar flash footprint to the Shutter
Hub's, which runs at ~91–92% flash on the same `esp32dev` board — see
[0001](0001-framework-reuse.md). Bluedroid is known to have a substantially larger flash/RAM
footprint than the alternative NimBLE stack.

## Decision

Use **NimBLE-Arduino** (`h2zero/NimBLE-Arduino`) as the BLE central stack, not the stock
`BLEDevice`/Bluedroid library magik6k's reference tool uses.

## Consequences

- magik6k's GATT calls are **ported**, not copied: same characteristic UUIDs (base
  `xxxxxxxx-cad9-46c6-a2ea-2ca16d57b4a5`), same byte-level payloads (e.g. 3-byte little-endian PIN
  for `0000000b` auth, 16-bit LE position for `00000005` goto), but expressed against
  `NimBLEDevice` / `NimBLEClient` instead of `BLEDevice` / `BLEClient`. This is a mechanical
  translation, not new protocol work — the risk is transcription bugs, not protocol-discovery
  bugs, and should be checked against a real motor in Phase 1.
- Meaningfully smaller flash/RAM footprint, leaving headroom for the WiFi/MQTT/WebUI/OTA stack.
- If flash pressure ever forces a reconsideration, Bluedroid becomes the fallback (closer parity
  to the reference implementation, at a flash cost) — revisit this ADR if that happens.
