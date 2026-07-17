# 0003 — Connect-on-demand BLE, not a held-open link

- **Status:** Accepted
- **Date:** 2026-07-17

## Context

The Somfy motors' BLE interface is a secondary/recovery channel alongside their primary Zigbee
radio (per magik6k's README: it's the interface TaHoma Pro / installer tools use, gated by a
PIN printed on the motor label). magik6k's reference CLI models an interactive session: connect
once, authenticate, issue however many commands the operator wants, disconnect when done. This
project instead needs to react to Home Assistant MQTT commands that can arrive at any time, for
any number of configured motors, from a single ESP32.

## Decision

Connect on demand, per command: **connect → authenticate (PIN) → write the GATT command →
disconnect**, mirroring the session shape magik6k's tool uses but automated and scoped to one
action. The BLE link is never held open between commands.

## Consequences

- No persistent per-motor connection state to manage across multiple simultaneous targets — a
  real constraint, since a single ESP32 BLE central can only maintain a small number of
  concurrent connections.
- Every command needs its own connect/auth/write/disconnect cycle with a timeout and retry
  (motor out of range, PIN rejected, GATT write failure) — `SomfyBle` owns this, not `Mqtt` or
  `Motors`.
- **Open question, revisit after Phase 1 hardware bring-up:** whether the position/state
  characteristics (`00000005` goto-position, `00000007`/`00000008`/`00000009` tilt) are readable
  for a readback-on-connect, or write-only as magik6k's confidence table suggests for some of
  them. If write-only, Home Assistant's reported position/state has to be **assumed state**
  (computed from the last commanded position plus calibration limits, like the Shutter Hub's
  µs-based cover position) rather than ground truth read from the motor. This materially affects
  `Mqtt`'s state-publish logic and needs to be settled with real hardware before Phase 4.
