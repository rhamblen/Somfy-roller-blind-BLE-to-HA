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
