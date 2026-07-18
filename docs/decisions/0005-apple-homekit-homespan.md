# 0005 — Apple HomeKit via HomeSpan

- **Status:** Accepted
- **Date:** 2026-07-18

## Context

The initial skeleton scope (project-brief.md, this repo's early commits) explicitly deferred
Apple HomeKit — "MQTT/HA only, unless a later phase asks for it." The project owner asked for it
to be added back into the skeleton: "the skeleton missed the apple home section which needs to be
added." The companion Shutter Hub project already solved this exact problem — a HomeSpan bridge
exposing one Window Covering accessory per device, including a hard-won fix for a HomeSpan 1.9.1
bug that silently prevented pairing — so the same approach applies here per
[0001-framework-reuse.md](0001-framework-reuse.md).

## Decision

Add a `HomeKit` module (ported from the Shutter Hub's, same file name) using **HomeSpan**
(`homespan/HomeSpan @ ~1.9.1`, pinned for the same core-2.0.9 compatibility reason as the
original), one Window Covering accessory per paired `Motor`. Carry over the **vendored HomeSpan
patch** (`firmware/patches/`) verbatim — same library version, same arduino-esp32 core, same bug.

## Position model deviates from the Shutter Hub

The Shutter Hub's HomeKit accessory streams a live position from `ServoController` every
`loop()` tick, because its servo driver is a continuously-addressable local PWM output. This
project's motors are BLE, connect-on-demand ([0003](0003-connect-on-demand-ble.md)) — there is no
live position feed to poll. Each accessory instead reads/writes `Motors::lastPct`, the same
"assumed state" MQTT will use once Phase 4 lands: a Home app move calls `SomfyBle` and only
updates the assumed position if the BLE command is reported as having succeeded. Until Phase 1
lands, `SomfyBle`'s calls are stubs, so accessories exist and are pairable but every move fails
(logged) — same "config now, real behavior later" shape as `Motors`/`Mqtt` already have.

## Consequences

- **Flash budget, realised:** HomeSpan + NimBLE + WiFiManager + ESPAsyncWebServer + PubSubClient
  together overflowed the default 1.28 MB OTA app partition (103.2 % — confirmed by an actual
  failed build, not just the risk noted in [0002](0002-ble-stack-nimble.md)). Fixed by switching
  `board_build.partitions` to `min_spiffs.csv` (each OTA app slot ~1.9 MB, filesystem partition
  shrunk to 128 KB — plenty for a ~60 KB web UI). `firmware/build_dist.ps1` needed **no changes**
  to keep working, because it already decodes partition offsets from the actual partition table
  instead of hardcoding them ([release-artifacts memory](../../CLAUDE.md), added the same day for
  exactly this kind of reason).
- QR setup ID is `"SOMF"` (was `"SHUT"` on the Shutter Hub) — must stay in sync between
  `HomeKit.cpp`'s `setQRID()` call and `app.js`'s `hkQrUri()`.
- No per-motor "invert" setting (the Shutter Hub has one) — not built yet; Somfy's protocol
  already has a fixed 0=open/32767=closed convention per motor, so it may never be needed.
- Same mDNS ownership contract as the Shutter Hub: `WebUI::begin()` always calls `MDNS.begin()`
  first, unconditionally — HomeSpan's own `mdns_init()` (inside `homeSpan.begin()`) becomes a
  cheap no-op once the responder is already running, and just adds `_hap._tcp`. Deferring mDNS
  init to HomeSpan instead hangs (the original v0.7.0 bug) — don't "fix" this the other way.
