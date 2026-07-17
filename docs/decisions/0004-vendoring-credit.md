# 0004 — Vendoring magik6k's tool as a submodule, not copying it

- **Status:** Accepted
- **Date:** 2026-07-17

## Context

magik6k's `somfy-sonesse2-ble-calib-tool-esp` (MIT) is the entire basis for this project's BLE
command layer — its README documents the GATT service/characteristic map and confidence level
for each, reverse-engineered by decompiling the TaHoma Pro Android app (originally by mrmelair;
magik6k built the CLI tool on top of that research — see the README credits). The project owner
was explicit that this credit must be preserved prominently, "under the license and credits."

Structurally, though, the tool is a single interactive-CLI `.ino` sketch, not a library — see
[0002](0002-ble-stack-nimble.md) — so it can't be linked into the firmware build regardless of
which BLE stack is used.

## Decision

Add the upstream repo as a **git submodule** at `vendor/somfy-sonesse2-ble-calib-tool-esp`,
excluded from the PlatformIO build (`lib_deps` never points into `vendor/`). It serves as:

1. The authoritative protocol reference during `SomfyBle` implementation (UUIDs, byte payloads,
   confidence notes) — cite it in code comments where a GATT call is ported from it.
2. Durable provenance for the credit requirement — the exact commit vendored is pinned by the
   submodule reference, and `README.md` → License & Credits links straight to it.

## Consequences

- Cloning this repo requires `git clone --recurse-submodules` or a follow-up
  `git submodule update --init`; documented in `CLAUDE.md` and the install instructions.
- The submodule's own `LICENSE`/README terms travel with it — this repo's `LICENSE` file notes
  the MIT-licensed protocol-research basis and points back to `README.md` for full attribution
  (magik6k's tool, and mrmelair's original protocol reverse-engineering per magik6k's own
  credits).
- If magik6k's upstream repo changes meaningfully (new characteristics, corrected confidence
  levels), bump the submodule deliberately and note it in `CHANGELOG.md` — it's not
  auto-updating.
