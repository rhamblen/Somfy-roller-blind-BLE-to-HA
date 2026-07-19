## Phase 2 — Motors calibration

Builds on Phase 1's BLE bring-up (v0.2.0) with a calibration workflow so a motor's raw
0–32767 goto range can be mapped to the standard 0–100% position scale.

⚠️ **Not yet verified against real hardware.** Neither this release nor v0.2.0 has been
confirmed to actually move a physical Somfy motor — both were written and published as
development checkpoints ahead of that test, since calibration is pure storage/math/UI
with no new BLE calls of its own. Treat both as snapshots, not confirmed-working
releases, until Identify/Goto/Stop are tested against real hardware.

### What's in this release

- **`Motors::pctToPos`/`posToPct`** — the raw (0–32767) ↔ percent (0–100, Home
  Assistant convention: 0 = closed) conversion, now a single shared implementation in
  `Motors` instead of a private copy that only `HomeKit` had. Falls back to the full
  protocol range when a motor isn't calibrated yet, so it stays operable either way.
- **Web UI calibration controls** (Motors page, per motor): **Set Open** / **Set
  Closed** snapshot whatever raw position is currently in the Goto field as that
  calibration edge. There's no BLE position readback in this protocol, so "jog with
  Goto, then mark the edge" is the only calibration method available. A **Goto %**
  control sends a 0–100% position through the calibration math.

### Upgrading a device already in the field

No partition-table change this release — normal OTA (web UI → OTA Update → Upload
Firmware, then Upload LittleFS) works.

### Release artifacts

| File | Use |
| ---- | --- |
| `somfy-ble-esp32dev-full-v0.3.0.bin` | First flash / recovery — USB (esptool/NodeMCU-PyFlasher) at offset `0x0` |
| `somfy-ble-esp32dev-ota-v0.3.0.bin` | Web UI → OTA Update → Upload Firmware |
| `somfy-ble-esp32dev-littlefs-v0.3.0.bin` | Web UI → OTA Update → Upload LittleFS |

Full roadmap: [docs/project-plan.md](docs/project-plan.md). Full change history:
[CHANGELOG.md](CHANGELOG.md).
