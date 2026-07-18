// HomeKit — HomeSpan bridge exposing one Window Covering per paired motor.
//
// Native HAP (no hub/bridge hardware needed). Config lives in AppConfig
// (hkEnabled/hkBridgeName/hkSetupCode); the System > HomeKit web tab reads bridge
// state through the getters below and renders the pairing QR. Ported from the
// Shutter Hub's HomeKit module (docs/decisions/0001-framework-reuse.md,
// docs/decisions/0005-apple-homekit-homespan.md) — same coexistence contract:
//   - HAP runs on TCP port 1201 (homeSpan.setPortNum) — the async web server keeps 80.
//   - mDNS is initialised by WebUI::begin() on the MAIN thread, unconditionally (both with
//     HomeKit on and off) — homeSpan.begin() below then calls mdns_init() itself too, but
//     with the responder already running that's a cheap no-op, and HomeSpan just ADDS its
//     _hap._tcp service on top. Deferring mDNS init to HomeSpan instead (the naive approach)
//     hangs — HomeSpan's own mdns_init() call, made from its background poll task, never
//     returns (the Shutter Hub v0.7.0 lesson). Don't "fix" this by skipping WebUI's mDNS init.
//   - HomeSpan 1.9.1's hostname self-check uses sscanf("%m…"), unsupported by newlib-nano →
//     while(1) PROGRAM HALTED before advertising. Fixed by a vendored patch — see
//     firmware/patches/. Don't drop that patch.
//   - WiFi stays owned by WiFiManager: we're already connected before HomeSpan polls, so its
//     checkConnect() sees WL_CONNECTED and never calls WiFi.begin() itself.
//   - QR setup ID is "SOMF" — must match the web UI's X-HM:// payload.
//   - HomeSpan runs on its OWN task (homeSpan.autoPoll in begin()) — HAP/pairing crypto would
//     otherwise monopolise the shared Arduino loop(). Do NOT call homeSpan.poll() from loop().
//
// Position model (deviates from the Shutter Hub): BLE is connect-on-demand, so there is no
// live position feed to stream in loop() the way ServoController provides one. Each Window
// Covering's Current/Target position is the shared Motors::lastPct "assumed state" (see
// docs/decisions/0003-connect-on-demand-ble.md) — a Home app move calls SomfyBle and, only on
// reported success, updates that assumed state (which Mqtt reads too, so both surfaces agree).
// Enabling/disabling or changing paired motors takes effect on the next reboot (the accessory
// tree is built once at begin(), like a normal HomeSpan sketch).
#pragma once

namespace HomeKit {
void begin();          // start the bridge if AppConfig::hkEnabled(); no-op otherwise
void loop();           // services only the deferred pairing reset (poll runs on its own task)
bool running();        // true once the bridge is up (enabled + begun)
bool paired();         // any HomeKit controller currently paired?
int  controllers();    // number of paired controllers
bool resetPairings();  // erase HomeKit pairing data + reboot; false if the bridge isn't running
}
