// Diagnostics — device health, structured serial logging, and info/reboot helpers.
//
// Every log line also lands in an in-RAM ring buffer and (if a sink is registered) is
// pushed live to the web UI over a WebSocket. The macros below are the single choke
// point — boot sequence, WiFi events, MQTT rx/tx, and BLE activity all flow here.
#pragma once
#include <Arduino.h>

namespace Diagnostics {
void   begin();                                          // log boot reason + boot count
void   logf(char level, const char *tag, const char *fmt, ...);  // structured serial log line
String humanUptime();                                    // "0d 01h 02m 03s"
String resetReason();                                    // why the chip last restarted
String infoJson();                                       // device health as JSON (served at /info)
void   reboot();                                         // immediate restart (blocks ~300 ms first)
// Restart after delayMs, fired from a high-priority esp_timer — NOT the Arduino loop.
// Safe to call from an async web handler: the HTTP response flushes, then the chip
// restarts even if the main loop() is stalled (which the flag-in-loop pattern couldn't).
void   scheduleReboot(uint32_t delayMs = 600);

// ---- Live log feed (Logs page) ----
// Sink receives one JSON object per line: {"t":<ms>,"lvl":"I","tag":"..","msg":".."}.
// WebUI registers a sink that broadcasts to the /ws/logs WebSocket clients.
typedef void (*LogSink)(const String &jsonLine);
void   setLogSink(LogSink sink);
String logHistoryJson();                                 // buffered lines as a JSON array
}

// Structured log macros ->  [   12.345] I/tag: message   (level: E>W>I>D>V)
#define LOGE(tag, ...) Diagnostics::logf('E', tag, __VA_ARGS__)
#define LOGW(tag, ...) Diagnostics::logf('W', tag, __VA_ARGS__)
#define LOGI(tag, ...) Diagnostics::logf('I', tag, __VA_ARGS__)
#define LOGD(tag, ...) Diagnostics::logf('D', tag, __VA_ARGS__)
#define LOGV(tag, ...) Diagnostics::logf('V', tag, __VA_ARGS__)
