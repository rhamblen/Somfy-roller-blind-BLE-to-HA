# Somfy Roller Blind BLE → Home Assistant / Apple Home

An ESP32 bridge that drives **Somfy Sonesse2 Zigbee** roller-blind motors over their
**Bluetooth Low Energy (BLE)** GATT interface and exposes them to Home Assistant (native
`cover` entities over MQTT) and Apple Home (a HomeKit Window Covering per motor) — no Zigbee
coordinator, TaHoma Pro account, or Somfy remote required.

The motors are normally paired to a Zigbee network, but they also expose a BLE interface
(intended for TaHoma Pro / installer recovery) that accepts movement and configuration commands
after PIN authentication. This project talks to that interface directly from an ESP32, connecting
on demand for each command rather than joining Zigbee at all.

## Status

**Phase S — firmware framework scaffold, v0.1.2.** The reusable base (WiFi provisioning, local
web UI, OTA, MQTT client, Apple HomeKit bridge) builds and has been verified end-to-end on real
hardware; BLE bring-up against a real motor is next. See
[docs/project-plan.md](docs/project-plan.md) for the phased roadmap.

## How it works

One ESP32 DevKit hosts a local web UI for configuration (WiFi, MQTT, paired motors), speaks
Home Assistant MQTT discovery for each configured motor, and — on a command — connects to the
target motor over BLE, authenticates with its PIN, sends the GATT command (goto position / stop),
and disconnects. The link isn't held open between commands.

Firmware architecture is a direct reuse of the module pattern from a companion project, the
**ESP32 Smart Shutter Hub**: `WiFiSetup` (WiFiManager captive portal), `WebUI` (ESPAsyncWebServer
+ LittleFS single-page app + live log WebSocket), `Ota` (custom dual-target firmware/filesystem
updater), `Mqtt` (PubSubClient + HA discovery), `HomeKit` (HomeSpan bridge, one Window Covering
per motor), all backed by an NVS `AppConfig`. See
[decisions/0001-framework-reuse.md](docs/decisions/0001-framework-reuse.md) and
[decisions/0005-apple-homekit-homespan.md](docs/decisions/0005-apple-homekit-homespan.md).

## Hardware

| Component | Choice |
| --------- | ------ |
| Controller | ESP32 DevKit (WROOM) |
| Target device | Somfy Sonesse2 Zigbee roller-blind motor(s), BLE GATT interface |
| PIN | Printed on the motor label; required for BLE authentication |

## Repo layout

| Path | Contents |
| ---- | -------- |
| [docs/project-brief.md](docs/project-brief.md) | Spec: goal, protocol summary, integration surface |
| [docs/project-plan.md](docs/project-plan.md) | Phased roadmap + status + open decisions |
| [docs/architecture.md](docs/architecture.md) | Principles, trade-offs, module map |
| [docs/ai-context.md](docs/ai-context.md) | Cold-start map for the next AI session |
| [docs/decisions/](docs/decisions/) | Architecture Decision Records |
| [docs/inventory.md](docs/inventory.md) | Motor facts (model, MAC, PIN source) |
| [firmware/](firmware/) | ESP32 firmware (PlatformIO, Arduino Core) — build/flash/OTA |
| [vendor/somfy-sonesse2-ble-calib-tool-esp](vendor/somfy-sonesse2-ble-calib-tool-esp) | magik6k's BLE protocol reference tool (git submodule) |
| [CHANGELOG.md](CHANGELOG.md) | Change history (Keep a Changelog + SemVer) |

## License & Credits

### License

This project is licensed under the MIT License — see the [`LICENSE`](LICENSE) file for details.

### Credits

The entire BLE control surface this project relies on exists because of prior reverse-engineering
work, credited here in full:

- **[magik6k/somfy-sonesse2-ble-calib-tool-esp](https://github.com/magik6k/somfy-sonesse2-ble-calib-tool-esp)**
  (MIT) — an ESP32 BLE CLI tool for configuring/recovering Somfy Sonesse2 motors. Its documented
  GATT service/characteristic map (authentication, goto-position, stop, tilt, configuration
  characteristics) is the direct basis for this project's BLE command layer. Vendored as a git
  submodule at [`vendor/somfy-sonesse2-ble-calib-tool-esp`](vendor/somfy-sonesse2-ble-calib-tool-esp)
  for reference and provenance; see
  [decisions/0004-vendoring-credit.md](docs/decisions/0004-vendoring-credit.md) for how it's used.
- **[mrmelair](https://mrmelair.com/2024/07/10/hacking-the-somfy-sonesse2-zigbee-motors/)** —
  the original reverse-engineering of the Somfy Sonesse2 BLE protocol (by decompiling the TaHoma
  Pro Android app), which magik6k's tool is itself built on. See also the
  [follow-up post](https://mrmelair.com/2024/08/22/more-ble-for-the-somfy-sonesse2-zigbee-motors/).
- **[HomeSpan](https://github.com/HomeSpan/HomeSpan)** (Apache-2.0) — the Apple HomeKit (HAP)
  library the `HomeKit` module is built on. Ships with a small vendored patch
  ([`firmware/patches/`](firmware/patches/)) for a HomeSpan 1.9.1 pairing bug.
- **[davidshimjs/qrcodejs](https://github.com/davidshimjs/qrcodejs)** (MIT) — client-side QR
  rendering for the HomeKit pairing code (`firmware/data/qrcode.min.js`).

### Disclaimer

This project sends undocumented, reverse-engineered commands to motorized window-covering
hardware. **Use at your own risk.** It is provided "as is", without warranty of any kind, and is
not affiliated with, endorsed by, or condoned by Somfy. Apple Home integration is via the
open-source [HomeSpan](https://github.com/HomeSpan/HomeSpan) HomeKit library; this project is
**not affiliated with, endorsed by, or condoned by Apple Inc.**

### Trademarks

- Somfy, Sonesse, and TaHoma are trademarks of Somfy SA.
- Apple, HomeKit, and Home are trademarks of Apple Inc.
- ESP32 is a trademark of Espressif Systems (Shanghai) Co., Ltd.
- Home Assistant is a trademark of the Open Home Foundation.
