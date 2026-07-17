# Project instructions — phased build/automation repo

> Repo-specific conventions for this project. Universal rules (e.g. how to
> publish a GitHub release) live in the global `~/.claude/CLAUDE.md`.

Follow the **phased-repo documentation conventions** defined in the global
`~/.claude/CLAUDE.md` (README lead + docs/, project-plan status table, ADRs
in `docs/decisions/`, `docs/ai-context.md` as the cold-start map, CHANGELOG
per phase). Same structure as this project's reference framework,
`D:\...\Shutter opener`.

## This repo specifically

- `vendor/somfy-sonesse2-ble-calib-tool-esp` is a **git submodule** (magik6k's
  BLE protocol reference tool, MIT). It is **not compiled** — it's a single
  Arduino `.ino` sketch using the stock Bluedroid `BLEDevice` library, kept
  for protocol reference and credit provenance. Our firmware re-implements
  the GATT calls against **NimBLE-Arduino** (see
  [decisions/0002-ble-stack-nimble.md](docs/decisions/0002-ble-stack-nimble.md)).
  Clone with `git clone --recurse-submodules`, or run
  `git submodule update --init` after a plain clone.
- Any README or docs change must keep magik6k (and, per his own README,
  mrmelair's original protocol reverse-engineering) credited under License &
  Credits — this was an explicit requirement from the project owner.
