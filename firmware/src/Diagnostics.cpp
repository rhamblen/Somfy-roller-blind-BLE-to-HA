#include "Diagnostics.h"
#include "AppConfig.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <stdarg.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

namespace {

const char *resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int-wdt";
    case ESP_RST_TASK_WDT:  return "task-wdt";
    case ESP_RST_WDT:       return "other-wdt";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_EXT:       return "external";
    default:                return "unknown";
  }
}

// minimal JSON string encoder (quote + escape " and \)
String jstr(const String &s) {
  String o = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  o += "\"";
  return o;
}

// ---- Live log ring buffer (fixed-size, no heap churn) ----
constexpr int LOG_RING = 60;
struct LogLine { uint32_t ms; char lvl; char tag[16]; char msg[168]; bool used; };
LogLine s_ring[LOG_RING];
int     s_head = 0;                        // next slot to write
Diagnostics::LogSink s_sink = nullptr;

// One buffered line -> {"t":<ms>,"lvl":"I","tag":"..","msg":".."}
String lineJson(const LogLine &l) {
  String j = "{\"t\":" + String(l.ms) + ",\"lvl\":\"";
  j += l.lvl; j += "\",\"tag\":" + jstr(l.tag) + ",\"msg\":" + jstr(l.msg) + "}";
  return j;
}

}  // namespace

namespace Diagnostics {

void begin() {
  LOGI("diag", "boot #%u, reset reason: %s", (unsigned)AppConfig::bootCount(), resetReasonStr());
}

void logf(char level, const char *tag, const char *fmt, ...) {
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  unsigned long ms = millis();
  Serial.printf("[%7lu.%03lu] %c/%s: %s\n", ms / 1000UL, ms % 1000UL, level, tag, msg);

  // Record into the ring buffer and push live to the web UI (if a sink is set).
  LogLine &l = s_ring[s_head];
  l.ms = (uint32_t)ms; l.lvl = level; l.used = true;
  strlcpy(l.tag, tag, sizeof(l.tag));
  strlcpy(l.msg, msg, sizeof(l.msg));
  s_head = (s_head + 1) % LOG_RING;
  if (s_sink) s_sink(lineJson(l));
}

void setLogSink(LogSink sink) { s_sink = sink; }

String logHistoryJson() {
  String j = "[";
  bool first = true;
  for (int i = 0; i < LOG_RING; i++) {
    const LogLine &l = s_ring[(s_head + i) % LOG_RING];  // oldest -> newest
    if (!l.used) continue;
    if (!first) j += ",";
    j += lineJson(l);
    first = false;
  }
  j += "]";
  return j;
}

String resetReason() { return resetReasonStr(); }

String humanUptime() {
  unsigned long s = millis() / 1000UL;
  unsigned long d = s / 86400UL; s %= 86400UL;
  unsigned long h = s / 3600UL;  s %= 3600UL;
  unsigned long m = s / 60UL;    s %= 60UL;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lud %02luh %02lum %02lus", d, h, m, s);
  return String(buf);
}

String infoJson() {
  String j = "{";
  j += "\"firmware\":"     + jstr(FW_VERSION) + ",";
  j += "\"device\":"       + jstr(AppConfig::deviceName()) + ",";
  j += "\"uptime\":"       + jstr(humanUptime()) + ",";
  j += "\"uptime_s\":"     + String(millis() / 1000UL) + ",";
  j += "\"boot_count\":"   + String(AppConfig::bootCount()) + ",";
  j += "\"last_flash\":{\"type\":" + jstr(AppConfig::lastFlashType()) +
       ",\"ok\":" + String(AppConfig::lastFlashOk() ? "true" : "false") +
       ",\"epoch\":" + String(AppConfig::lastFlashEpoch()) + "},";
  j += "\"reset_reason\":" + jstr(resetReasonStr()) + ",";
  j += "\"free_heap\":"    + String(ESP.getFreeHeap()) + ",";
  j += "\"min_free_heap\":"+ String(ESP.getMinFreeHeap()) + ",";
  j += "\"wifi\":{";
  j += "\"ssid\":" + jstr(WiFi.SSID()) + ",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ip\":"   + jstr(WiFi.localIP().toString()) + ",";
  j += "\"mac\":"  + jstr(WiFi.macAddress()) + ",";
  j += "\"host\":" + jstr(AppConfig::deviceName() + ".local");
  j += "}}";
  return j;
}

void reboot() {
  LOGW("diag", "reboot requested");
  delay(300);
  ESP.restart();
}

void scheduleReboot(uint32_t delayMs) {
  static esp_timer_handle_t timer = nullptr;
  LOGW("diag", "reboot scheduled in %u ms", (unsigned)delayMs);
  if (!timer) {
    // ESP_TIMER_TASK dispatch runs the callback in the high-priority esp_timer task,
    // so the restart fires even if the Arduino loop() is starved or blocked.
    const esp_timer_create_args_t args = {
      .callback = [](void *) { esp_restart(); },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "reboot",
      .skip_unhandled_events = false,
    };
    esp_timer_create(&args, &timer);
  } else {
    esp_timer_stop(timer);                     // reschedule if called again
  }
  esp_timer_start_once(timer, (uint64_t)delayMs * 1000ULL);
}

}  // namespace Diagnostics
