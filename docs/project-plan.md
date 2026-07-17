# Project Plan — Somfy Roller Blind BLE → Home Assistant

Phased roadmap. See [project-brief.md](project-brief.md) for the full spec and
[inventory.md](inventory.md) for motor/hardware facts.

## Status

☐ not started · ◐ in progress · ☑ done

| Phase | Version | Title | Status |
| ----- | ------- | ----- | ------ |
| S | v0.1.0 | Firmware framework scaffold (WiFi/WebUI/OTA/MQTT, stub BLE/Motors) | ☑ |
| 1 | v0.2.0 | BLE bring-up: scan, connect, PIN auth, single-motor goto/stop (bench test) | ☐ |
| 2 | v0.3.0 | Motors config + calibration (open/closed limits, like a shutter's edge calibration) | ☐ |
| 3 | v0.4.0 | Web UI: Motors tab (add/scan/pair/calibrate) | ☐ |
| 4 | v0.5.0 | MQTT / Home Assistant cover integration + discovery | ☐ |
| 5+ | — | Multi-motor scale-out, favourites, tilt support (if a venetian motor is added) | ☐ |

## Open decisions

- **Position readback vs assumed state** (blocks Phase 4 design) — whether the motor's position
  characteristic is readable for ground truth, or write-only, forcing HA state to be computed
  from the last commanded position. See
  [decisions/0003-connect-on-demand-ble.md](decisions/0003-connect-on-demand-ble.md). Verify in
  Phase 1 against real hardware.
- **Motor count cap** — how many motors `Motors::MAX` should allow. Deferred until the user
  confirms how many Somfy motors this will control.

---

## Phase S — Firmware framework scaffold (v0.1.0) ☑

- **Objective:** a buildable base image with on-device WiFi setup, browser control, and OTA,
  carried over from the Shutter Hub project, before any BLE work.
- **What we built:** PlatformIO project (`esp32dev`, Arduino Core); WiFiManager AP
  `Somfy-BLE-Setup` + captive portal storing creds in NVS; mDNS; ESPAsyncWebServer + LittleFS
  single-page UI shell with a live log WebSocket; custom dual-target (`firmware`/`filesystem`)
  OTA updater decoupled from reboot; MQTT client scaffold (`Mqtt` module, no discovery yet — no
  motors configured); stub `Motors` (empty NVS-backed list) and `SomfyBle` (NimBLE wrapper, calls
  are TODO) modules wired into `main.cpp` so the shape is there for Phase 1.
- **Why:** re-solving WiFi provisioning / safe OTA / a live web UI from scratch would waste effort
  the Shutter Hub project already spent — see
  [decisions/0001-framework-reuse.md](decisions/0001-framework-reuse.md).
- **Exit criteria:** ☑ compiles; ☐ flashed to a spare ESP32 DevKit and verified end-to-end
  (WiFi join, web UI loads, OTA round-trip) — do this before starting Phase 1.

## Phase 1 — BLE bring-up (v0.2.0)

- **Objective:** prove the ported GATT calls work against a real motor.
- **What we'll build:** `SomfyBle::connect/authenticate/gotoPosition/stop` implemented for real
  against `NimBLEClient`, exercised from a bench/serial test path (no web UI yet). Resolve the
  position-readback open decision above.
- **Prerequisites:** a Somfy Sonesse2 motor, its BLE MAC address, and the PIN from its label.
- **Exit criteria:** a real motor moves to a commanded position and stops on command, from the
  ESP32, over BLE, with the link closed between commands.

## Phase 2 — Motors config + calibration (v0.3.0)

- **Objective:** persist paired motors and their calibration, mirroring the Shutter Hub's
  per-shutter edge-calibration pattern (`Shutters::edgeUs` → here, motor open/closed limits).

## Phase 3 — Web UI: Motors tab (v0.4.0)

- **Objective:** add/scan/pair/calibrate motors from the browser, replacing the bench test path.

## Phase 4 — MQTT / Home Assistant integration (v0.5.0)

- **Objective:** each configured motor appears in HA as a `cover` via MQTT discovery, following
  the topic-contract shape from the Shutter Hub's ADR 0005, adapted for connect-on-demand BLE.
