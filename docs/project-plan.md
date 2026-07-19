# Project Plan — Somfy Roller Blind BLE → Home Assistant

Phased roadmap. See [project-brief.md](project-brief.md) for the full spec and
[inventory.md](inventory.md) for motor/hardware facts.

## Status

☐ not started · ◐ in progress · ☑ done

| Phase | Version | Title | Status |
| ----- | ------- | ----- | ------ |
| S | v0.1.0 | Firmware framework scaffold (WiFi/WebUI/OTA/MQTT, stub BLE/Motors) | ☑ |
| 1 | v0.2.0 | BLE bring-up: connect, PIN auth, goto/stop/identify via the web UI's Motors page | ◐ |
| 2 | v0.3.0 | Motors calibration (open/closed limits, like a shutter's edge calibration) | ◐ |
| 3 | v0.4.0 | Web UI: BLE scan + pairing flow (add/remove/rename shipped in Phase 1) | ☐ |
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

## Phase S — Firmware framework scaffold (v0.1.0 → v0.1.2) ☑

- **Objective:** a buildable base image with on-device WiFi setup, browser control, and OTA,
  carried over from the Shutter Hub project, before any BLE work.
- **What we built:** PlatformIO project (`esp32dev`, Arduino Core, `min_spiffs.csv` partitions);
  WiFiManager AP `Somfy-BLE-Setup` + captive portal storing creds in NVS; mDNS; ESPAsyncWebServer
  + LittleFS single-page UI shell with a live log WebSocket; custom dual-target
  (`firmware`/`filesystem`) OTA updater decoupled from reboot; MQTT client scaffold (`Mqtt`
  module, no discovery yet — no motors configured); stub `Motors` (empty NVS-backed list) and
  `SomfyBle` (NimBLE wrapper, calls are TODO) modules wired into `main.cpp` so the shape is there
  for Phase 1; **Apple HomeKit bridge** (`HomeKit` module, HomeSpan, vendored patch) exposing one
  Window Covering per paired motor — see
  [decisions/0005-apple-homekit-homespan.md](decisions/0005-apple-homekit-homespan.md).
  `firmware/build_dist.ps1` builds all three release bins in one step.
- **Why:** re-solving WiFi provisioning / safe OTA / a live web UI from scratch would waste effort
  the Shutter Hub project already spent — see
  [decisions/0001-framework-reuse.md](decisions/0001-framework-reuse.md).
- **Exit criteria:** ☑ compiles; ☑ flashed to a spare ESP32 DevKit and verified end-to-end (WiFi
  join, web UI loads over the home network) on 2026-07-18 — two hardware bugs found and fixed
  (missing LittleFS in the `-full-` image; a WiFi/BLE coexistence crash) — see CHANGELOG v0.1.1.
  HomeKit pairing itself is unverified (no motors yet to make it meaningful) — verify once Phase
  1-3 land real motors, don't block on it now.
- **Released:** [v0.1.2](https://github.com/rhamblen/Somfy-roller-blind-BLE-to-HA/releases/tag/v0.1.2)
  on 2026-07-18 — first GitHub Release, per the section-completion release policy
  (`docs/ai-context.md` Versioning). Phase S is closed; Phase 1 starts at v0.2.0.

## Phase 1 — BLE bring-up (v0.2.0) ◐

- **Objective:** prove the ported GATT calls work against a real motor.
- **What we built:** `SomfyBle::connectAndGoto/connectAndStop/connectAndIdentify` implemented for
  real against `NimBLEClient` — exact transcription of the vendored reference's wire protocol
  (PIN → 3-byte LE, goto → 2-byte LE position, stop/identify → single `0x01` byte, write-with-
  response-then-fallback). Exercised from the web UI's **Motors** page rather than a disposable
  serial CLI — add a motor (name/MAC/PIN), then Identify/Goto/Stop buttons per motor
  (`POST /api/motors/{add,remove,rename,goto,stop,identify}`). Pulled forward add/remove/rename
  from Phase 3, which now shrinks to just the BLE scan + pairing flow.
- **Prerequisites:** a Somfy Sonesse2 motor, its BLE MAC address, and the PIN from its label.
- **Exit criteria:** ☑ compiles (69.4% flash / 22.2% RAM); ☐ a real motor moves to a commanded
  position and stops on command, from the ESP32, over BLE, with the link closed between commands
  — **unverified, awaiting hardware test.** Resolve the position-readback open decision above once
  tested (does `UUID_GOTO_POS`/tilt chars read back, or is HA state assumed-only?).
- **Released:** [v0.2.0](https://github.com/rhamblen/Somfy-roller-blind-BLE-to-HA/releases/tag/v0.2.0)
  on 2026-07-19, marked **pre-release** on GitHub since hardware verification is still outstanding
  — published at the project owner's explicit request, ahead of the usual "release on exit
  criteria met" policy. Un-mark pre-release once the hardware test passes.

## Phase 2 — Motors calibration (v0.3.0) ◐

- **Objective:** persist each motor's open/closed limits and map raw goto positions to a 0–100%
  scale, mirroring the Shutter Hub's per-shutter edge-calibration pattern (`Shutters::edgeUs` →
  here, `Motors::edgePos`, already scaffolded).
- **What we built:** `Motors::pctToPos/posToPct` — the shared raw↔percent math, moved out of a
  private `HomeKit.cpp` helper into `Motors` so both `HomeKit` and the web UI use the same source
  of truth (falls back to the full 0–32767 range when uncalibrated, so a motor stays operable
  before calibration, same as the Shutter Hub's MVP). Web UI: **Set Open** / **Set Closed**
  buttons snapshot whatever raw position is in the Goto field — there's no BLE position readback
  (ADR 0003), so jog-then-mark is the only calibration method available. A **Goto %** control
  sends a 0–100% position through the calibration math.
- **Exit criteria:** ☑ compiles (69.5% flash / 22.2% RAM); ☐ unverified — same hardware-test
  blocker as Phase 1 (calibration is meaningless if `SomfyBle`'s goto doesn't actually move the
  motor). Verify both phases together once hardware testing happens.
- **Released:** [v0.3.0](https://github.com/rhamblen/Somfy-roller-blind-BLE-to-HA/releases/tag/v0.3.0)
  on 2026-07-19, marked **pre-release** — same rationale as v0.2.0 above.

## Phase 3 — Web UI: BLE scan + pairing flow (v0.4.0)

- **Objective:** replace manually typing in a MAC address (Phase 1) with a BLE scan + pick-a-device
  pairing flow. Add/remove/rename and the bench-test bar already shipped in Phase 1.

## Phase 4 — MQTT / Home Assistant integration (v0.5.0)

- **Objective:** each configured motor appears in HA as a `cover` via MQTT discovery, following
  the topic-contract shape from the Shutter Hub's ADR 0005, adapted for connect-on-demand BLE.
