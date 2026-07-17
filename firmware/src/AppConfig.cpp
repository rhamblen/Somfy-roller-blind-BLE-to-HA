#include "AppConfig.h"
#include <Preferences.h>
#include <WiFi.h>

namespace {
Preferences prefs;
const char *NS = "somfyble";
String   g_deviceName = "somfy-ble";
uint32_t g_bootCount  = 0;
String   g_lfType     = "none";
bool     g_lfOk       = false;
uint32_t g_lfEpoch    = 0;

// MQTT / Home Assistant
bool     g_mqEn       = false;
String   g_mqHost     = "";
uint16_t g_mqPort     = 1883;
String   g_mqClient   = "";       // blank => derive from MAC
String   g_mqUser     = "";
String   g_mqPass     = "";
String   g_mqBase     = "somfy-ble";
bool     g_mqHaDisc   = true;

// Web auth
bool     g_authEn     = false;
String   g_authUser   = "admin";
String   g_authPass   = "";
}

namespace AppConfig {

void begin() {
  prefs.begin(NS, false);                       // read/write namespace
  g_deviceName = prefs.getString("devName", "somfy-ble");
  g_lfType     = prefs.getString("lfType", "none");
  g_lfOk       = prefs.getBool("lfOk", false);
  g_lfEpoch    = prefs.getUInt("lfEpoch", 0);

  g_mqEn       = prefs.getBool("mqEn", false);
  g_mqHost     = prefs.getString("mqHost", "");
  g_mqPort     = prefs.getUShort("mqPort", 1883);
  g_mqClient   = prefs.getString("mqClient", "");
  g_mqUser     = prefs.getString("mqUser", "");
  g_mqPass     = prefs.getString("mqPass", "");
  g_mqBase     = prefs.getString("mqBase", "somfy-ble");
  g_mqHaDisc   = prefs.getBool("mqHaDisc", true);

  g_authEn     = prefs.getBool("authEn", false);
  g_authUser   = prefs.getString("authUser", "admin");
  g_authPass   = prefs.getString("authPass", "");

  g_bootCount  = prefs.getUInt("bootCount", 0) + 1;
  prefs.putUInt("bootCount", g_bootCount);
}

String deviceName() { return g_deviceName; }

void setDeviceName(const String &name) {
  g_deviceName = name;
  prefs.putString("devName", name);
}

uint32_t bootCount() { return g_bootCount; }

String   lastFlashType()  { return g_lfType; }
bool     lastFlashOk()    { return g_lfOk; }
uint32_t lastFlashEpoch() { return g_lfEpoch; }

void recordFlash(const String &type, bool ok, uint32_t epoch) {
  g_lfType = type; g_lfOk = ok; g_lfEpoch = epoch;
  prefs.putString("lfType", type);
  prefs.putBool("lfOk", ok);
  prefs.putUInt("lfEpoch", epoch);
}

// ---- MQTT / Home Assistant ----

bool     mqttEnabled()     { return g_mqEn; }
String   mqttHost()        { return g_mqHost; }
uint16_t mqttPort()        { return g_mqPort; }
String   mqttUser()        { return g_mqUser; }
String   mqttPass()        { return g_mqPass; }
String   mqttBaseTopic()   { return g_mqBase.length() ? g_mqBase : String("somfy-ble"); }
bool     mqttHaDiscovery() { return g_mqHaDisc; }

String mqttClientId() {
  if (g_mqClient.length()) return g_mqClient;
  String mac = WiFi.macAddress();               // AA:BB:CC:DD:EE:FF
  mac.replace(":", "");
  return "somfy-ble-" + mac.substring(6);        // last 3 bytes -> 6 hex chars
}

void setMqtt(bool enabled, const String &host, uint16_t port, const String &clientId,
             const String &user, const String &pass, const String &baseTopic, bool haDiscovery) {
  g_mqEn = enabled; g_mqHost = host; g_mqPort = port ? port : 1883;
  g_mqClient = clientId; g_mqUser = user; g_mqPass = pass;
  g_mqBase = baseTopic.length() ? baseTopic : String("somfy-ble");
  g_mqHaDisc = haDiscovery;
  prefs.putBool("mqEn", g_mqEn);
  prefs.putString("mqHost", g_mqHost);
  prefs.putUShort("mqPort", g_mqPort);
  prefs.putString("mqClient", g_mqClient);
  prefs.putString("mqUser", g_mqUser);
  prefs.putString("mqPass", g_mqPass);
  prefs.putString("mqBase", g_mqBase);
  prefs.putBool("mqHaDisc", g_mqHaDisc);
}

// ---- Web interface authentication ----

bool   authEnabled() { return g_authEn && g_authPass.length(); }  // never lock out without a password
String authUser()    { return g_authUser.length() ? g_authUser : String("admin"); }
String authPass()    { return g_authPass; }

void setAuth(bool enabled, const String &user, const String &pass) {
  g_authEn = enabled; g_authUser = user; g_authPass = pass;
  prefs.putBool("authEn", g_authEn);
  prefs.putString("authUser", g_authUser);
  prefs.putString("authPass", g_authPass);
}

void factoryReset() {
  prefs.clear();                 // wipe the "somfyble" namespace (keeps WiFi creds + Motors elsewhere)
  prefs.putUInt("bootCount", g_bootCount);   // preserve the boot counter across a config reset
}

}  // namespace AppConfig
