# 0001 — Reuse the Shutter Hub firmware framework

- **Status:** Accepted
- **Date:** 2026-07-17

## Context

This project needs the same non-BLE plumbing as a companion project, the ESP32 Smart Shutter
Hub: on-device WiFi provisioning with no compiled-in credentials, a local web UI for
configuration, browser-based OTA updates, and an MQTT client publishing Home Assistant discovery.
That project solved all four on real hardware over several phases (WiFiManager captive portal,
ESPAsyncWebServer + LittleFS single-page app, a custom dual-target `Update.h` OTA endpoint,
PubSubClient + HA discovery), including hard-won fixes (e.g. no-cache static serving so an OTA'd
LittleFS image isn't masked by the browser cache; decoupling OTA flash from reboot so a stalled
main loop can't strand a device mid-update).

## Decision

Carry the framework over **as the base**, module-for-module: `Diagnostics`, `WiFiSetup`, `Ota`
copied near-verbatim (rebranded name/AP SSID only); `AppConfig`, `WebUI`, `Mqtt` adapted in place
(same JSON-REST + LittleFS SPA + `guard()` auth pattern, same NVS-config style) with
shutter/servo/HomeKit/light-sensor specifics dropped in favor of Motors/BLE fields.

## Rationale

- Re-solving already-solved problems (WiFi provisioning, safe OTA, live log streaming) wastes
  effort and risks reintroducing bugs the other project already found and fixed.
- Keeping the same module shape (`begin()`/`loop()`, NVS-backed `AppConfig`, JSON REST + LittleFS
  SPA) means both projects stay mutually legible — fixes discovered in one are easy to port to
  the other.

## Consequences

- Same core library choices for the non-BLE half of the stack: `tzapu/WiFiManager`,
  `ESP32Async/AsyncTCP` + `ESP32Async/ESPAsyncWebServer`, `knolleary/PubSubClient`.
- Flash/RAM budget precedent carries over too — the Shutter Hub's near-identical base already
  runs ~91–92% flash on an `esp32dev` board before considering BLE, which is why the BLE stack
  choice ([0002](0002-ble-stack-nimble.md)) is footprint-sensitive.
- No HomeSpan/Apple HomeKit, light sensor, or servo backend in this project — those modules are
  simply not carried over.
