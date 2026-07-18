## Phase S — Firmware framework scaffold

First release. This is the reusable firmware base — on-device WiFi setup, a local web UI, OTA
updates, MQTT, and an Apple HomeKit bridge — verified end-to-end on real ESP32 hardware. BLE
bring-up against an actual Somfy motor is next (Phase 1); `Motors` and `SomfyBle` exist in this
release as real-but-empty / stubbed modules so the shape is already there.

### What's in this release

- **On-device WiFi setup** — no credentials compiled in. First boot raises a `Somfy-BLE-Setup`
  captive portal; credentials persist in NVS across OTA updates.
- **Local web UI** (`http://somfy-ble.local`) — Info dashboard, MQTT config, System (device /
  WiFi / HomeKit / Security), OTA updater, and a live log viewer over WebSocket.
- **OTA updates** — a custom dual-target updater (firmware and/or filesystem, independently),
  decoupled from reboot so a failed upload can't strand the device mid-update.
- **MQTT client** — connects and publishes hub diagnostics (WiFi signal, uptime) via Home
  Assistant discovery now; per-motor `cover` discovery lands in Phase 4 once there are real
  motors to discover.
- **Apple HomeKit bridge** (HomeSpan) — one Window Covering accessory per paired motor, with a
  pairing QR code under System → HomeKit. Runs on its own task so it never stalls the web server.
- **BLE stack**: NimBLE-Arduino, chosen over the reference tool's stock Bluedroid library for
  flash footprint — see [ADR 0002](docs/decisions/0002-ble-stack-nimble.md).
- **Credits preserved**: [magik6k](https://github.com/magik6k/somfy-sonesse2-ble-calib-tool-esp)'s
  BLE protocol reference tool is vendored as a git submodule; the protocol itself traces back to
  [mrmelair](https://mrmelair.com/2024/07/10/hacking-the-somfy-sonesse2-zigbee-motors/)'s
  reverse-engineering. Full credits in [README.md](README.md).

### Hardware-verified fixes (found during first bring-up)

- Missing LittleFS partition in the `-full-` dist image (device booted with the plain recovery
  page instead of the real UI).
- A WiFi/BLE coexistence crash-loop (`abort()` in `coex_core_enable`) caused by disabling WiFi
  modem sleep past the setup portal — fatal once BLE (NimBLE) tries to come up on a single-radio
  ESP32.
- A flash overflow (103.2%) once HomeSpan was added on top of NimBLE + the web/MQTT stack —
  fixed by switching to the `min_spiffs.csv` partition scheme (~1.9 MB per OTA app slot).

### Upgrading a device already in the field

This release changed the flash partition table. **OTA cannot apply a partition-table change** —
flash `somfy-ble-esp32dev-full-v0.1.2.bin` over USB once; every update after that can go back to
normal OTA. See `firmware/README.md`'s Update-over-the-air section.

### Release artifacts

Three binaries per release, built with `firmware/build_dist.ps1`:

| File | Use |
| ---- | --- |
| `somfy-ble-esp32dev-full-v0.1.2.bin` | First flash / recovery — USB (esptool/NodeMCU-PyFlasher) at offset `0x0` |
| `somfy-ble-esp32dev-ota-v0.1.2.bin` | Web UI → OTA Update → Upload Firmware |
| `somfy-ble-esp32dev-littlefs-v0.1.2.bin` | Web UI → OTA Update → Upload LittleFS |

Full roadmap: [docs/project-plan.md](docs/project-plan.md). Full change history:
[CHANGELOG.md](CHANGELOG.md).
