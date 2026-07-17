# Firmware — Somfy Roller Blind BLE → HA

PlatformIO project (Arduino Core, `esp32dev`). Firmware framework carried over from the
ESP32 Smart Shutter Hub — see [../docs/decisions/0001-framework-reuse.md](../docs/decisions/0001-framework-reuse.md).
See [../docs/project-plan.md](../docs/project-plan.md) for the phased roadmap.

## What this version does (v0.1.0 — Phase S)

On-device WiFi setup, advertises `somfy-ble.local` over mDNS, and serves a single-page web
UI from LittleFS (sidebar: Info · Motors · MQTT · System · OTA · Logs) over a JSON/REST API.
A live log stream runs over WebSocket (`/ws/logs`). Custom **OTA** flashes firmware and/or
the LittleFS image independently. `Motors` and `SomfyBle` exist as real-but-empty /
stubbed modules respectively — BLE bring-up against a real motor is Phase 1.

## WiFi setup (on-device — no credentials in the binary)

On first boot (and after **Reset WiFi**) the hub becomes an access point
**`Somfy-BLE-Setup`** with a captive portal:

1. Join the `Somfy-BLE-Setup` network from a phone or laptop.
2. The captive portal opens (or browse to `http://192.168.4.1`). Pick your WiFi and
   enter the password.
3. The hub saves the credentials to **NVS** and reconnects. NVS is a separate flash
   partition, so the credentials **survive OTA updates** — you only do this once.

To change networks later, use **System → WiFi** (scan/connect in place); **Reset WiFi**
in the System page's Quick Actions reboots back into this setup portal.

## Build

```
pio run                    # builds the default (and only) env, esp32dev
pio run -t buildfs         # builds the LittleFS image from data/
```

The firmware version is embedded via `-D FW_VERSION` in `platformio.ini` and surfaced in
the web UI (sidebar footer, Info page, OTA page) and `/api/info` — bump it there when
cutting a release, and keep `docs/project-plan.md` / `docs/ai-context.md` / `CHANGELOG.md`
in sync (see the versioning note in `docs/ai-context.md`).

Prebuilt bins are collected in [`dist/`](dist/). A release ships **three bins**:

| File | Use |
| ---- | --- |
| `somfy-ble-esp32dev-full-vX.Y.Z.bin` | first USB flash, merged at `0x0` |
| `somfy-ble-esp32dev-ota-vX.Y.Z.bin` | OTA page → **Upload Firmware** |
| `somfy-ble-esp32dev-littlefs-vX.Y.Z.bin` | OTA page → **Upload LittleFS** |

`-ota-` is `.pio/build/esp32dev/firmware.bin` as-is (what `pio run -t upload` sends over
serial). `-littlefs-` is `.pio/build/esp32dev/littlefs.bin` from `pio run -t buildfs`.
`-full-` is those two merged with the bootloader and partition table into one image that
flashes at offset `0x0` — the merge recipe:

```
esptool.py --chip esp32 merge_bin --flash_mode dio --flash_freq 40m --flash_size 4MB \
  -o dist/somfy-ble-esp32dev-full-vX.Y.Z.bin \
  0x1000  .pio/build/esp32dev/bootloader.bin \
  0x8000  .pio/build/esp32dev/partitions.bin \
  0xe000  <arduino-esp32 core>/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32dev/firmware.bin
```

(Exact offsets and the `boot_app0.bin` path come from `pio run -t upload -v`, which prints
the underlying `esptool.py write_flash` command.) `dist/*.bin` is gitignored — build it,
don't commit it; attach to a GitHub release when one is cut.

## First flash (USB — one time only)

After this, every update goes over WiFi (OTA below).

**Option A — PlatformIO, board on USB:**
```
pio run -t upload      # auto-detects the port; monitor with: pio device monitor
```

**Option B — single merged image (NodeMCU-PyFlasher / esptool):**
```
esptool --chip esp32 write_flash 0x0 dist/somfy-ble-esp32dev-full-vX.Y.Z.bin
```
In NodeMCU-PyFlasher: select the `-full-` bin, address `0x0`, flash.

## Update over the air (every time after the first)

The **OTA Update** page at `http://somfy-ble.local/` shows the installed version, chip,
last flash, and two uploaders — **Firmware** and **Filesystem (LittleFS)** — with an
upload log.

1. Build new bins: `pio run` (firmware) and `pio run -t buildfs` (filesystem).
2. **Upload Firmware** with the app bin; **Upload LittleFS** when the web UI (`data/`)
   changed. Upload the filesystem first if both changed, then the firmware.
3. Hit **Reboot** — flashing is decoupled from restarting, so both can be uploaded
   in any order before applying. Saved WiFi and settings (NVS) are untouched.

## Layout

```
firmware/
├─ platformio.ini              board/env + libraries + build flags
├─ include/                    module headers (*.h)
├─ data/                       LittleFS web UI (index.html, style.css, app.js)
├─ src/
│  ├─ main.cpp                 thin entry point — wires the modules together
│  ├─ AppConfig.cpp            persisted settings: device, MQTT, web auth (NVS)
│  ├─ Diagnostics.cpp          logging + log ring buffer + WS sink, /info
│  ├─ WiFiSetup.cpp            WiFiManager AP + captive portal
│  ├─ WebUI.cpp                static SPA + JSON API + /ws/logs + mDNS
│  ├─ Ota.cpp                  custom firmware + LittleFS OTA
│  ├─ Motors.cpp               per-motor definitions + calibration (NVS)      [Phase 1-2]
│  ├─ Mqtt.cpp                 hub diagnostics now; HA covers + discovery     [Phase 4]
│  └─ SomfyBle.cpp             connect-on-demand BLE client (NimBLE)          [Phase 1]
└─ dist/                       prebuilt release artifacts (bins gitignored)
```

The web UI is static files in `data/` (LittleFS), served by `WebUI` over a JSON API +
WebSocket; the firmware falls back to an embedded recovery page if the FS image is
missing. `WebUI` is named that (not `WebServer`) to avoid clashing with the Arduino
core's `WebServer.h` that WiFiManager includes.
