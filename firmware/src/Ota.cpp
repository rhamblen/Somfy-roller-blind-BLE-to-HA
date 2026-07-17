#include "Ota.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <Update.h>
#include <time.h>

// Custom OTA — mirrors the Shutter Hub's dual-target update page on the Arduino
// Core's Update library: a firmware image and/or a LittleFS filesystem image,
// flashed independently or together, with a recorded last-flash result.
//
// Upload endpoint:  POST /api/ota?target=firmware|filesystem   (multipart, field "file")
// Neither target reboots on its own: a firmware image only runs after a restart, but
// auto-rebooting raced with a following filesystem upload (the reboot could fire mid-
// write, or be lost entirely) and depended on the main loop being alive. So flashing
// is now decoupled from restarting — flash firmware and/or filesystem in any order,
// then hit Reboot (Diagnostics::scheduleReboot, which fires regardless of loop state).

namespace {
bool   g_ok      = false;   // result of the in-flight / last upload
String g_err     = "";
String g_target  = "firmware";
}

namespace Ota {

void begin(AsyncWebServer &server) {
  server.on("/api/ota", HTTP_POST,
    // (1) final response, once the whole upload has been received
    [](AsyncWebServerRequest *req) {
      String resp = String("{\"ok\":") + (g_ok ? "true" : "false") +
                    ",\"target\":\"" + g_target + "\",\"error\":\"" + g_err + "\"}";
      req->send(g_ok ? 200 : 500, "application/json", resp);
      // No auto-reboot — the caller flashes what it needs, then hits Reboot.
    },
    // (2) streamed upload body — write straight into the target partition
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (index == 0) {
        g_target = (req->hasParam("target") && req->getParam("target")->value() == "filesystem")
                     ? "filesystem" : "firmware";
        g_ok = false; g_err = "";
        int cmd = (g_target == "filesystem") ? U_SPIFFS : U_FLASH;
        LOGI("ota", "%s upload begin: %s", g_target.c_str(), filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
          g_err = Update.errorString();
          LOGE("ota", "begin failed: %s", g_err.c_str());
        }
      }
      if (len && !Update.hasError()) {
        if (Update.write(data, len) != len) g_err = Update.errorString();
      }
      if (final) {
        if (Update.end(true)) {
          g_ok = true;
          LOGI("ota", "%s flash OK (%u bytes)", g_target.c_str(), (unsigned)(index + len));
        } else {
          g_err = Update.errorString();
          LOGE("ota", "%s flash FAILED: %s", g_target.c_str(), g_err.c_str());
        }
        time_t now = time(nullptr);
        AppConfig::recordFlash(g_target, g_ok, now > 1000000000 ? (uint32_t)now : 0);
      }
    });
  LOGI("ota", "custom OTA ready — POST /api/ota?target=firmware|filesystem");
}

void loop() {
  // Nothing to service — flashing no longer auto-reboots (see the header note). Kept so
  // the WebUI loop's Ota::loop() call stays valid and there's a home for future OTA housekeeping.
}

}  // namespace Ota
