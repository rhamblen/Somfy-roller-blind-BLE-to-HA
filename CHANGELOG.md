# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning follows
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-07-18

### Added
- Phase S: firmware framework scaffold — WiFiManager captive-portal
  provisioning, ESPAsyncWebServer + LittleFS single-page UI, live log
  WebSocket, custom dual-target OTA updater, MQTT client scaffold, NVS-backed
  `AppConfig`. Carried over from the Shutter Hub project's proven framework.
- Stub `Motors` and `SomfyBle` modules (compile, empty/no-op — real BLE
  bring-up is Phase 1).
- `vendor/somfy-sonesse2-ble-calib-tool-esp` added as a git submodule
  (magik6k's Somfy Sonesse2 BLE protocol reference tool, MIT).
- Repo scaffold: README, LICENSE, docs (`project-brief`, `project-plan`,
  `architecture`, `ai-context`, ADRs 0001–0004, `inventory`).

### Fixed
- First hardware test found the `-full-` dist image never included the LittleFS
  partition, so a fresh flash booted with an empty filesystem and only the
  plain-text OTA recovery page — fixed the `merge_bin` recipe to include it
  (offset `0x290000`).
- First hardware test also found `SomfyBle::begin()` hard-crashing on boot
  (`abort()` in `coex_core_enable`) because `WiFiSetup` left WiFi modem sleep
  disabled past the setup portal, breaking WiFi/BLE coexistence on the
  single-radio ESP32-WROOM. `WiFiSetup::connect()` now restores
  `WiFi.setSleep(true)` before returning. See `docs/ai-context.md` Gotchas.
