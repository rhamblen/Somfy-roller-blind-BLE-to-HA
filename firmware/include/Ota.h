// Ota — browser firmware update, wired to an AsyncWebServer.
#pragma once
#include <ESPAsyncWebServer.h>

namespace Ota {
void begin(AsyncWebServer &server);  // mount /api/ota + progress logging
void loop();                         // must be pumped from the main loop
}
