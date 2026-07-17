// Motors — per-motor definitions + calibration, persisted in NVS.
//
// Each motor is one entry: a friendly name, a BLE MAC address, a 3-byte PIN (from
// the motor label — see docs/decisions/0004-vendoring-credit.md for the protocol
// source), and calibrated open/closed positions (0-32767, per magik6k's goto-position
// characteristic). Stored in its own Preferences namespace, mirroring the Shutter
// Hub's Shutters module, so a config reset never wipes hard-won calibration or PINs.
//
// Stub for Phase S (v0.0.1): the storage/list API is real, but nothing populates it
// yet — motor pairing is Phase 3 (web UI) built on Phase 1 BLE bring-up.
#pragma once
#include <Arduino.h>

namespace Motors {
const int MAX = 8;                 // cap; raise if more motors are ever needed
const int UNSET = -1;              // sentinel for an un-calibrated position

void   begin();                    // load records from NVS
int    count();

String listJson();                 // JSON array of all motors (id,name,mac,openPos,closedPos,calibrated)
bool   exists(const String &id);

// Index-based access for iteration (MQTT discovery + state publishing, Phase 4).
int    find(const String &id);     // index 0..count()-1, or -1 if unknown
String idAt(int i);
String nameAt(int i);
String macAt(int i);
bool   calibratedAt(int i);        // both open/closed positions snapshotted

// Mutations — each persists immediately and returns success. The PIN is write-only
// from the caller's perspective (never round-tripped into listJson()).
bool   add(const String &name, const String &mac, const String &pin);   // false if full; id auto-slugged
bool   remove(const String &id);
bool   rename(const String &id, const String &name);
String pinFor(const String &id);                            // for SomfyBle to authenticate with
bool   setEdge(const String &id, bool openEdge, int pos);    // snapshot open/closed; calibrated once both set
int    edgePos(const String &id, bool openEdge);             // calibrated open/closed position, or UNSET
}
