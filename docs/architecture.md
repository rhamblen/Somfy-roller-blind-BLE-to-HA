# Architecture

## Principles

- **The repo is the source of truth for intent + rationale.** Update docs when decisions change;
  significant choices get an ADR in [decisions/](decisions/).
- **Reuse the proven framework wholesale** for everything that isn't BLE-specific —
  see [decisions/0001-framework-reuse.md](decisions/0001-framework-reuse.md). Don't re-solve WiFi
  provisioning, OTA safety, or the web UI shell from scratch.
- **Connect-on-demand BLE, never a held-open link** — see
  [decisions/0003-connect-on-demand-ble.md](decisions/0003-connect-on-demand-ble.md). A single
  ESP32 BLE central can only track a handful of connections; holding links open across multiple
  motors doesn't scale and isn't how the reference tool's protocol was designed to be driven.
- **Motor count is configuration, not code**, same convention as the Shutter Hub's shutter count —
  never hard-code a specific number of motors; cap with a `Motors::MAX` constant, raise it if
  needed.
- **Flash budget is a first-class constraint.** The non-BLE half of the stack alone runs the
  Shutter Hub to ~91–92% flash on the same board family; the BLE stack choice
  ([decisions/0002](decisions/0002-ble-stack-nimble.md)) was made specifically to leave headroom.

## Topology

```
Home Assistant  <—MQTT (cover discovery)—>  ESP32 (this project)  <—BLE, connect-on-demand—>  Somfy motor(s)
                                                    |
                                              local Web UI
                                          (config, calibration, OTA)
```

No Zigbee coordinator, no TaHoma cloud/account, no Somfy remote in the loop. The ESP32 is the only
thing that ever talks to the motors, and it only does so in short connect → auth → command →
disconnect bursts.

## Module map

Same shape as the Shutter Hub (`begin()`/`loop()` modules, NVS-backed `AppConfig`, JSON-REST +
LittleFS SPA web UI):

| Module | Role | Carried over from Shutter Hub? |
| --- | --- | --- |
| `Diagnostics` | Logging, `/info`, reboot scheduling | Verbatim |
| `WiFiSetup` | WiFiManager captive-portal provisioning | Verbatim (AP name changed) |
| `Ota` | Dual-target (`firmware`/`filesystem`) OTA over `Update.h`, decoupled from reboot | Verbatim |
| `AppConfig` | NVS-backed settings (device name, MQTT, web auth, Motors) | Adapted — dropped servo/HomeKit/light-sensor fields |
| `WebUI` | ESPAsyncWebServer + LittleFS SPA + `/ws/logs` + JSON REST + recovery page | Adapted — Motors status/routes replace Shutters/ServoController |
| `Mqtt` | PubSubClient + HA discovery, one `cover` per device | Adapted — driven by `Motors` instead of `Shutters` |
| `Motors` | NVS-backed list of paired motors + calibration | New — ported pattern from `Shutters.h` |
| `SomfyBle` | Connect-on-demand BLE client: connect/auth/goto/stop | New — protocol ported from `vendor/somfy-sonesse2-ble-calib-tool-esp`, translated to NimBLE ([0002](decisions/0002-ble-stack-nimble.md)) |

## Open trade-offs

- **Position ground truth vs assumed state** — see
  [decisions/0003](decisions/0003-connect-on-demand-ble.md). Unresolved until Phase 1 hardware
  testing; the `Mqtt` state-publish design depends on the answer.
- **NimBLE vs Bluedroid** — see [decisions/0002](decisions/0002-ble-stack-nimble.md). Chosen for
  footprint; revisit only if it turns out to be materially harder to get NimBLE talking to these
  motors than magik6k's Bluedroid-based reference.
