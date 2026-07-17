// WebUI — the device web interface (tabbed status page) + HTTP routes + mDNS.
//
// Named WebUI (not WebServer) to avoid clashing with the Arduino core's
// WebServer.h that WiFiManager pulls in. Owns the AsyncWebServer instance.
#pragma once

namespace WebUI {
void begin();  // start mDNS, register routes, mount OTA, start the server
void loop();   // pump OTA + service deferred reboot / forget-wifi actions
}
