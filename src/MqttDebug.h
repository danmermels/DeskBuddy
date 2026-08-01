#ifndef MQTT_DEBUG_H
#define MQTT_DEBUG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "Constants.h"
#include "Behaviour.h"
#include "State.h"
#include "Logger.h"
#include "Learning.h"

extern PubSubClient mqttClient;
extern Preferences preferences;
extern NTPClient timeClient;
extern struct tm ts;
extern char buf[];
extern const char* getPresenceStateName(int state);
extern void triggerBehaviour(int event, String detail, int forceMode);

static String presenceStateName(int s) {
  const char* n = getPresenceStateName(s);
  return String(n);
}

static void publishDebug(const String& json) {
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_DEBUG_RESP_TOPIC, json.c_str());
  }
}

static String fmtMs(unsigned long ms) {
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hr  = min / 60;
  min %= 60;
  if (hr > 0) return String(hr) + "h" + String(min) + "m";
  return String(min) + "m";
}

static bool isDigitStr(const String& s) {
  if (s.length() == 0) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i]) && s[i] != '.' && s[i] != '-') return false;
  }
  return true;
}

// ---- GET handlers ----

static void handleGetState() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["state"] = presenceStateName(appState.currentPresenceState);
  doc["rawDist"] = appState.rawDetectionDist;
  doc["filtDist"] = round(appState.filteredDetectionDist * 10.0f) / 10.0f;
  doc["present"] = appState.sensorPresenceDetected;
  doc["moving"] = appState.sensorMovingTargetDetected;
  doc["stable"] = appState.sensorPresenceDetected;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetRadar() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["rawDist"] = appState.rawDetectionDist;
  doc["filtDist"] = round(appState.filteredDetectionDist * 10.0f) / 10.0f;
  doc["present"] = appState.sensorPresenceDetected;
  doc["moving"] = appState.sensorMovingTargetDetected;
  doc["static"] = appState.sensorStaticPresenceDetected;
  doc["sim"] = appState.simulationMode;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetFilters() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["filtDist"] = round(appState.filteredDetectionDist * 10.0f) / 10.0f;
  doc["filterWindow"] = appConfig.filterWindow;
  doc["distAvg"] = round(appState.sessionDistanceAverage * 10.0f) / 10.0f;
  doc["distCount"] = appState.sessionDistanceCount;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetStats() {
  DynamicJsonDocument doc(1024);
  doc["ok"] = true;
  doc["deskTime"] = fmtMs(appStats.totalDeskTime);
  doc["focusTime"] = fmtMs(appStats.totalFocusTime);
  doc["breakTime"] = fmtMs(appStats.totalBreakTime);
  doc["breakCount"] = appStats.breakCount;
  doc["score"] = appStats.productivityScore;
  doc["motionTime"] = fmtMs(appStats.totalMotionTime);
  doc["motionCount"] = appStats.motionCount;
  doc["longestStreak"] = fmtMs(appStats.longestSittingStreak);
  doc["latestBreak"] = fmtMs(appStats.latestBreakDuration);
  doc["firstSit"] = appStats.firstSitToday;
  doc["dailyAiCount"] = appStats.dailyAiRequestCount;
  doc["fsWrites"] = appStats.fsWriteCount;
  doc["fsReads"] = appStats.fsReadCount;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetConfig() {
  DynamicJsonDocument doc(1024);
  doc["ok"] = true;
  doc["aiMode"] = appConfig.aiMode;
  doc["aiPersona"] = appConfig.aiPersona;
  doc["clockFace"] = appConfig.clockFace;
  doc["buddyFontIdx"] = appConfig.buddyFontIndex;
  doc["userName"] = appConfig.userName;
  doc["targetHours"] = appConfig.targetHours;
  doc["focusDistLim"] = appConfig.focusDistanceLimit;
  doc["motionRatioLim"] = appConfig.motionRatioLimit;
  doc["distLimit"] = appConfig.deskDistanceLimit;
  doc["filterWindow"] = appConfig.filterWindow;
  doc["hasMail"] = appConfig.hasMail;
  doc["time24h"] = appConfig.time24h;
  doc["g0mSens"] = appConfig.g0mSens;
  doc["g0sSens"] = appConfig.g0sSens;
  doc["g1mSens"] = appConfig.g1mSens;
  doc["g1sSens"] = appConfig.g1sSens;
  doc["g2mSens"] = appConfig.g2mSens;
  doc["g2sSens"] = appConfig.g2sSens;
  doc["g3mSens"] = appConfig.g3mSens;
  doc["g3sSens"] = appConfig.g3sSens;
  doc["g4mSens"] = appConfig.g4mSens;
  doc["g4sSens"] = appConfig.g4sSens;
  doc["g5mSens"] = appConfig.g5mSens;
  doc["g5sSens"] = appConfig.g5sSens;
  doc["g6mSens"] = appConfig.g6mSens;
  doc["g6sSens"] = appConfig.g6sSens;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetSession() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["deskTime"] = fmtMs(appState.sessionDeskTime);
  doc["motionTime"] = fmtMs(appState.sessionMotionTime);
  doc["distAvg"] = round(appState.sessionDistanceAverage * 10.0f) / 10.0f;
  doc["distCount"] = appState.sessionDistanceCount;
  unsigned long contMs = millis() - appState.continuousPresenceStart;
  doc["continuousPresence"] = fmtMs(contMs);
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetTime() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["epoch"] = timeClient.getEpochTime();
  doc["hour"] = ts.tm_hour;
  doc["minute"] = ts.tm_min;
  doc["day"] = timeClient.getDay();
  doc["dayName"] = String(buf);
  doc["ntpSet"] = timeClient.isTimeSet();
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

static void handleGetSystem() {
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["minHeap"] = ESP.getMinFreeHeap();
  doc["uptime"] = fmtMs(millis());
  doc["wifiRssi"] = WiFi.RSSI();
  doc["wifiStatus"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
  doc["simActive"] = appState.simulationMode;
  doc["simContinuous"] = appState.simulationContinuous;
  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

// Generic GET: lookup any variable by name
static void handleGetGeneric(const String& key) {
  DynamicJsonDocument doc(256);

  // Presence state
  if (key == "state" || key == "presence") {
    doc["ok"] = true;
    doc[key] = presenceStateName(appState.currentPresenceState);
  }
  else if (key == "rawDist") {
    doc["ok"] = true;
    doc[key] = appState.rawDetectionDist;
  }
  else if (key == "filtDist") {
    doc["ok"] = true;
    doc[key] = round(appState.filteredDetectionDist * 10.0f) / 10.0f;
  }
  else if (key == "present") {
    doc["ok"] = true;
    doc[key] = appState.sensorPresenceDetected;
  }
  else if (key == "moving") {
    doc["ok"] = true;
    doc[key] = appState.sensorMovingTargetDetected;
  }
  else if (key == "score" || key == "productivityScore") {
    doc["ok"] = true;
    doc[key] = appStats.productivityScore;
  }
  else if (key == "deskTime") {
    doc["ok"] = true;
    doc[key] = fmtMs(appStats.totalDeskTime);
  }
  else if (key == "focusTime") {
    doc["ok"] = true;
    doc[key] = fmtMs(appStats.totalFocusTime);
  }
  else if (key == "breakTime") {
    doc["ok"] = true;
    doc[key] = fmtMs(appStats.totalBreakTime);
  }
  else if (key == "breakCount") {
    doc["ok"] = true;
    doc[key] = appStats.breakCount;
  }
  else if (key == "motionTime") {
    doc["ok"] = true;
    doc[key] = fmtMs(appStats.totalMotionTime);
  }
  else if (key == "motionCount") {
    doc["ok"] = true;
    doc[key] = appStats.motionCount;
  }
  else if (key == "longestStreak") {
    doc["ok"] = true;
    doc[key] = fmtMs(appStats.longestSittingStreak);
  }
  else if (key == "userName") {
    doc["ok"] = true;
    doc[key] = appConfig.userName;
  }
  else if (key == "aiMode") {
    doc["ok"] = true;
    doc[key] = appConfig.aiMode;
  }
  else if (key == "aiPersona") {
    doc["ok"] = true;
    doc[key] = appConfig.aiPersona;
  }
  else if (key == "clockFace") {
    doc["ok"] = true;
    doc[key] = appConfig.clockFace;
  }
  else if (key == "buddyFontIdx") {
    doc["ok"] = true;
    doc[key] = appConfig.buddyFontIndex;
  }
  else if (key == "distLimit") {
    doc["ok"] = true;
    doc[key] = appConfig.deskDistanceLimit;
  }
  else if (key == "focusDistLim") {
    doc["ok"] = true;
    doc[key] = appConfig.focusDistanceLimit;
  }
  else if (key == "motionRatioLim") {
    doc["ok"] = true;
    doc[key] = appConfig.motionRatioLimit;
  }
  else if (key == "filterWindow") {
    doc["ok"] = true;
    doc[key] = appConfig.filterWindow;
  }
  else if (key == "freeHeap") {
    doc["ok"] = true;
    doc[key] = (int)ESP.getFreeHeap();
  }
  else {
    doc["ok"] = false;
    doc["error"] = "Unknown key '" + key + "'";
  }

  String out;
  serializeJson(doc, out);
  publishDebug(out);
}

// ---- SET handler ----

static void handleSet(const String& args) {
  int sp = args.indexOf(' ');
  if (sp < 0) {
    publishDebug("{\"ok\":false,\"error\":\"Usage: SET config.key value\"}");
    return;
  }

  String key = args.substring(0, sp);
  String valStr = args.substring(sp + 1);

  if (!key.startsWith("config.") && !key.startsWith("stats.")) {
    publishDebug("{\"ok\":false,\"error\":\"SET only supports config.* and stats.* keys\"}");
    return;
  }

  String cfgKey = key.substring(key.indexOf('.') + 1);
  bool ok = false;
  bool isConfig = key.startsWith("config.");

  if (isConfig) preferences.begin("deskbuddy", false);

  if (cfgKey == "aiMode" && isDigitStr(valStr)) {
    appConfig.aiMode = valStr.toInt();
    preferences.putInt("aiMode", appConfig.aiMode);
    ok = true;
  } else if (cfgKey == "aiPersona" && isDigitStr(valStr)) {
    appConfig.aiPersona = valStr.toInt();
    preferences.putInt("aiPersona", appConfig.aiPersona);
    ok = true;
  } else if (cfgKey == "clockFace" && isDigitStr(valStr)) {
    appConfig.clockFace = valStr.toInt();
    preferences.putInt("clockFace", appConfig.clockFace);
    ok = true;
  } else if (cfgKey == "buddyFontIdx" && isDigitStr(valStr)) {
    appConfig.buddyFontIndex = valStr.toInt();
    preferences.putInt("buddyFontIdx", appConfig.buddyFontIndex);
    ok = true;
  } else if (cfgKey == "userName") {
    if (valStr.startsWith("\"") && valStr.endsWith("\"")) {
      valStr = valStr.substring(1, valStr.length() - 1);
    }
    appConfig.userName = valStr;
    preferences.putString("userName", appConfig.userName);
    ok = true;
  } else if (cfgKey == "targetHours" && isDigitStr(valStr)) {
    appConfig.targetHours = valStr.toFloat();
    preferences.putFloat("targetHours", appConfig.targetHours);
    ok = true;
  } else if (cfgKey == "focusDistLim" && isDigitStr(valStr)) {
    appConfig.focusDistanceLimit = valStr.toInt();
    preferences.putInt("focusDistLim", appConfig.focusDistanceLimit);
    ok = true;
  } else if (cfgKey == "motionRatioLim" && isDigitStr(valStr)) {
    appConfig.motionRatioLimit = valStr.toInt();
    preferences.putInt("motionRatioLim", appConfig.motionRatioLimit);
    ok = true;
  } else if (cfgKey == "distLimit" && isDigitStr(valStr)) {
    appConfig.deskDistanceLimit = valStr.toInt();
    preferences.putInt("distLimit", appConfig.deskDistanceLimit);
    ok = true;
  } else if (cfgKey == "filterWindow" && isDigitStr(valStr)) {
    appConfig.filterWindow = valStr.toFloat();
    preferences.putFloat("filterWindow", appConfig.filterWindow);
    ok = true;
  } else if (cfgKey == "hasMail") {
    appConfig.hasMail = (valStr == "1" || valStr == "true");
    preferences.putBool("hasMail", appConfig.hasMail);
    ok = true;
  } else if (cfgKey == "time24h") {
    appConfig.time24h = (valStr == "1" || valStr == "true");
    preferences.putBool("time24h", appConfig.time24h);
    ok = true;
  } else if (cfgKey == "g0mSens" && isDigitStr(valStr)) {
    appConfig.g0mSens = valStr.toInt();
    preferences.putInt("g0mSens", appConfig.g0mSens);
    ok = true;
  } else if (cfgKey == "g0sSens" && isDigitStr(valStr)) {
    appConfig.g0sSens = valStr.toInt();
    preferences.putInt("g0sSens", appConfig.g0sSens);
    ok = true;
  } else if (cfgKey == "g1mSens" && isDigitStr(valStr)) {
    appConfig.g1mSens = valStr.toInt();
    preferences.putInt("g1mSens", appConfig.g1mSens);
    ok = true;
  } else if (cfgKey == "g1sSens" && isDigitStr(valStr)) {
    appConfig.g1sSens = valStr.toInt();
    preferences.putInt("g1sSens", appConfig.g1sSens);
    ok = true;
  } else if (cfgKey == "g2mSens" && isDigitStr(valStr)) {
    appConfig.g2mSens = valStr.toInt();
    preferences.putInt("g2mSens", appConfig.g2mSens);
    ok = true;
  } else if (cfgKey == "g2sSens" && isDigitStr(valStr)) {
    appConfig.g2sSens = valStr.toInt();
    preferences.putInt("g2sSens", appConfig.g2sSens);
    ok = true;
  } else if (cfgKey == "g3mSens" && isDigitStr(valStr)) {
    appConfig.g3mSens = valStr.toInt();
    preferences.putInt("g3mSens", appConfig.g3mSens);
    ok = true;
  } else if (cfgKey == "g3sSens" && isDigitStr(valStr)) {
    appConfig.g3sSens = valStr.toInt();
    preferences.putInt("g3sSens", appConfig.g3sSens);
    ok = true;
  } else if (cfgKey == "g4mSens" && isDigitStr(valStr)) {
    appConfig.g4mSens = valStr.toInt();
    preferences.putInt("g4mSens", appConfig.g4mSens);
    ok = true;
  } else if (cfgKey == "g4sSens" && isDigitStr(valStr)) {
    appConfig.g4sSens = valStr.toInt();
    preferences.putInt("g4sSens", appConfig.g4sSens);
    ok = true;
  } else if (cfgKey == "g5mSens" && isDigitStr(valStr)) {
    appConfig.g5mSens = valStr.toInt();
    preferences.putInt("g5mSens", appConfig.g5mSens);
    ok = true;
  } else if (cfgKey == "g5sSens" && isDigitStr(valStr)) {
    appConfig.g5sSens = valStr.toInt();
    preferences.putInt("g5sSens", appConfig.g5sSens);
    ok = true;
  } else if (cfgKey == "g6mSens" && isDigitStr(valStr)) {
    appConfig.g6mSens = valStr.toInt();
    preferences.putInt("g6mSens", appConfig.g6mSens);
    ok = true;
  } else if (cfgKey == "g6sSens" && isDigitStr(valStr)) {
    appConfig.g6sSens = valStr.toInt();
    preferences.putInt("g6sSens", appConfig.g6sSens);
    ok = true;
  }

  // Stats overrides
  else if (cfgKey == "breakCount" && isDigitStr(valStr)) {
    appStats.breakCount = valStr.toInt();
    ok = true;
  } else if (cfgKey == "latestBreakDuration" && isDigitStr(valStr)) {
    appStats.latestBreakDuration = valStr.toInt();
    ok = true;
  } else if (cfgKey == "previousLatestBreakDuration" && isDigitStr(valStr)) {
    appStats.previousLatestBreakDuration = valStr.toInt();
    ok = true;
  } else if (cfgKey == "totalBreakTime" && isDigitStr(valStr)) {
    appStats.totalBreakTime = valStr.toInt();
    ok = true;
  } else if (cfgKey == "totalDeskTime" && isDigitStr(valStr)) {
    appStats.totalDeskTime = valStr.toInt();
    ok = true;
  } else if (cfgKey == "totalFocusTime" && isDigitStr(valStr)) {
    appStats.totalFocusTime = valStr.toInt();
    ok = true;
  } else if (cfgKey == "firstSitToday") {
    appStats.firstSitToday = (valStr == "1" || valStr == "true");
    ok = true;
  } else if (cfgKey == "overnightBreakDuration" && isDigitStr(valStr)) {
    appStats.overnightBreakDuration = valStr.toInt();
    ok = true;
  }

  if (isConfig) preferences.end();

  if (ok) {
    String resp = "{\"ok\":true,\"key\":\"" + cfgKey + "\",\"value\":" + valStr + "}";
    publishDebug(resp);
  } else {
    String resp = "{\"ok\":false,\"error\":\"Unknown or invalid key '" + cfgKey + "'\"}";
    publishDebug(resp);
  }
}

// ---- SIM handler ----

static void handleSim(const String& args) {
  int sp = args.indexOf(' ');
  String cmd = (sp < 0) ? args : args.substring(0, sp);
  String params = (sp < 0) ? "" : args.substring(sp + 1);

  if (cmd == "radar") {
    int p1 = params.indexOf(' ');
    int p2 = (p1 > 0) ? params.indexOf(' ', p1 + 1) : -1;
    if (p1 < 0 || p2 < 0) {
      publishDebug("{\"ok\":false,\"error\":\"Usage: SIM radar <dist> <moving> <present>\"}");
      return;
    }
    int dist    = params.substring(0, p1).toInt();
    int moving  = params.substring(p1 + 1, p2).toInt();
    int present = params.substring(p2 + 1).toInt();

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = dist;
    appState.simulatedMoving = (moving != 0);
    appState.simulatedPresent = (present != 0);
    appState.simulatedStateOverride = -1;

    String resp = "{\"ok\":true,\"sim\":\"on\",\"dist\":" + String(dist) +
                  ",\"moving\":" + String(moving) + ",\"present\":" + String(present) + "}";
    publishDebug(resp);

  } else if (cmd == "away") {
    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = 0;
    appState.simulatedMoving = false;
    appState.simulatedPresent = false;
    appState.simulatedStateOverride = STATE_AWAY;

    publishDebug("{\"ok\":true,\"sim\":\"on\",\"preset\":\"away\"}");

  } else if (cmd == "sit") {
    int dist = params.length() > 0 ? params.toInt() : 80;
    if (dist <= 0) dist = 80;

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = dist;
    appState.simulatedMoving = false;
    appState.simulatedPresent = true;
    appState.simulatedStateOverride = -1;

    String resp = "{\"ok\":true,\"sim\":\"on\",\"preset\":\"sit\",\"dist\":" + String(dist) + "}";
    publishDebug(resp);

  } else if (cmd == "focus") {
    int dist = params.length() > 0 ? params.toInt() : 40;
    if (dist <= 0) dist = 40;

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = dist;
    appState.simulatedMoving = false;
    appState.simulatedPresent = true;
    appState.simulatedStateOverride = STATE_FOCUS;

    String resp = "{\"ok\":true,\"sim\":\"on\",\"preset\":\"focus\",\"dist\":" + String(dist) + "}";
    publishDebug(resp);

  } else if (cmd == "busy") {
    int dist = params.length() > 0 ? params.toInt() : 40;
    if (dist <= 0) dist = 40;

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = dist;
    appState.simulatedMoving = true;
    appState.simulatedPresent = true;
    appState.simulatedStateOverride = STATE_BUSY;

    String resp = "{\"ok\":true,\"sim\":\"on\",\"preset\":\"busy\",\"dist\":" + String(dist) + "}";
    publishDebug(resp);

  } else if (cmd == "distracted") {
    int dist = params.length() > 0 ? params.toInt() : 120;
    if (dist <= 0) dist = 120;

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedDistance = dist;
    appState.simulatedMoving = true;
    appState.simulatedPresent = true;
    appState.simulatedStateOverride = STATE_DISTRACTED;

    String resp = "{\"ok\":true,\"sim\":\"on\",\"preset\":\"distracted\",\"dist\":" + String(dist) + "}";
    publishDebug(resp);

  } else if (cmd == "state") {
    if (params.length() == 0) {
      publishDebug("{\"ok\":false,\"error\":\"Usage: SIM state <AWAY|FOCUS|BUSY|DISTRACTED|REGULAR>\"}");
      return;
    }
    params.toUpperCase();
    int st = -1;
    if (params == "AWAY")        st = STATE_AWAY;
    else if (params == "FOCUS")  st = STATE_FOCUS;
    else if (params == "BUSY")   st = STATE_BUSY;
    else if (params == "DISTRACTED") st = STATE_DISTRACTED;
    else if (params == "REGULAR") st = STATE_REGULAR;

    if (st < 0) {
      publishDebug("{\"ok\":false,\"error\":\"Unknown state '" + params + "'\"}");
      return;
    }

    appState.simulationMode = true;
    appState.simulationContinuous = true;
    appState.simulatedStateOverride = st;

    if (st == STATE_AWAY) {
      appState.simulatedDistance = 0;
      appState.simulatedMoving = false;
      appState.simulatedPresent = false;
    } else {
      appState.simulatedPresent = true;
      appState.simulatedDistance = (st == STATE_FOCUS) ? 40 : 80;
      appState.simulatedMoving = (st == STATE_BUSY || st == STATE_DISTRACTED);
    }

    String resp = "{\"ok\":true,\"sim\":\"on\",\"overrideState\":\"" + String(getPresenceStateName(st)) + "\"}";
    publishDebug(resp);

  } else if (cmd == "time") {
    if (params.length() == 0) {
      publishDebug("{\"ok\":false,\"error\":\"Usage: SIM time <epoch>\"}");
      return;
    }
    appState.simulatedEpoch = params.toInt();
    String resp = "{\"ok\":true,\"simTime\":" + params + "}";
    publishDebug(resp);

  } else if (cmd == "loop") {
    appState.simulationContinuous = true;
    publishDebug("{\"ok\":true,\"sim\":\"continuous\"}");

  } else if (cmd == "stop") {
    appState.simulationMode = false;
    appState.simulationContinuous = false;
    appState.simulatedDistance = 0;
    appState.simulatedMoving = false;
    appState.simulatedPresent = false;
    appState.simulatedStateOverride = -1;
    appState.simulatedEpoch = 0;
    publishDebug("{\"ok\":true,\"sim\":\"off\"}");

  } else {
    publishDebug("{\"ok\":false,\"error\":\"Unknown SIM command '" + cmd + "'\"}");
  }
}

// ---- SYS handler ----

static void handleSys(const String& args) {
  if (args == "reboot") {
    publishDebug("{\"ok\":true,\"msg\":\"Rebooting...\"}");
    delay(200);
    ESP.restart();
  } else if (args == "reset_stats") {
    preferences.begin("deskbuddy", false);
    preferences.clear();
    preferences.end();
    if (LittleFS.exists("/stats.json")) LittleFS.remove("/stats.json");
    publishDebug("{\"ok\":true,\"msg\":\"Stats cleared, rebooting...\"}");
    delay(200);
    ESP.restart();
  } else if (args == "factory_reset") {
    preferences.begin("deskbuddy", false);
    preferences.clear();
    preferences.end();
    if (LittleFS.exists("/stats.json")) LittleFS.remove("/stats.json");
    publishDebug("{\"ok\":true,\"msg\":\"Factory reset, rebooting...\"}");
    delay(200);
    ESP.restart();
  } else {
    publishDebug("{\"ok\":false,\"error\":\"Unknown SYS command '" + args + "'\"}");
  }
}

// ---- TRIGGER handler ----

static int parseEventType(const String& s) {
  if (isDigitStr(s)) return s.toInt();
  String u = s;
  u.toUpperCase();
  if (u == "FIRST_SIT" || u == "FIRSTSIT")                return EVENT_FIRST_SIT;
  if (u == "WELCOME_BACK" || u == "WELCOMEBACK")          return EVENT_WELCOME_BACK;
  if (u == "STRETCH")                                     return EVENT_STRETCH;
  if (u == "FOCUS_END" || u == "FOCUSEND")                return EVENT_FOCUS_END;
  if (u == "SLACKER")                                     return EVENT_SLACKER;
  if (u == "STREAK_BEATEN" || u == "STREAKBEATEN")        return EVENT_STREAK_BEATEN;
  if (u == "LUNCH" || u == "LUNCH_REMINDER" || u == "LUNCHREMINDER") return EVENT_LUNCH_REMINDER;
  if (u == "EXCESSIVE_BREAKS" || u == "EXCESSIVEBREAKS")  return EVENT_EXCESSIVE_BREAKS;
  if (u == "GOAL_COMPLETED" || u == "GOALCOMPLETED")      return EVENT_GOAL_COMPLETED;
  if (u == "JOURNAL")                                     return EVENT_JOURNAL;
  if (u == "NAGGING")                                     return EVENT_NAGGING;
  if (u == "TASK_DUE" || u == "TASKDUE")                  return EVENT_TASK_DUE;
  if (u == "PAGE")                                        return EVENT_PAGE;
  if (u == "LATEHOURS" || u == "LATEHOURS_SIT" || u == "LATEHOURSSIT") return EVENT_LATEHOURS_SIT;
  return -1;
}

static void handleTrigger(const String& args) {
  int sp = args.indexOf(' ');
  String typeStr = (sp < 0) ? args : args.substring(0, sp);
  String rest = (sp < 0) ? "" : args.substring(sp + 1);

  int eventType = parseEventType(typeStr);
  if (typeStr.length() == 0 || eventType < 0 || eventType > EVENT_LATEHOURS_SIT) {
    Logger::log("MQTT", "Invalid TRIGGER event type '%s'. Use 0-%d or a name (FIRST_SIT, WELCOME_BACK, STRETCH, FOCUS_END, SLACKER, STREAK_BEATEN, LUNCH, EXCESSIVE_BREAKS, GOAL_COMPLETED, JOURNAL, NAGGING, TASK_DUE, PAGE, LATEHOURS).", typeStr.c_str(), EVENT_LATEHOURS_SIT);
    publishDebug("{\"ok\":false,\"error\":\"Invalid event type. Use 0-" + String(EVENT_LATEHOURS_SIT) + " or an event name (e.g. LUNCH, JOURNAL, LATEHOURS)\"}");
    return;
  }

  int forceMode = 0;
  String detail = rest;

  if (rest.length() > 0) {
    int sp2 = rest.indexOf(' ');
    String modeStr = (sp2 < 0) ? rest : rest.substring(0, sp2);
    String modeUp = modeStr;
    modeUp.toUpperCase();
    if (modeUp == "AI") {
      forceMode = 1;
      detail = (sp2 < 0) ? "" : rest.substring(sp2 + 1);
    } else if (modeUp == "FALLBACK") {
      forceMode = 2;
      detail = (sp2 < 0) ? "" : rest.substring(sp2 + 1);
    } else if (isDigitStr(modeStr)) {
      forceMode = modeStr.toInt();
      detail = (sp2 < 0) ? "" : rest.substring(sp2 + 1);
    }
  }

  Logger::log("MQTT", "TRIGGER event=%d mode=%d detail=\"%s\"", eventType, forceMode, detail.c_str());
  appState.manualTriggerOverride = true;
  triggerBehaviour(eventType, detail, forceMode);

  String resp = "{\"ok\":true,\"triggered\":\"" + String(eventType) +
                "\",\"mode\":" + String(forceMode) + "}";
  publishDebug(resp);
}

// ---- Main debug command dispatcher ----
// Parses and dispatches plain-text MQTT commands received from the web terminal or debug broker.
// Protocol Format: [COMMAND] [arguments...] (e.g. "GET state", "SET config.userName \"alex\"", "SIM away")
// Supported Commands:
// - GET <target>: Queries internal system values. Options include:
//                 'state'   (presence evaluation),
//                 'radar'   (raw radar detections),
//                 'filters' (median filter status),
//                 'stats'   (daily performance accumulated statistics),
//                 'config'  (all user configurable variables),
//                 'session' (current sitting session stats),
//                 'time'    (epoch and NTP time client settings),
//                 'system'  (free heap memory, RSSI signal, uptime).
//                 Also accepts generic variable names.
// - SET <key> <val>: Updates config values in Preferences or overrides metrics in memory.
// - SIM <scenario>: Controls the radar simulation mode (e.g. 'away', 'sit', 'focus', 'stop').
// - SYS <action>: Executes administrative tasks ('reboot', 'reset_stats', 'factory_reset').
// - TRIGGER <eventType> [ai|fallback] [detail]: Simulates a behaviour event through
//                 triggerBehaviour (e.g. 'TRIGGER 2 ai').
void handleDebugCommand(const String& payload) {
  String trimmed = payload;
  trimmed.trim();
  Logger::log("MQTT", "CMD: \"%s\"", trimmed.c_str());

  int sp = trimmed.indexOf(' ');
  String cmd = (sp < 0) ? trimmed : trimmed.substring(0, sp);
  String args = (sp < 0) ? "" : trimmed.substring(sp + 1);
  cmd.toUpperCase();

  if (cmd == "GET") {
    args.trim();
    if (args.length() == 0) {
      publishDebug("{\"ok\":false,\"error\":\"Usage: GET <target>\"}");
    } else if (args == "state")      handleGetState();
    else if (args == "radar")        handleGetRadar();
    else if (args == "filters")      handleGetFilters();
    else if (args == "stats")        handleGetStats();
    else if (args == "config")       handleGetConfig();
    else if (args == "session")      handleGetSession();
    else if (args == "time")         handleGetTime();
    else if (args == "system")       handleGetSystem();
    else                             handleGetGeneric(args);
  }
  else if (cmd == "SET") {
    handleSet(args);
  }
  else if (cmd == "SIM") {
    handleSim(args);
  }
  else if (cmd == "SYS") {
    handleSys(args);
  }
  else if (cmd == "TRIGGER") {
    handleTrigger(args);
  }
  else {
    publishDebug("{\"ok\":false,\"error\":\"Unknown command '" + cmd + "'. Use GET/SET/SIM/SYS/TRIGGER\"}");
  }
}

#endif // MQTT_DEBUG_H
