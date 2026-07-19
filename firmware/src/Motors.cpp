#include "Motors.h"
#include "Diagnostics.h"
#include <Preferences.h>

// Records live in memory and are re-serialised wholesale to NVS on every mutation
// (<= MAX entries — cheap). The `id` slug is stored explicitly so a rename never
// changes the MQTT/HA identity of a motor. Pattern ported from the Shutter Hub's
// Shutters module (docs/decisions/0001-framework-reuse.md).
namespace {
Preferences prefs;
const char *NS = "motors";       // own namespace — survives an app config reset

struct Motor {
  String id;
  String name;
  String mac;                      // BLE MAC address, "AA:BB:CC:DD:EE:FF"
  String pin;                      // 3-byte PIN from the motor label, as decimal text
  int    closedPos = Motors::UNSET;  // 0-32767, per the goto-position characteristic
  int    openPos   = Motors::UNSET;
  int    lastPct   = Motors::UNSET;  // assumed state, 0-100 (HA convention: 0 = closed)
};

Motor g_list[Motors::MAX];
int   g_count = 0;

int indexOf(const String &id) {
  for (int i = 0; i < g_count; i++) if (g_list[i].id == id) return i;
  return -1;
}

String jesc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// name -> stable slug: lowercase, non-alnum -> '_', collapse/trim underscores,
// then de-duplicate against existing ids ("front_left", "front_left_2", …).
String slugify(const String &name) {
  String s;
  bool lastUnderscore = true;                 // suppress a leading underscore
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (isalnum(c)) { s += (char)tolower(c); lastUnderscore = false; }
    else if (!lastUnderscore) { s += '_'; lastUnderscore = true; }
  }
  while (s.endsWith("_")) s.remove(s.length() - 1);
  if (!s.length()) s = "motor";
  String base = s; int n = 2;
  while (indexOf(s) >= 0) s = base + "_" + String(n++);
  return s;
}

void persist() {
  prefs.putInt("count", g_count);
  for (int i = 0; i < g_count; i++) {
    const Motor &m = g_list[i];
    prefs.putString(("id" + String(i)).c_str(), m.id);
    prefs.putString(("n"  + String(i)).c_str(), m.name);
    prefs.putString(("m"  + String(i)).c_str(), m.mac);
    prefs.putString(("p"  + String(i)).c_str(), m.pin);
    prefs.putInt(("cl" + String(i)).c_str(), m.closedPos);
    prefs.putInt(("op" + String(i)).c_str(), m.openPos);
    prefs.putInt(("lp" + String(i)).c_str(), m.lastPct);
  }
}

bool calibrated(const Motor &m) {
  return m.closedPos != Motors::UNSET && m.openPos != Motors::UNSET;
}
}  // namespace

namespace Motors {

void begin() {
  prefs.begin(NS, false);
  g_count = constrain(prefs.getInt("count", 0), 0, MAX);
  for (int i = 0; i < g_count; i++) {
    Motor &m = g_list[i];
    m.id        = prefs.getString(("id" + String(i)).c_str(), "");
    m.name      = prefs.getString(("n"  + String(i)).c_str(), "");
    m.mac       = prefs.getString(("m"  + String(i)).c_str(), "");
    m.pin       = prefs.getString(("p"  + String(i)).c_str(), "");
    m.closedPos = prefs.getInt(("cl" + String(i)).c_str(), UNSET);
    m.openPos   = prefs.getInt(("op" + String(i)).c_str(), UNSET);
    m.lastPct   = prefs.getInt(("lp" + String(i)).c_str(), UNSET);
  }
  LOGI("motors", "loaded %d motor(s)", g_count);
}

int  count() { return g_count; }
bool exists(const String &id) { return indexOf(id) >= 0; }

int    find(const String &id) { return indexOf(id); }
String idAt(int i)         { return (i >= 0 && i < g_count) ? g_list[i].id   : String(); }
String nameAt(int i)       { return (i >= 0 && i < g_count) ? g_list[i].name : String(); }
String macAt(int i)        { return (i >= 0 && i < g_count) ? g_list[i].mac  : String(); }
bool   calibratedAt(int i) { return (i >= 0 && i < g_count) ? calibrated(g_list[i]) : false; }

String listJson() {
  String j = "[";
  for (int i = 0; i < g_count; i++) {
    const Motor &m = g_list[i];
    if (i) j += ",";
    j += "{\"id\":\""   + jesc(m.id)   + "\"";
    j += ",\"name\":\"" + jesc(m.name) + "\"";
    j += ",\"mac\":\""  + jesc(m.mac)  + "\"";
    j += ",\"closedPos\":" + String(m.closedPos);
    j += ",\"openPos\":"   + String(m.openPos);
    j += ",\"lastPct\":"   + String(m.lastPct);
    j += ",\"calibrated\":" + String(calibrated(m) ? "true" : "false");
    j += "}";   // PIN is never included — write-only from the caller's perspective
  }
  j += "]";
  return j;
}

bool add(const String &name, const String &mac, const String &pin) {
  if (g_count >= MAX) return false;
  Motor &m = g_list[g_count];
  m = Motor();                                 // reset to defaults
  m.name = name.length() ? name : String("Motor ") + String(g_count + 1);
  m.id   = slugify(m.name);
  m.mac  = mac;
  m.pin  = pin;
  g_count++;
  persist();
  LOGI("motors", "added '%s' (id=%s, mac=%s)", m.name.c_str(), m.id.c_str(), m.mac.c_str());
  return true;
}

bool remove(const String &id) {
  int i = indexOf(id);
  if (i < 0) return false;
  for (int k = i; k < g_count - 1; k++) g_list[k] = g_list[k + 1];
  g_count--;
  // Clear the now-orphaned tail slot's keys so a shrink can't resurrect stale data.
  int tail = g_count;
  for (const char *p : {"id", "n", "m", "p", "cl", "op", "lp"})
    prefs.remove((String(p) + String(tail)).c_str());
  persist();
  LOGI("motors", "removed id=%s", id.c_str());
  return true;
}

bool rename(const String &id, const String &name) {
  int i = indexOf(id);
  if (i < 0 || !name.length()) return false;
  g_list[i].name = name;                       // id/slug is stable — not regenerated
  persist();
  return true;
}

String pinFor(const String &id) {
  int i = indexOf(id);
  return i < 0 ? String() : g_list[i].pin;
}

bool setEdge(const String &id, bool openEdge, int pos) {
  int i = indexOf(id);
  if (i < 0) return false;
  if (openEdge) g_list[i].openPos = pos; else g_list[i].closedPos = pos;
  persist();
  LOGI("motors", "%s %s edge = %d", id.c_str(), openEdge ? "open" : "closed", pos);
  return true;
}

int edgePos(const String &id, bool openEdge) {
  int i = indexOf(id);
  if (i < 0) return UNSET;
  return openEdge ? g_list[i].openPos : g_list[i].closedPos;
}

// Resolved open/closed edges for math: calibrated values if set, else the full protocol
// range (0=open, 32767=closed) — mirrors HomeKit.cpp's former local motorInfo() helper.
void resolvedEdges(const String &id, int &openPos, int &closedPos) {
  int op = edgePos(id, true);
  int cl = edgePos(id, false);
  openPos   = (op != UNSET) ? op : 0;
  closedPos = (cl != UNSET) ? cl : 32767;
}

int posToPct(const String &id, int pos) {
  int openPos, closedPos;
  resolvedEdges(id, openPos, closedPos);
  if (openPos == closedPos) return 0;             // degenerate — no usable travel range
  int pct = (int)lroundf(100.0f * (pos - closedPos) / (float)(openPos - closedPos));
  return constrain(pct, 0, 100);
}

int pctToPos(const String &id, int pct) {
  int openPos, closedPos;
  resolvedEdges(id, openPos, closedPos);
  pct = constrain(pct, 0, 100);
  return closedPos + (int)lroundf((openPos - closedPos) * pct / 100.0f);
}

int lastPct(const String &id) {
  int i = indexOf(id);
  return i < 0 ? UNSET : g_list[i].lastPct;
}

void setLastPct(const String &id, int pct) {
  int i = indexOf(id);
  if (i < 0) return;
  g_list[i].lastPct = constrain(pct, 0, 100);
  persist();
}

}  // namespace Motors
