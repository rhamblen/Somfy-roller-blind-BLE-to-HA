#include "SomfyBle.h"
#include "Diagnostics.h"
#include <NimBLEDevice.h>

// GATT map — Service 00000000 (Operational), ported from
// vendor/somfy-sonesse2-ble-calib-tool-esp's somfy-ble-cli.ino (magik6k, MIT — see
// docs/decisions/0004-vendoring-credit.md), whose UUIDs, byte payloads, and write
// pattern (write-with-response, fall back to write-without-response) this mirrors.
namespace {
const NimBLEUUID UUID_IDENTIFY ("00000001-cad9-46c6-a2ea-2ca16d57b4a5");  // 0x01 -> jog
const NimBLEUUID UUID_GOTO_POS ("00000005-cad9-46c6-a2ea-2ca16d57b4a5");  // 16-bit LE, 0=open..32767=closed
const NimBLEUUID UUID_STOP     ("00000006-cad9-46c6-a2ea-2ca16d57b4a5");  // 0x01
const NimBLEUUID UUID_AUTH     ("0000000b-cad9-46c6-a2ea-2ca16d57b4a5");  // PIN, 3-byte LE

NimBLEClient *g_client = nullptr;   // created once, reused across connections (mirrors the
                                    // reference's single pClient created in setup())

// Find `uuid` in any of the connected peer's services and write to it — the reference
// doesn't assume which service the characteristic lives in either (bleWriteFlexible).
// Try with-response first; NimBLE-Arduino reports failure via a bool return (no
// exceptions), so the with/without-response fallback is a plain retry, not a catch.
bool writeChar(const NimBLEUUID &uuid, const uint8_t *data, size_t len) {
  if (!g_client || !g_client->isConnected()) {
    LOGW("ble", "writeChar %s: not connected", uuid.toString().c_str());
    return false;
  }
  auto *services = g_client->getServices();
  for (auto *svc : *services) {
    NimBLERemoteCharacteristic *chr = svc->getCharacteristic(uuid);
    if (!chr) continue;
    if (chr->writeValue(data, len, true))  return true;   // with response
    if (chr->writeValue(data, len, false)) return true;   // without response
    LOGW("ble", "write %s failed (found char, write rejected)", uuid.toString().c_str());
    return false;
  }
  LOGW("ble", "characteristic %s not found on this device", uuid.toString().c_str());
  return false;
}

bool doConnect(const String &mac) {
  if (!g_client) g_client = NimBLEDevice::createClient();
  if (g_client->isConnected()) g_client->disconnect();   // never stack connections (ADR 0003)

  LOGD("ble", "connecting to %s", mac.c_str());
  g_client->setConnectTimeout(10);   // seconds
  NimBLEAddress addr(std::string(mac.c_str()));   // defaults to a public address, same as the
                                                   // reference's BLEAddress(mac) — try
                                                   // BLE_ADDR_RANDOM here first if connects
                                                   // reliably fail on real hardware.
  if (!g_client->connect(addr)) {
    LOGW("ble", "connect to %s failed (timeout or out of range)", mac.c_str());
    return false;
  }
  LOGD("ble", "connected to %s", mac.c_str());
  return true;
}

// PIN as a 3-byte little-endian int, written to UUID_AUTH — exact transcription of the
// reference's `auth` command. No read-back/ack exists in the protocol; success here just
// means the write was accepted, not that the PIN was correct (the next command's result
// is the real signal, same caveat the reference CLI has).
bool doAuth(const String &pin) {
  uint32_t p = (uint32_t)pin.toInt();
  uint8_t bytes[3] = { (uint8_t)(p & 0xFF), (uint8_t)((p >> 8) & 0xFF), (uint8_t)((p >> 16) & 0xFF) };
  LOGD("ble", "authenticating (pin=%u)", (unsigned)p);
  return writeChar(UUID_AUTH, bytes, 3);
}

void doDisconnect() {
  if (g_client && g_client->isConnected()) g_client->disconnect();
}

// Shared connect -> auth -> <caller's write> -> disconnect shape for the three public
// calls below. `cmd` does the one command-specific GATT write and returns its result.
template <typename Cmd>
bool connectAuthAndRun(const String &mac, const String &pin, const char *what, Cmd cmd) {
  if (mac.isEmpty() || pin.isEmpty()) {
    LOGW("ble", "%s: missing mac or pin", what);
    return false;
  }
  if (!doConnect(mac)) return false;
  if (!doAuth(pin)) {
    LOGW("ble", "%s: auth write failed", what);
    doDisconnect();
    return false;
  }
  bool ok = cmd();
  doDisconnect();
  LOGI("ble", "%s %s -> %s", what, mac.c_str(), ok ? "OK" : "FAILED");
  return ok;
}
}  // namespace

namespace SomfyBle {

void begin() {
  NimBLEDevice::init("");
  g_client = NimBLEDevice::createClient();
  LOGI("ble", "NimBLE ready (base UUID 00000000-cad9-46c6-a2ea-2ca16d57b4a5)");
}

void loop() {
  // Nothing to pump — connect-on-demand calls below are blocking.
}

bool connectAndGoto(const String &mac, const String &pin, int position) {
  position = constrain(position, 0, 32767);
  return connectAuthAndRun(mac, pin, "goto", [position]() {
    uint8_t data[2] = { (uint8_t)(position & 0xFF), (uint8_t)((position >> 8) & 0xFF) };
    return writeChar(UUID_GOTO_POS, data, 2);
  });
}

bool connectAndStop(const String &mac, const String &pin) {
  return connectAuthAndRun(mac, pin, "stop", []() {
    uint8_t v = 0x01;
    return writeChar(UUID_STOP, &v, 1);
  });
}

bool connectAndIdentify(const String &mac, const String &pin) {
  return connectAuthAndRun(mac, pin, "identify", []() {
    uint8_t v = 0x01;
    return writeChar(UUID_IDENTIFY, &v, 1);
  });
}

void startScan(uint32_t durationMs) {
  (void)durationMs;
  // TODO (Phase 3): NimBLEScan, collect {mac,name,rssi} for the Motors pairing UI.
  LOGW("ble", "startScan: not yet implemented (Phase 3)");
}

bool scanning() { return false; }

String scanResultsJson() { return "[]"; }

}  // namespace SomfyBle
