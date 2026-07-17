# Inventory

Baseline facts this project's build assumes. Fill in as hardware is confirmed.

## Controller

- **Board:** ESP32 DevKit (WROOM) — several spares on hand, exact model/flash size TBD.
- **PlatformIO board id:** `esp32dev`.

## Target motor(s)

| Field | Value |
| --- | --- |
| Model | Somfy Sonesse2 Zigbee roller-blind motor |
| Count | TBD — `Motors::MAX` will be set once known ([open decision](project-plan.md#open-decisions)) |
| BLE MAC address(es) | TBD — captured via a BLE scan during Phase 1/3 pairing |
| PIN | Printed on each motor's label; entered via the web UI, never committed to the repo |

## Reference software (not part of this repo's build)

| Item | Where | License |
| --- | --- | --- |
| BLE protocol reference tool | `vendor/somfy-sonesse2-ble-calib-tool-esp` (git submodule) | MIT (magik6k) |
| Original protocol reverse-engineering | mrmelair's blog posts (linked from the submodule's README and this project's `README.md`) | N/A (research writeup) |
