# Project Brief — Somfy Roller Blind BLE → Home Assistant

## Goal

Control Somfy Sonesse2 Zigbee roller-blind motor(s) from Home Assistant over MQTT, without a
Zigbee coordinator, a TaHoma Pro account, or a Somfy remote — by talking to the motors' secondary
BLE GATT interface directly from an ESP32.

## Why BLE instead of Zigbee

The motors are Zigbee devices at heart, but they also expose a BLE interface — intended for
TaHoma Pro / installer recovery tooling — that accepts movement and configuration commands after
PIN authentication (PIN printed on the motor label). This project uses that interface as the
**primary** control path, bypassing Zigbee entirely. See
[architecture.md](architecture.md) for the trade-offs this implies (no Zigbee mesh, no
group-cast, connect-on-demand BLE per command).

## Hardware

| Component | Choice |
| --------- | ------ |
| Controller | ESP32 DevKit (WROOM), `esp32dev` PlatformIO board |
| Target device | Somfy Sonesse2 Zigbee roller-blind motor(s) |
| Auth | 3-byte PIN, printed on the motor label, entered once via the web UI |

## Protocol summary

Reverse-engineered by [mrmelair](https://mrmelair.com/2024/07/10/hacking-the-somfy-sonesse2-zigbee-motors/),
packaged as a reference tool by
[magik6k](https://github.com/magik6k/somfy-sonesse2-ble-calib-tool-esp) (vendored at
`vendor/somfy-sonesse2-ble-calib-tool-esp` — see
[decisions/0004-vendoring-credit.md](decisions/0004-vendoring-credit.md)). All GATT
characteristics share base UUID `xxxxxxxx-cad9-46c6-a2ea-2ca16d57b4a5`; the ones this project
needs first (Service `00000000`, Operational):

| UUID prefix | Function | Payload |
| --- | --- | --- |
| `0000000b` | Authenticate | PIN as 3-byte little-endian int |
| `00000005` | Go to position (lift) | 16-bit LE, 0 = open, 32767 = closed |
| `00000006` | Stop | `0x01` |
| `00000001` | Identify (jog) | `0x01` — useful for "is this the right motor" during pairing |

Full map (including tilt, configuration, and Zigbee-network characteristics) is in the
submodule's own README.

## Integration surface

Each configured motor is exposed to Home Assistant as one `cover` entity via MQTT discovery
(open/close/stop + position 0–100), following the same topic-contract shape the Shutter Hub
project uses (see its [0005-mqtt-command-structure.md](https://github.com/rhamblen/esp32-shutter-hub/blob/main/docs/decisions/0005-mqtt-command-structure.md)
for the pattern this is modeled on) — adapted for BLE's connect-on-demand model
([decisions/0003](decisions/0003-connect-on-demand-ble.md)).

## Out of scope (for now)

- Zigbee interoperability (this project never joins the Zigbee network).
- Tilt/venetian-specific characteristics (roller blinds only, initially).
- Apple HomeKit (no HomeSpan bridge — MQTT/HA only, unless a later phase asks for it).
