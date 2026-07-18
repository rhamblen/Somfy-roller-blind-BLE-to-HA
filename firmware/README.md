# Firmware — Somfy Roller Blind BLE → HA

PlatformIO project (Arduino Core, `esp32dev`). Firmware framework carried over from the
ESP32 Smart Shutter Hub — see [../docs/decisions/0001-framework-reuse.md](../docs/decisions/0001-framework-reuse.md).
See [../docs/project-plan.md](../docs/project-plan.md) for the phased roadmap.

## What this version does (v0.1.2 — Phase S)

On-device WiFi setup, advertises `somfy-ble.local` over mDNS, and serves a single-page web
UI from LittleFS (sidebar: Info · Motors · MQTT · System · OTA · Logs) over a JSON/REST API.
A live log stream runs over WebSocket (`/ws/logs`). Custom **OTA** flashes firmware and/or
the LittleFS image independently. `Motors` and `SomfyBle` exist as real-but-empty /
stubbed modules respectively — BLE bring-up against a real motor is Phase 1.

**Apple HomeKit**: a HomeSpan bridge exposes each paired motor as a Window Covering,
configured under **System → HomeKit** — see
[../docs/decisions/0005-apple-homekit-homespan.md](../docs/decisions/0005-apple-homekit-homespan.md).
It runs on its own FreeRTOS task, so HAP never stalls the web server or MQTT. Uses a vendored
HomeSpan patch (`patches/`, applied pre-build) for a known 1.9.1 pairing bug.

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

Prebuilt bins are collected in [`dist/`](dist/). **Every build produces three bins** —
build them all in one shot with:

```
pwsh firmware/build_dist.ps1
```

`dist/` is scratch output, not an archive: by default the script **deletes any other
version's bins already sitting in `dist/`** once the new ones are built, so there's never
a stale binary next to the current one to flash by mistake. Pass `-KeepOld` to keep every
version around instead.

| File | Use |
| ---- | --- |
| `somfy-ble-esp32dev-full-vX.Y.Z.bin` | first USB flash, merged at `0x0` |
| `somfy-ble-esp32dev-ota-vX.Y.Z.bin` | OTA page → **Upload Firmware** |
| `somfy-ble-esp32dev-littlefs-vX.Y.Z.bin` | OTA page → **Upload LittleFS** |

`-ota-` is `.pio/build/esp32dev/firmware.bin` as-is (what `pio run -t upload` sends over
serial). `-littlefs-` is `.pio/build/esp32dev/littlefs.bin` from `pio run -t buildfs`.
`-full-` is bootloader + partition table + app **and the LittleFS image** merged into one
image that flashes at offset `0x0` — a single USB flash of `-full-` alone is enough to get
the complete web UI on first boot, no separate filesystem upload needed.

`build_dist.ps1` reads the version straight out of `platformio.ini`'s `FW_VERSION` (so the
filename can never drift from what's embedded in the binary) and **decodes the actual
partition table** (`gen_esp32part.py`) for the app and LittleFS offsets on every run, rather
than hardcoding them — a partition-scheme change can't silently produce a broken `-full-`
image again. (**Without the LittleFS entry in the merge, `-full-` boots with an empty
filesystem and only the plain-text OTA recovery page appears, not the real web UI** — this
bit us on the very first hardware test, v0.1.0's initial hand-built `-full-` omitted it;
that's exactly why this is a script now instead of a manual `esptool merge_bin` command.)

`dist/*.bin` is gitignored — build it, don't commit it; attach to a GitHub release when one
is cut.

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

> **Exception: if `board_build.partitions` changed since the device's last flash, OTA cannot
> apply it — do a full USB reflash instead (see above).** OTA only ever writes into the app/FS
> slots that already exist on the chip; the partition table itself is a fixed flash region OTA
> never touches. This bit us going from v0.1.1 to v0.1.2 (switched to `min_spiffs.csv`): the
> `-ota-` firmware upload failed "Not Enough Space" because it's sized for the new, larger app
> slot, which doesn't exist yet on a device still running the old table. The filesystem upload
> "succeeding" in that same session was coincidence (new image happened to be smaller than the
> old partition), not evidence OTA can partially apply a partition change.

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
├─ data/                       LittleFS web UI (index.html, style.css, app.js, qrcode.min.js)
├─ patches/                    vendored HomeSpan 1.9.1 patch, applied pre-build
├─ src/
│  ├─ main.cpp                 thin entry point — wires the modules together
│  ├─ AppConfig.cpp            persisted settings: device, MQTT, web auth, HomeKit (NVS)
│  ├─ Diagnostics.cpp          logging + log ring buffer + WS sink, /info
│  ├─ WiFiSetup.cpp            WiFiManager AP + captive portal
│  ├─ WebUI.cpp                static SPA + JSON API + /ws/logs + mDNS
│  ├─ Ota.cpp                  custom firmware + LittleFS OTA
│  ├─ Motors.cpp               per-motor definitions + calibration + assumed state (NVS) [Phase 1-2]
│  ├─ Mqtt.cpp                 hub diagnostics now; HA covers + discovery     [Phase 4]
│  ├─ HomeKit.cpp              HomeSpan bridge — one Window Covering per motor
│  └─ SomfyBle.cpp             connect-on-demand BLE client (NimBLE)          [Phase 1]
├─ build_dist.ps1              builds all 3 dist/ release bins in one step
└─ dist/                       prebuilt release artifacts (bins gitignored)
```

The web UI is static files in `data/` (LittleFS), served by `WebUI` over a JSON API +
WebSocket; the firmware falls back to an embedded recovery page if the FS image is
missing. `WebUI` is named that (not `WebServer`) to avoid clashing with the Arduino
core's `WebServer.h` that WiFiManager includes.
