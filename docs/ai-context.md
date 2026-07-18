# AI Context — cold-start orientation

Dense factual map for the next AI session. Not for end users. Read this first.

## Purpose

Control Somfy Sonesse2 Zigbee roller-blind motor(s) from Home Assistant over MQTT, by talking to
the motors' secondary BLE GATT interface directly from an **ESP32 DevKit** — no Zigbee
coordinator, TaHoma Pro account, or Somfy remote. Companion project to the (separate) ESP32 Smart
Shutter Hub, whose firmware framework this project reuses wholesale for everything that isn't
BLE-specific.

## How to work here

- The **repo is the source of truth for intent + rationale.** Update docs when decisions change.
- Framework, BLE stack, connect-on-demand model, and vendoring approach are **locked via ADRs**
  in `docs/decisions/` — don't silently re-open them; if reversing, write a new ADR.
- Motor count is **configuration, not code** — never hard-code a specific number.
- Follow the phased-repo doc conventions (status table, ADRs, CHANGELOG per phase) — same
  conventions as the Shutter Hub project, defined in the user's global `~/.claude/CLAUDE.md`.
- **Credit requirement:** any README/docs edit must keep magik6k (and, per his own README,
  mrmelair's original protocol reverse-engineering) credited under License & Credits — explicit
  requirement from the project owner, not just good practice.

## File map

| File | Contents |
| ---- | -------- |
| `README.md` | Shop-window overview + License & Credits |
| `docs/project-brief.md` | Goal, protocol summary, integration surface |
| `docs/project-plan.md` | Phased roadmap + status table + open decisions |
| `docs/architecture.md` | Principles, topology, module map, open trade-offs |
| `docs/inventory.md` | Motor facts (model, MAC, PIN source) — stub until hardware is confirmed |
| `docs/decisions/0001–0004` | ADRs: framework reuse, NimBLE stack choice, connect-on-demand BLE, vendoring/credit |
| `firmware/` | PlatformIO project (Arduino core, `esp32dev` board) |
| `firmware/data/` | LittleFS web UI (served by `WebUI`) |
| `vendor/somfy-sonesse2-ble-calib-tool-esp` | magik6k's BLE protocol reference tool — **git submodule, not compiled** (see [0004](decisions/0004-vendoring-credit.md)) |
| `CHANGELOG.md` | Keep-a-Changelog; update every phase |

## Locked facts

- **Board:** ESP32 DevKit (WROOM), PlatformIO board `esp32dev`. Several spares on hand.
- **Target:** Somfy Sonesse2 Zigbee roller-blind motor(s), controlled over BLE only (never joins
  Zigbee). Auth is a 3-byte PIN printed on the motor label, entered via the web UI (never
  committed to the repo — see `.gitignore`).
- **BLE stack:** NimBLE-Arduino, chosen over the reference tool's stock Bluedroid `BLEDevice` for
  flash/RAM footprint ([decisions/0002](decisions/0002-ble-stack-nimble.md)). GATT calls are
  ported from `vendor/somfy-sonesse2-ble-calib-tool-esp`, not copied — same UUIDs/payloads,
  different client API.
- **Connection model:** connect-on-demand, one command per connect/auth/write/disconnect cycle —
  never a held-open link ([decisions/0003](decisions/0003-connect-on-demand-ble.md)). Whether
  position is readable-back or must be tracked as assumed state is **still open**, pending Phase
  1 hardware testing.
- **Protocol:** base UUID `xxxxxxxx-cad9-46c6-a2ea-2ca16d57b4a5`. Key characteristics: `0000000b`
  auth (3-byte LE PIN), `00000005` goto-position (16-bit LE, 0=open/32767=closed), `00000006`
  stop (`0x01`), `00000001` identify/jog (`0x01`). Full map in the submodule's README.
- **Firmware framework:** carried over from the Shutter Hub project
  ([decisions/0001](decisions/0001-framework-reuse.md)) — WiFiManager captive portal,
  ESPAsyncWebServer + LittleFS SPA + `/ws/logs`, custom dual-target `Update.h` OTA (decoupled from
  reboot), PubSubClient + HA MQTT discovery, NVS `AppConfig`. No HomeKit/HomeSpan, no light
  sensor, no servo backend — those modules were not carried over.

## Build phases

**S** (v0.1.0, done): framework scaffold — WiFi/WebUI/OTA/MQTT-client build and boot; `Motors` and
`SomfyBle` exist as stub modules (empty list / TODO calls) so the shape is in place. Verified with
`pio run -e esp32dev` (75.2% flash / 18.6% RAM) and `-t buildfs`; **not yet flashed to real
hardware** — do that before starting Phase 1. Remaining:
**1** (v0.2.0) BLE bring-up (real motor, resolve position-readback question) → **2** (v0.3.0)
Motors config + calibration → **3** (v0.4.0) Motors web UI → **4** (v0.5.0) MQTT/HA cover
integration. See [project-plan.md](project-plan.md) for detail.

## Versioning

Firmware version starts at **v0.1.0** (not v0.0.x) and is embedded in every build via
`-D FW_VERSION` in `firmware/platformio.ini`, surfaced in the web UI (sidebar footer, Info page,
OTA page) and `/api/info` — so the running version is always visible without a serial connection.
This deviates from the phase→version mapping convention in the user's global CLAUDE.md (which
would start Phase S at v0.0.x); that deviation is deliberate for this repo, per an explicit
instruction from the project owner.

Every release also builds **three** `firmware/dist/` bins (gitignored, built on demand) —
`-full-` (merged, for a NodeMCU-style USB flash tool), `-ota-` (plain firmware, for the web
UI's OTA page), `-littlefs-` (web UI's LittleFS OTA upload). See `firmware/README.md`'s Build
section for the exact `esptool merge_bin` recipe — another explicit project-owner requirement.

## Gotchas

- **WiFi/BLE coexistence: never leave `WiFi.setSleep(false)` on past the setup portal.**
  Root-caused on first hardware test (2026-07-18, v0.1.0): the device booted, joined WiFi, and
  started the web server fine, then hard-crashed (`abort()` in `coex_core_enable`, decoded via
  `xtensa-esp32-elf-addr2line` against `firmware.elf`) the moment `SomfyBle::begin()` called
  `NimBLEDevice::init()` — and kept crash-looping, which looked like "no web page" from the
  client side even though the server had genuinely started (see the boot log). A classic
  single-radio ESP32 (WROOM) time-shares one antenna between WiFi and BLE via a coexistence
  scheduler that depends on WiFi modem sleep being available; `WiFiSetup.cpp` (carried over from
  the Shutter Hub, which has no BLE) disabled sleep permanently for a snappy captive portal. Fix:
  `WiFiSetup::connect()` now restores `WiFi.setSleep(true)` right after leaving AP mode, before
  anything touches BLE. If you ever add another `WiFi.setSleep(false)` call anywhere (e.g. for
  portal responsiveness), make sure it's re-enabled before `SomfyBle::begin()` runs.
- The reference tool (`vendor/`) is a single interactive-CLI `.ino`, **not a library** — never try
  to `#include` it directly; port the specific GATT call you need into `SomfyBle`.
- Clone with `git clone --recurse-submodules`, or `git submodule update --init` afterward, or
  `vendor/` will be an empty directory.
- PIN and motor MAC addresses are runtime config (web UI → NVS), never compiled in or committed —
  see `.gitignore`.
- Don't hold a BLE connection open across commands or across multiple motors — see
  [decisions/0003](decisions/0003-connect-on-demand-ble.md).
