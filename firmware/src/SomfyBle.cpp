#include "SomfyBle.h"
#include "Diagnostics.h"
#include <NimBLEDevice.h>

// GATT map — Service 00000000 (Operational), from vendor/somfy-sonesse2-ble-calib-tool-esp's
// README (protocol reverse-engineered by mrmelair; see docs/decisions/0004-vendoring-credit.md).
// All characteristics share this base UUID, with the service/characteristic id substituted
// into the leading 8 hex digits.
namespace {
const char *UUID_BASE_FMT = "%08x-cad9-46c6-a2ea-2ca16d57b4a5";

// Service 00000000 — Operational
constexpr uint32_t CHR_AUTH        = 0x0000000b;  // PIN as 3-byte little-endian int
constexpr uint32_t CHR_IDENTIFY    = 0x00000001;  // 0x01 — jog, useful for pairing confirmation
constexpr uint32_t CHR_GOTO_POS    = 0x00000005;  // 16-bit LE, 0=open, 32767=closed
constexpr uint32_t CHR_STOP        = 0x00000006;  // 0x01

String charUuid(uint32_t id) {
  char buf[40];
  snprintf(buf, sizeof(buf), UUID_BASE_FMT, (unsigned)id);
  return String(buf);
}

bool g_scanning = false;
}  // namespace

namespace SomfyBle {

void begin() {
  NimBLEDevice::init("");
  LOGI("ble", "NimBLE ready (base UUID %s)", charUuid(0).c_str());
}

void loop() {
  // Nothing to pump yet — connect-on-demand calls below are blocking. A future phase
  // may move scan-result collection here if it needs to be non-blocking end to end.
}

bool connectAndGoto(const String &mac, const String &pin, int position) {
  (void)mac; (void)pin; (void)position;
  // TODO (Phase 1): connect(mac) -> write CHR_AUTH with the 3-byte LE pin -> write
  // CHR_GOTO_POS with the 16-bit LE position -> disconnect. Port from magik6k's
  // cmdConnect/cmdAuth/cmdGoto (vendor/somfy-sonesse2-ble-calib-tool-esp), verified
  // against real hardware before this returns anything but false.
  LOGW("ble", "connectAndGoto: not yet implemented (Phase 1)");
  return false;
}

bool connectAndStop(const String &mac, const String &pin) {
  (void)mac; (void)pin;
  // TODO (Phase 1): connect -> auth -> write CHR_STOP (0x01) -> disconnect.
  LOGW("ble", "connectAndStop: not yet implemented (Phase 1)");
  return false;
}

bool connectAndIdentify(const String &mac, const String &pin) {
  (void)mac; (void)pin;
  // TODO (Phase 1): connect -> auth -> write CHR_IDENTIFY (0x01) -> disconnect.
  LOGW("ble", "connectAndIdentify: not yet implemented (Phase 1)");
  return false;
}

void startScan(uint32_t durationMs) {
  (void)durationMs;
  // TODO (Phase 3): NimBLEScan, collect {mac,name,rssi} for the Motors pairing UI.
  LOGW("ble", "startScan: not yet implemented (Phase 3)");
}

bool scanning() { return g_scanning; }

String scanResultsJson() { return "[]"; }

}  // namespace SomfyBle
