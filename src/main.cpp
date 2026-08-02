#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ld2410.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

// ============================================================
// Named Constants (Imported from Constants.h)
// ============================================================
#include "Constants.h"

// Maximum AI response character count, shared with Display.h and AI.h
extern const int AI_RESPONSE_MAX_CHARS = 90;
extern const int DISPLAY_CHARS_PER_LINE = 19;

// ============================================================
// Subsystem Headers (extern globals are linked from this file)
// ============================================================
#include "Behaviour.h"
#include "Learning.h"
#include "PresenceAnalysis.h"
#include "MessageManager.h"
#include "../Credentials.h"
#include "MqttService.h"
#include "MqttDebug.h"
#include "Display.h"
#include "Radar.h"
#include "Stats.h"
#include "AI.h"
#include "Web.h"
#include "Faceplates.h"

// ============================================================
// State management definitions
// ============================================================
#include "State.h"
#include "Logger.h"

ConfigState appConfig;
StatsState appStats;
RuntimeState appState;
TodoState appTodo;
TftMessageHistory tftMsgHistory;

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite hourHandSprite = TFT_eSprite(&tft);
TFT_eSprite minuteHandSprite = TFT_eSprite(&tft);
TFT_eSprite secondHandSprite = TFT_eSprite(&tft);
TFT_eSprite centerBgSprite = TFT_eSprite(&tft);
ld2410 radar;
WebServer server(80);
DNSServer dnsServer;

// MessageManager instance
MessageManager messageManager;

// Rolling Median Filters
RollingMedianFilter detectionDistFilter(DIST_FILTER_SIZE);
RollingMedianFilter motionFilter(MOTION_FILTER_SIZE);

// Rolling Motion Window (for state machine transitions)
static uint16_t recentMotionBuckets[RECENT_MOTION_WINDOW_S] = {0};
static uint16_t recentDeskBuckets[RECENT_MOTION_WINDOW_S] = {0};
static size_t recentBucketHead = 0;
static unsigned long lastBucketAdvanceTime = 0;

void clearRecentMotionWindow() {
  memset(recentMotionBuckets, 0, sizeof(recentMotionBuckets));
  memset(recentDeskBuckets, 0, sizeof(recentDeskBuckets));
  recentBucketHead = 0;
  lastBucketAdvanceTime = millis();
}

// Productivity & Session Timing Metrics
volatile uint32_t currentSitDownSessionId = 0;
uint32_t aiQuerySessionId = 0;
TaskHandle_t aiQueryTaskHandle = NULL;

// Network & MQTT Service Instances
WiFiClient wifiClient;
PubSubClient mqttClient;

#include <queue>
std::queue<MqttQueueMessage> mqttPublishQueue;
SemaphoreHandle_t mqttPublishQueueMutex = NULL;

// Persistent Settings (NVS Preferences)
Preferences preferences;

// Presence Session States (Daily Workday Tracking)
enum PresenceState {
  PRESENCE_AWAY,     // User has left or has not checked in today
  PRESENCE_SITTING,  // User is actively present at the desk
  PRESENCE_BREAK     // User is away for a short break
};

PresenceState currentSessionState = PRESENCE_AWAY;

// Animated Ring Colors & Parameters
const RGBColor stateColors[] = {
  {80, 80, 80},     // STATE_AWAY: Dark Grey
  {0, 120, 255},    // STATE_FOCUS: Deep Blue
  {0, 220, 80},     // STATE_BUSY: Forest Green
  {255, 50, 50},    // STATE_DISTRACTED: Soft Red
  {200, 200, 200}   // STATE_REGULAR: Soft White
};


// NTP Client & Weather Data
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned long lastHourlyUpdate = 0;
struct tm ts;
char buf[80];

// UI Pages
int uiPage = 0;

// Helper to parse dotted-quad IP string to IPAddress
IPAddress parseIP(const String& s) {
  IPAddress ip;
  ip.fromString(s);
  return ip;
}

// Formatting helper for durations
String formatTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;
  if (hours > 0) {
    return String(hours) + "h" + String(minutes) + "m";
  }
  return String(minutes) + "m";
}

// Formatting helper for epoch timestamp to HH:MM (local time already shifted offset)
String formatEpochTime(uint32_t epoch) {
  if (epoch == 0) return "Not registered yet";
  time_t rawTime = (time_t)epoch;
  struct tm * gmTimeInfo = gmtime(&rawTime);
  char timeStr[10];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", gmTimeInfo->tm_hour, gmTimeInfo->tm_min);
  return String(timeStr);
}

// Converts state ID to string representation
const char* getPresenceStateName(int state) {
  switch (state) {
    case STATE_AWAY:        return "Away";
    case STATE_FOCUS:       return "Focus";
    case STATE_BUSY:        return "Busy";
    case STATE_DISTRACTED:  return "Distracted";
    case STATE_REGULAR:     return "Regular Activity";
    default:                return "Unknown";
  }
}

// Dynamic quote personalization helper
String personalizeQuote(String quote, String name) {
  char formattedQuote[128];
  snprintf(formattedQuote, sizeof(formattedQuote), quote.c_str(), name.c_str());
  return String(formattedQuote);
}

// Save daily statistics to LittleFS using an atomic rename pattern
void saveDailyStats() {
  Serial.println("[STATS] saveDailyStats: Opening stats.json.tmp for writing...");
  fs::File file = LittleFS.open("/stats.json.tmp", "w");
  if (!file) {
    Serial.println("[STATS] ERROR: saveDailyStats failed to open /stats.json.tmp!");
    return;
  }
  DynamicJsonDocument doc(8192);
  doc["firstSitToday"] = appStats.firstSitToday;
  doc["firstSitEpoch"] = appStats.firstSitEpoch;
  doc["breakCount"] = appStats.breakCount;
  doc["totalDeskTime"] = appStats.totalDeskTime;
  doc["totalFocusTime"] = appStats.totalFocusTime;
  doc["totalBreakTime"] = appStats.totalBreakTime;
  doc["overnightBreakDuration"] = appStats.overnightBreakDuration;
  doc["lastAwayEpoch"] = appStats.lastAwayEpoch;
  doc["dailyAiRequestCount"] = appStats.dailyAiRequestCount;
  doc["lastNtpDay"] = appStats.lastNtpDay;
  doc["longestSittingStreak"] = appStats.longestSittingStreak;
  doc["latestBreakDuration"] = appStats.latestBreakDuration;
  doc["totalMotionTime"] = appStats.totalMotionTime;
  doc["motionCount"] = appStats.motionCount;
  doc["isStopByTracking"] = appState.isStopByTracking;
  doc["originalLastAwayEpoch"] = appState.originalLastAwayEpoch;
  doc["totalStopByTimeMs"] = appState.totalStopByTimeMs;
  doc["previousLatestBreakDuration"] = appStats.previousLatestBreakDuration;
  doc["lastMidnightCheckDay"] = appStats.lastMidnightCheckDay;
  doc["userName"] = appConfig.userName;
  doc["deskDistanceLimit"] = appConfig.deskDistanceLimit;
  doc["focusDistanceLimit"] = appConfig.focusDistanceLimit;
  doc["motionRatioLimit"] = appConfig.motionRatioLimit;
  doc["hasMail"] = appConfig.hasMail;
  doc["time24h"] = appConfig.time24h;
  doc["targetHours"] = appConfig.targetHours;
  JsonArray countArray = doc.createNestedArray("historyDaysCountWeekly");
  for (int d = 0; d < 7; d++) {
    countArray.add(appStats.historyDaysCountWeekly[d]);
  }
  doc["lunchReminderTriggered"] = appStats.lunchReminderTriggered;
  doc["excessiveBreaksTriggered"] = appStats.excessiveBreaksTriggered;
  doc["goalCompletedTriggered"] = appStats.goalCompletedTriggered;
  doc["morningJournalTriggered"] = appStats.morningJournalTriggered;
  doc["preLunchJournalTriggered"] = appStats.preLunchJournalTriggered;
  doc["endOfDayJournalTriggered"] = appStats.endOfDayJournalTriggered;
  doc["nagQueueIndex"] = appStats.nagQueueIndex;
  doc["dueFiredDay"] = appStats.dueFiredDay;
  doc["dueFiredKeys"] = appStats.dueFiredKeys;
  doc["fsWriteCount"] = appStats.fsWriteCount + 1; // Anticipate this successful save
  doc["fsReadCount"] = appStats.fsReadCount;
  doc["fsWritesToday"] = appStats.fsWritesToday;

  doc["dailyTaskTotal"] = appStats.dailyTaskTotal;
  doc["dailyTaskDone"] = appStats.dailyTaskDone;
  doc["dailyTallyDate"] = appStats.dailyTallyDate;
  doc["monthlyTaskTotal"] = appStats.monthlyTaskTotal;
  doc["monthlyTaskDone"] = appStats.monthlyTaskDone;
  doc["monthlyTallyMonth"] = appStats.monthlyTallyMonth;
  JsonArray dilDailyDays = doc.createNestedArray("diligenceDailyDays");
  for (int i = 0; i < 7; i++) dilDailyDays.add(appStats.diligenceDailyDays[i]);
  JsonArray dilDailyDone = doc.createNestedArray("diligenceDailyDone");
  for (int i = 0; i < 7; i++) dilDailyDone.add(appStats.diligenceDailyDone[i]);
  JsonArray dilDailyTotal = doc.createNestedArray("diligenceDailyTotal");
  for (int i = 0; i < 7; i++) dilDailyTotal.add(appStats.diligenceDailyTotal[i]);
  JsonArray dilMonthlyMonths = doc.createNestedArray("diligenceMonthlyMonths");
  for (int i = 0; i < 12; i++) dilMonthlyMonths.add(appStats.diligenceMonthlyMonths[i]);
  JsonArray dilMonthlyDone = doc.createNestedArray("diligenceMonthlyDone");
  for (int i = 0; i < 12; i++) dilMonthlyDone.add(appStats.diligenceMonthlyDone[i]);
  JsonArray dilMonthlyTotal = doc.createNestedArray("diligenceMonthlyTotal");
  for (int i = 0; i < 12; i++) dilMonthlyTotal.add(appStats.diligenceMonthlyTotal[i]);

  JsonArray historyArray = doc.createNestedArray("hourlyPresenceHistoryWeekly");
  for (int d = 0; d < 7; d++) {
    JsonArray dayArray = historyArray.createNestedArray();
    for (int h = 0; h < 24; h++) {
      dayArray.add(appStats.hourlyPresenceHistoryWeekly[d][h]);
    }
  }
  JsonArray msArray = doc.createNestedArray("presenceMsCurrentDay");
  for (int h = 0; h < 24; h++) {
    msArray.add(appStats.presenceMsCurrentDay[h]);
  }

  Serial.println("[STATS] saveDailyStats: Serializing JSON payload...");
  if (serializeJson(doc, file) == 0) {
    Serial.println("[STATS] ERROR: saveDailyStats failed to serialize JSON!");
    file.close();
    return;
  }
  file.close();

  if (LittleFS.exists("/stats.json")) {
    Serial.println("[STATS] saveDailyStats: Removing old stats.json...");
    LittleFS.remove("/stats.json");
  }
  
  Serial.println("[STATS] saveDailyStats: Renaming stats.json.tmp to stats.json...");
  if (LittleFS.rename("/stats.json.tmp", "/stats.json")) {
    appStats.fsWriteCount++;
    appStats.fsWritesToday++;
    Serial.printf("[STATS] saveDailyStats SUCCESS! Total writes: %d\n", appStats.fsWriteCount);
  } else {
    Serial.println("[STATS] ERROR: saveDailyStats rename failed!");
  }
}

// Load daily statistics from LittleFS
void loadDailyStats() {
  appStats.fsReadCount++;
  Serial.println("[STATS] loadDailyStats: Checking if stats.json exists...");
  if (!LittleFS.exists("/stats.json")) {
    Serial.println("[STATS] loadDailyStats: No stats.json found, initializing with empty history.");
    for (int d = 0; d < 7; d++) {
      for (int h = 0; h < 24; h++) {
        appStats.hourlyPresenceHistoryWeekly[d][h] = 0;
      }
    }
    saveDailyStats();
    return;
  }
  
  Serial.println("[STATS] loadDailyStats: Opening stats.json...");
  fs::File file = LittleFS.open("/stats.json", "r");
  if (!file) {
    Serial.println("[STATS] ERROR: loadDailyStats failed to open /stats.json!");
    return;
  }
  DynamicJsonDocument doc(8192);
  Serial.println("[STATS] loadDailyStats: Deserializing JSON payload...");
  DeserializationError error = deserializeJson(doc, file);
  if (!error) {
    appStats.firstSitToday = doc["firstSitToday"] | true;
    appStats.firstSitEpoch = doc["firstSitEpoch"] | 0;
    appStats.breakCount = doc["breakCount"] | 0;
    appStats.totalDeskTime = doc["totalDeskTime"] | 0UL;
    appStats.totalFocusTime = doc["totalFocusTime"] | 0UL;
    appStats.totalBreakTime = doc["totalBreakTime"] | 0UL;
    appStats.overnightBreakDuration = doc["overnightBreakDuration"] | 0UL;
    appStats.lastAwayEpoch = doc["lastAwayEpoch"] | 0;
    appStats.dailyAiRequestCount = doc["dailyAiRequestCount"] | 0;
    appStats.lastNtpDay = doc["lastNtpDay"] | -1;
    appStats.longestSittingStreak = doc["longestSittingStreak"] | 0UL;
    appStats.latestBreakDuration = doc["latestBreakDuration"] | 0UL;
    appStats.totalMotionTime = doc["totalMotionTime"] | 0UL;
    appStats.motionCount = doc["motionCount"] | 0;
    appState.isStopByTracking = false; // Reset to false on boot to prevent stale stop-by rollback loops
    appState.originalLastAwayEpoch = doc["originalLastAwayEpoch"] | 0;
    appState.totalStopByTimeMs = doc["totalStopByTimeMs"] | 0UL;
    appStats.previousLatestBreakDuration = doc["previousLatestBreakDuration"] | 0UL;
    appStats.lastMidnightCheckDay = doc["lastMidnightCheckDay"] | -1;
    // Configuration parameters are loaded on boot from Preferences, not stats.json
    appStats.lunchReminderTriggered = doc["lunchReminderTriggered"] | false;
    appStats.excessiveBreaksTriggered = doc["excessiveBreaksTriggered"] | false;
    appStats.goalCompletedTriggered = doc["goalCompletedTriggered"] | false;
    appStats.morningJournalTriggered = doc["morningJournalTriggered"] | false;
    appStats.preLunchJournalTriggered = doc["preLunchJournalTriggered"] | false;
    appStats.endOfDayJournalTriggered = doc["endOfDayJournalTriggered"] | false;
    appStats.nagQueueIndex = doc["nagQueueIndex"] | 0;
    appStats.dueFiredDay = doc["dueFiredDay"] | "";
    appStats.dueFiredKeys = doc["dueFiredKeys"] | "";
    appStats.fsWriteCount = doc["fsWriteCount"] | 0;
    appStats.fsReadCount = doc["fsReadCount"] | appStats.fsReadCount;
    appStats.fsWritesToday = doc["fsWritesToday"] | 0;
    appStats.dailyTaskTotal = doc["dailyTaskTotal"] | 0;
    appStats.dailyTaskDone = doc["dailyTaskDone"] | 0;
    appStats.dailyTallyDate = doc["dailyTallyDate"] | "";
    appStats.monthlyTaskTotal = doc["monthlyTaskTotal"] | 0;
    appStats.monthlyTaskDone = doc["monthlyTaskDone"] | 0;
    appStats.monthlyTallyMonth = doc["monthlyTallyMonth"] | "";

    if (doc.containsKey("diligenceDailyDays")) {
      JsonArray a = doc["diligenceDailyDays"].as<JsonArray>();
      for (int i = 0; i < 7 && i < a.size(); i++) appStats.diligenceDailyDays[i] = a[i].as<String>();
    }
    if (doc.containsKey("diligenceDailyDone")) {
      JsonArray a = doc["diligenceDailyDone"].as<JsonArray>();
      for (int i = 0; i < 7 && i < a.size(); i++) appStats.diligenceDailyDone[i] = a[i];
    }
    if (doc.containsKey("diligenceDailyTotal")) {
      JsonArray a = doc["diligenceDailyTotal"].as<JsonArray>();
      for (int i = 0; i < 7 && i < a.size(); i++) appStats.diligenceDailyTotal[i] = a[i];
    }
    if (doc.containsKey("diligenceMonthlyMonths")) {
      JsonArray a = doc["diligenceMonthlyMonths"].as<JsonArray>();
      for (int i = 0; i < 12 && i < a.size(); i++) appStats.diligenceMonthlyMonths[i] = a[i].as<String>();
    }
    if (doc.containsKey("diligenceMonthlyDone")) {
      JsonArray a = doc["diligenceMonthlyDone"].as<JsonArray>();
      for (int i = 0; i < 12 && i < a.size(); i++) appStats.diligenceMonthlyDone[i] = a[i];
    }
    if (doc.containsKey("diligenceMonthlyTotal")) {
      JsonArray a = doc["diligenceMonthlyTotal"].as<JsonArray>();
      for (int i = 0; i < 12 && i < a.size(); i++) appStats.diligenceMonthlyTotal[i] = a[i];
    }

    if (doc.containsKey("historyDaysCountWeekly")) {
      JsonArray countArray = doc["historyDaysCountWeekly"].as<JsonArray>();
      for (int d = 0; d < 7 && d < countArray.size(); d++) {
        appStats.historyDaysCountWeekly[d] = countArray[d];
      }
    } else {
      int historyDaysCount = doc["historyDaysCount"] | 0;
      for (int d = 0; d < 7; d++) {
        appStats.historyDaysCountWeekly[d] = historyDaysCount;
      }
    }

    if (doc.containsKey("hourlyPresenceHistoryWeekly")) {
      JsonArray historyArray = doc["hourlyPresenceHistoryWeekly"].as<JsonArray>();
      for (int d = 0; d < 7 && d < historyArray.size(); d++) {
        JsonArray dayArray = historyArray[d].as<JsonArray>();
        for (int h = 0; h < 24 && h < dayArray.size(); h++) {
          appStats.hourlyPresenceHistoryWeekly[d][h] = dayArray[h];
        }
      }
    } else if (doc.containsKey("hourlyPresenceHistory")) {
      // Migrate old 1D history to all 7 days
      JsonArray historyArray = doc["hourlyPresenceHistory"].as<JsonArray>();
      for (int h = 0; h < 24 && h < historyArray.size(); h++) {
        uint8_t val = historyArray[h];
        for (int d = 0; d < 7; d++) {
          appStats.hourlyPresenceHistoryWeekly[d][h] = val;
        }
      }
    }

    if (doc.containsKey("presenceMsCurrentDay")) {
      JsonArray msArray = doc["presenceMsCurrentDay"].as<JsonArray>();
      for (int h = 0; h < 24 && h < msArray.size(); h++) {
        appStats.presenceMsCurrentDay[h] = msArray[h];
      }
    }
    Serial.println("[STATS] loadDailyStats SUCCESS!");
  } else {
    Serial.printf("[STATS] ERROR: loadDailyStats deserializeJson failed: %s\n", error.c_str());
  }
  file.close();
}

// Asynchronous WiFi Reconnection Checker
void checkWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > WIFI_CHECK_MS) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      if (appConfig.wifiStaticEnabled) {
        WiFi.config(parseIP(appConfig.wifiIp), parseIP(appConfig.wifiGw), 
                    parseIP(appConfig.wifiSubnet), parseIP(appConfig.wifiDns1), 
                    parseIP(appConfig.wifiDns2));
      }
      WiFi.begin(appConfig.wifiSsid.c_str(), appConfig.wifiPass.c_str());
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to connect
  Serial.println("\n[DIAGNOSTICS] setup() started");

  // Bind WiFiClient to PubSubClient dynamically
  mqttClient.setClient(wifiClient);
  Serial.println("[DIAGNOSTICS] mqttClient bound");

  // Setup Mutex for AI Thread Safety
  appState.aiMutex = xSemaphoreCreateMutex();
  Serial.println("[DIAGNOSTICS] Mutex created");

  // Setup Mutex for MQTT History Thread Safety
  
  // Setup Mutex for MQTT Publish Queue Thread Safety
  mqttPublishQueueMutex = xSemaphoreCreateMutex();

  // Setup Mutex for System Logging Thread Safety

  // Setup persistent background task for AI HTTPS Queries
  xTaskCreate(
    aiQueryTask,
    "AIQuery",
    12288,
    NULL,
    1,
    &aiQueryTaskHandle
  );
  Serial.println("[DIAGNOSTICS] AIQuery task created");


  // Load persistent configurations
  preferences.begin("deskbuddy", false);
  appConfig.aiMode = preferences.getInt("aiMode", 1);
  appConfig.aiPersona = preferences.getInt("aiPersona", 0);
  appConfig.clockFace = preferences.getInt("clockFace", 0);
  appConfig.targetHours = preferences.getFloat("targetHours", 8.0);
  appConfig.userName = preferences.getString("userName", "human");
  appConfig.focusDistanceLimit = preferences.getInt("focusDistLim", 50);
  appConfig.motionRatioLimit = preferences.getInt("motionRatioLim", 15);
  appConfig.deskDistanceLimit = preferences.getInt("distLimit", 120);
  appConfig.filterWindow = preferences.getFloat("filterWindow", 2.0);
  appConfig.hasMail = preferences.getBool("hasMail", false);
  appConfig.time24h = preferences.getBool("time24h", true);
  appConfig.buddyFontIndex = preferences.getInt("buddyFontIdx", 0);
  appConfig.g0mSens = preferences.getInt("g0mSens", 90);
  appConfig.g0sSens = preferences.getInt("g0sSens", 90);
  appConfig.g1mSens = preferences.getInt("g1mSens", 60);
  appConfig.g1sSens = preferences.getInt("g1sSens", 40);
  appConfig.g2mSens = preferences.getInt("g2mSens", 50);
  appConfig.g2sSens = preferences.getInt("g2sSens", 40);
  appConfig.g3mSens = preferences.getInt("g3mSens", 40);
  appConfig.g3sSens = preferences.getInt("g3sSens", 40);
  appConfig.g4mSens = preferences.getInt("g4mSens", 45);
  appConfig.g4sSens = preferences.getInt("g4sSens", 40);
  appConfig.g5mSens = preferences.getInt("g5mSens", 50);
  appConfig.g5sSens = preferences.getInt("g5sSens", 40);
  appConfig.g6mSens = preferences.getInt("g6mSens", 50);
  appConfig.g6sSens = preferences.getInt("g6sSens", 40);

  // Load WiFi credentials
  appConfig.wifiSsid = preferences.getString("wifiSsid", DEFAULT_SSID);
  appConfig.wifiPass = preferences.getString("wifiPass", DEFAULT_PASS);
  appConfig.wifiStaticEnabled = preferences.getBool("wifiStatic", true);
  appConfig.wifiIp = preferences.getString("wifiIp", "192.168.15.160");
  appConfig.wifiGw = preferences.getString("wifiGw", "192.168.15.1");
  appConfig.wifiSubnet = preferences.getString("wifiSubnet", "255.255.255.0");
  appConfig.wifiDns1 = preferences.getString("wifiDns1", "1.1.1.1");
  appConfig.wifiDns2 = preferences.getString("wifiDns2", "8.8.8.8");

  // Load MQTT broker
  appConfig.mqttBroker = preferences.getString("mqttBroker", MQTT_BROKER_IP);
  appConfig.mqttPort = preferences.getInt("mqttPort", MQTT_BROKER_PORT);

  // Load API keys
  appConfig.groqApiKey = preferences.getString("groqKey", GroqApiKey);
  appConfig.geminiApiKey = preferences.getString("geminiKey", GeminiApiKey);
  appConfig.deepseekApiKey = preferences.getString("deepseekKey", DeepSeekApiKey);
  appConfig.openWeatherKey = preferences.getString("owKey", OpenWeatherKey);
  appConfig.openWeatherLat = preferences.getFloat("owLat", -23.11);
  appConfig.openWeatherLon = preferences.getFloat("owLon", -46.53);
  preferences.end();

  // Initialize LittleFS & load daily stats
  Serial.println("[SYSTEM] Mounting LittleFS...");
  if (LittleFS.begin(true)) {
    Serial.println("[SYSTEM] LittleFS mounted successfully.");
    loadDailyStats();
  } else {
    Serial.println("[SYSTEM] ERROR: LittleFS mount failed!");
  }

  // Initialize TFT Display & show splash screen
  tft.init();
  tft.setRotation(0);
  drawRLEImage("/away.rle", 0, 0);
  
  unsigned long bootStartTime = millis();

  // Initialize Radar Sensor Subsystem
  setupRadar();

  // Set Hostname & Configure WiFi
  WiFi.mode(WIFI_STA);
  if (appConfig.wifiStaticEnabled) {
    WiFi.config(parseIP(appConfig.wifiIp), parseIP(appConfig.wifiGw), 
                parseIP(appConfig.wifiSubnet), parseIP(appConfig.wifiDns1), 
                parseIP(appConfig.wifiDns2));
  }
  WiFi.setHostname("deskbuddy");
  WiFi.begin(appConfig.wifiSsid.c_str(), appConfig.wifiPass.c_str());

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS) {
    delay(100);
  }

  // WiFi failed — start captive portal AP mode
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection failed — starting captive portal AP mode");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    dnsServer.start(53, "*", WiFi.softAPIP());
    appState.captivePortalMode = true;
    Serial.printf("[AP] Started: %s — IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.printf("[WIFI] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin("deskbuddy")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[MDNS] Started: http://deskbuddy.local");
    }
  }

  // Setup MQTT client
  setupMqtt();

  // Setup NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(NTP_TIME_OFFSET);

  // Setup Web Server Subsystem
  setupWebServer();

  // Setup OTA Updates
  ArduinoOTA
    .onStart([]() {
      appState.otaInProgress = true;
    })
    .onEnd([]() {
    })
    .onProgress([](unsigned int progress, unsigned int total) {
    })
    .onError([](ota_error_t error) {
    });
  ArduinoOTA.begin();

  appState.lastLoopTime = millis();
  appState.lastStateTransitionTime = millis();
  
  // Force NTP and Weather update on the very first loop execution
  lastHourlyUpdate = millis() - NTP_INTERVAL_MS - 1000;

  // Ensure splash screen displays for at least 4 seconds total at boot
  unsigned long elapsedBoot = millis() - bootStartTime;
  if (elapsedBoot < BOOT_SPLASH_MS) {
    delay(BOOT_SPLASH_MS - elapsedBoot);
  }
}

extern int dateToDays(String dateStr);

inline void checkDueTasks(int currentHour, int currentMin, String currentDayString) {
  if (!LittleFS.exists("/todo.json")) return;
  fs::File file = LittleFS.open("/todo.json", "r");
  if (!file) return;
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return;
  
  // F10 (T2): track which tasks already fired today so catch-up never re-announces them.
  // Persisted in stats.json so a reboot doesn't clear the day's already-announced tasks.
  if (appStats.dueFiredDay != currentDayString) {
    appStats.dueFiredDay = currentDayString;
    appStats.dueFiredKeys = "";
  }
  
  int nowMins = currentHour * 60 + currentMin;
  String dueTasksDetail = "";

  if (doc.containsKey("daily")) {
    JsonArray daily = doc["daily"].as<JsonArray>();
    for (JsonObject task : daily) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveToday = false;
      int tHour = task["hour"] | 12;
      int tMin = task["minute"] | 0;
      String taskText = task["text"] | "";
      String tDate = task["startDate"] | "";
      
      if (isRecurrent) {
        String endDate = task["endDate"] | "";
        if ((tDate.length() == 0 || currentDayString >= tDate) &&
            (endDate.length() == 0 || currentDayString < endDate)) {
          isActiveToday = true;
        }
        if (task.containsKey("completedDates")) {
          JsonArray compDates = task["completedDates"].as<JsonArray>();
          for (JsonVariant d : compDates) {
            if (d.as<String>() == currentDayString) {
              isCompleted = true;
              break;
            }
          }
        }
      } else {
        String targetDate = task["targetDate"] | "";
        isActiveToday = (targetDate == currentDayString);
        isCompleted = task["completed"] | false;
      }
      
      if (isActiveToday && !isCompleted) {
        int dueMins = tHour * 60 + tMin;
        String dueKey = String(tHour) + ":" + String(tMin) + "|" + taskText;
        // F10 (T2): fire once the task time has arrived, even if a previous minute was missed
        if (dueMins <= nowMins && appStats.dueFiredKeys.indexOf(dueKey) == -1) {
          if (dueTasksDetail.length() > 0) {
            dueTasksDetail += "|";
          }
          dueTasksDetail += taskText;
          appStats.dueFiredKeys += dueKey + ";";
        }
      }
    }
  }

  if (dueTasksDetail.length() > 0) {
    // F10 (T2/T4): route through MessageManager so a task due while away lines up for display on return
    messageManager.scheduleMessageWithPriority(
      EVENT_TASK_DUE,
      dueTasksDetail,
      MessageManager::P_HIGH, 0, MessageManager::R_IMPORTANT
    );
    saveDailyStats();
  }
}

// Late hours = outside the learned workday window (start -30 min pad, end +2h grace),
// or any time when the clock isn't set. The grace period keeps normal workday behaviour
// (welcome-backs, breaks) running for a while after the learned workday ends.
static bool isLateHoursNow() {
  if (!timeClient.isTimeSet()) return true;
  int learnedStart = getLearnedWorkdayStart(ts.tm_wday);
  int learnedEnd = getLearnedWorkdayEnd(ts.tm_wday);
  int currentLocalMinutes = ts.tm_hour * 60 + ts.tm_min;
  int paddingMinutes = LATEHOURS_PADDING_MS / 60000;
  int graceMinutes = LATEHOURS_POST_END_GRACE_MS / 60000;
  int workdayStartMinutes = learnedStart * 60 - paddingMinutes;
  int workdayEndMinutes = learnedEnd * 60 + graceMinutes;
  if (currentLocalMinutes >= workdayStartMinutes && currentLocalMinutes < workdayEndMinutes) {
    return false;
  }
  return true;
}

void loop(void) {
  // Poll critical background systems
  ArduinoOTA.handle();
  if (appState.otaInProgress) {
    delay(50);
    return;
  }
  if (appState.captivePortalMode) {
    dnsServer.processNextRequest();
  }
  unsigned long startWeb = millis();
  server.handleClient();
  if (millis() - startWeb > 2UL) {
    appState.lastWebActivityTime = millis();
  }
  if (!appState.captivePortalMode) {
    checkWiFiConnection();
  }

  unsigned long now = millis();
  
  // Safety timeout for AI Query (reset isAILoading if it hangs for > 45s)
  if (appState.isAILoading && (now - appState.lastAiQueryStartTime > 45000)) {
    Logger::log("BEHAVIOUR", "AI Query Timeout: forcing isAILoading = false");
    appState.isAILoading = false;
  }

  unsigned long elapsed = now - appState.lastLoopTime;
  appState.lastLoopTime = now;

  // Keep local time struct and date string updated from NTP
  if (timeClient.isTimeSet()) {
    if (appState.sitDownEpoch == 0) {
      appState.sitDownEpoch = timeClient.getEpochTime();
    }
    if (appStats.firstSitEpoch == 0 && !appStats.firstSitToday) {
      appStats.firstSitEpoch = (appState.sitDownEpoch > 0) ? appState.sitDownEpoch : timeClient.getEpochTime();
      saveDailyStats();
    }
    // Evaluate initial session state on first NTP sync after reboot/boot.
    // This performs crash recovery or day rollover if the device booted across days.
    static bool bootStateEvaluated = false;
    if (!bootStateEvaluated) {
      bootStateEvaluated = true;
      if (currentSessionState == PRESENCE_AWAY && appStats.lastAwayEpoch > 0) {
        uint32_t currentEpoch = timeClient.getEpochTime();
        int currentDay = timeClient.getDay();
        uint32_t referenceAwayEpoch = appState.isStopByTracking ? appState.originalLastAwayEpoch : appStats.lastAwayEpoch;
        if (shouldResetDaySession(currentEpoch, referenceAwayEpoch, currentDay, appStats.lastNtpDay)) {
          // A day rollover is verified (e.g. overnight absence or timezone transition).
          // Merge accumulated presence statistics from the prior day into history before clearing.
          int mergeDay = (appStats.lastNtpDay != -1) ? appStats.lastNtpDay : currentDay;
          mergeCurrentDayPresence(mergeDay);
          resetDailyStats(appStats.lastAwayEpoch, currentDay);
          currentSessionState = PRESENCE_AWAY;
        } else {
          // Resume current day's active session (crash recovery during workday).
          // Transition to PRESENCE_BREAK to allow session continuity.
          currentSessionState = PRESENCE_BREAK;
          // Set lastAwayEpoch to now so the offline period isn't counted as an inflated break.
          appStats.lastAwayEpoch = currentEpoch;
          appState.originalLastAwayEpoch = currentEpoch;
          appState.totalStopByTimeMs = 0;
          appState.isStopByTracking = false;
        }
      }
    }
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      ts = *ptm;
      strftime(buf, sizeof(buf), "%a %d/%m", ptm);
    }
  }

  // Initialize lastNtpDay if not set yet
  if (appStats.lastNtpDay == -1 && WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    appStats.lastNtpDay = timeClient.getDay();
    saveDailyStats();
  }

  // Midnight diagnostics reset
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentDay = timeClient.getDay();
    if (appStats.lastMidnightCheckDay == -1) {
      appStats.lastMidnightCheckDay = currentDay;
    } else if (currentDay != appStats.lastMidnightCheckDay) {
      // Midnight: merge current day's presence before resetting
      int mergeDay = (appStats.lastNtpDay != -1) ? appStats.lastNtpDay : currentDay;
      mergeCurrentDayPresence(mergeDay);
      appStats.lastMidnightCheckDay = currentDay;
      resetDailyStats(appStats.lastAwayEpoch, currentDay);
    }
  }

  static bool stablePresence = false;
  static bool lastRawPresent = false;
  static unsigned long lastRawPresentChangeTime = 0;

  bool rawPresent = false;
  int rawState = STATE_AWAY;
  appState.sensorPresenceDetected = false;
  appState.sensorMovingTargetDetected = false;
  appState.sensorStaticPresenceDetected = false;

  // Read from the physical radar sensor (or use simulated values)
  if (appState.simulationMode) {
    appState.sensorPresenceDetected = appState.simulatedPresent;
    appState.sensorMovingTargetDetected = appState.simulatedMoving;
    appState.sensorStaticPresenceDetected = appState.simulatedMoving;
    appState.rawDetectionDist = appState.simulatedPresent ? appState.simulatedDistance : 0;

    if (!appState.simulationContinuous) {
      appState.simulationMode = false;
    }
  } else {
  radar.read();
  if (radar.isConnected()) {
    appState.sensorPresenceDetected = radar.presenceDetected();
    appState.sensorStaticPresenceDetected = radar.stationaryTargetDetected();

    if (radar.presenceDetected()) {
      appState.rawDetectionDist = radar.detectionDistance();
    } else {
      appState.rawDetectionDist = 0;
    }

    // Update filtered values at a fixed 10Hz frequency (every 100ms)
    static unsigned long lastFilterUpdate = 0;
    static bool filteredMovingTarget = false;
    if (now - lastFilterUpdate >= FILTER_UPDATE_MS) {
      lastFilterUpdate = now;
      
      // Filter motion detection
      motionFilter.add(radar.movingTargetDetected() ? 1.0f : 0.0f);
      filteredMovingTarget = (motionFilter.getMedian(MOTION_FILTER_SIZE) > FILTER_MOTION_THRESHOLD);

      if (appState.rawDetectionDist > 0) {
        detectionDistFilter.add((float)appState.rawDetectionDist);
        // Accumulate session distance stats
        appState.sessionDistanceSum += appState.rawDetectionDist;
        appState.sessionDistanceCount++;
        appState.sessionDistanceAverage = (float)appState.sessionDistanceSum / appState.sessionDistanceCount;
      }
      int samples = (int)(appConfig.filterWindow * 10.0f);
      if (samples < 1) samples = 1;
      if (samples > DIST_FILTER_SIZE) samples = DIST_FILTER_SIZE;
      if (appState.rawDetectionDist > 0) {
        appState.filteredDetectionDist = detectionDistFilter.getMedian(samples);
      }
    }
    
    appState.sensorMovingTargetDetected = filteredMovingTarget;
  }
  } // end else (non-simulation)

  rawPresent = appState.sensorPresenceDetected && (appState.rawDetectionDist == 0 || appState.rawDetectionDist <= appConfig.deskDistanceLimit);
  if (rawPresent) {
    // Advance 1-second rolling window bucket
    if (now - lastBucketAdvanceTime >= 1000) {
      recentBucketHead = (recentBucketHead + 1) % RECENT_MOTION_WINDOW_S;
      recentMotionBuckets[recentBucketHead] = 0;
      recentDeskBuckets[recentBucketHead] = 0;
      lastBucketAdvanceTime = now;
    }

    // Accumulate time into current 1-second bucket
    uint16_t tickMs = (elapsed < 1000) ? (uint16_t)elapsed : 1000;
    recentDeskBuckets[recentBucketHead] += tickMs;
    if (appState.sensorMovingTargetDetected) {
      recentMotionBuckets[recentBucketHead] += tickMs;
    }

    // Calculate windowed motion ratio over the last RECENT_MOTION_WINDOW_S seconds
    uint32_t windowMotionMs = 0;
    uint32_t windowDeskMs = 0;
    for (int i = 0; i < RECENT_MOTION_WINDOW_S; i++) {
      windowMotionMs += recentMotionBuckets[i];
      windowDeskMs += recentDeskBuckets[i];
    }
    int recentMotionRatio = (windowDeskMs > 0) ? (int)((windowMotionMs * 100) / windowDeskMs) : 0;
    if (recentMotionRatio > 100) recentMotionRatio = 100;

    float currentDist = (appState.sessionDistanceAverage > 0.0) ? appState.sessionDistanceAverage : (float)appState.rawDetectionDist;
    bool inFocusZone = (currentDist > 0.0 && currentDist < appConfig.focusDistanceLimit);
    bool highMotion = (recentMotionRatio > appConfig.motionRatioLimit);

    if (inFocusZone) {
      rawState = highMotion ? STATE_BUSY : STATE_FOCUS;
    } else {
      rawState = highMotion ? STATE_DISTRACTED : STATE_REGULAR;
    }
  } else {
    rawState = STATE_AWAY;
  }

  // Debouncing logic to filter sensor instability/boundary jitter
  if (rawPresent != lastRawPresent) {
    lastRawPresent = rawPresent;
    lastRawPresentChangeTime = now;
  }

  if (rawPresent != stablePresence) {
    unsigned long debounceLimit = 0;
    if (rawPresent) {
      debounceLimit = appStats.firstSitToday ? DEBOUNCE_PRESENCE_OVERNIGHT_MS : DEBOUNCE_PRESENCE_MS;
    } else {
      debounceLimit = DEBOUNCE_AWAY_MS;
    }
    if (now - lastRawPresentChangeTime >= debounceLimit) {
      stablePresence = rawPresent;
    }
  }

  bool targetPresent = stablePresence;
  int targetState = stablePresence ? ((rawState != STATE_AWAY) ? rawState : STATE_REGULAR) : STATE_AWAY;

  // Handle Presence State Machine Transitions
  if (targetPresent) {
    accumulatePresence(ts.tm_hour, elapsed);
    // Accumulate desk time if present
    appStats.totalDeskTime += elapsed;
    appState.sessionDeskTime += elapsed;
    
    // Accumulate focus time
    if (appState.currentPresenceState == STATE_FOCUS) {
      appStats.totalFocusTime += elapsed;
    }

    // Accumulate motion time
    if (appState.sensorMovingTargetDetected) {
      appState.sessionMotionTime += elapsed;
      appStats.totalMotionTime += elapsed;
    }

    // Count motion occurrences
    static bool lastMovingState = false;
    if (appState.sensorMovingTargetDetected) {
      if (!lastMovingState) {
        appStats.motionCount++;
        lastMovingState = true;
      }
    } else {
      lastMovingState = false;
    }

    if (appState.currentPresenceState == STATE_AWAY) {
      // Transition: Away -> Present
      appState.sitDownTime = now;
      appState.sitDownEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
      appState.lastNagTime = now;
      currentSitDownSessionId++;
      Logger::log("STATE", "Away->Present: sit=%lu epoch=%s session=%d state=%s", 
                  now, Logger::formatEpoch(appState.sitDownEpoch).c_str(), currentSitDownSessionId, presenceStateName(targetState));
      
      // Calculate currentBreakDurationMs immediately at the transition using transition timestamps
      appState.currentBreakDurationMs = 0;
      uint32_t referenceAwayEpoch = appState.isStopByTracking ? appState.originalLastAwayEpoch : appStats.lastAwayEpoch;
      if (referenceAwayEpoch > 0 && appState.sitDownEpoch >= referenceAwayEpoch) {
        unsigned long grossSec = appState.sitDownEpoch - referenceAwayEpoch;
        unsigned long grossMs = grossSec * 1000UL;
        if (grossMs >= appState.totalStopByTimeMs) {
          appState.currentBreakDurationMs = grossMs - appState.totalStopByTimeMs;
        }
      } else if (appState.lastStateTransitionTime > 0 && now >= appState.lastStateTransitionTime) {
        unsigned long grossMs = now - appState.lastStateTransitionTime;
        if (grossMs >= appState.totalStopByTimeMs) {
          appState.currentBreakDurationMs = grossMs - appState.totalStopByTimeMs;
        }
      }
      Logger::log("STATE", "Away->Present: refAway=%s gross=%lu s break=%lu s stopBy=%lu s", 
                  Logger::formatEpoch(referenceAwayEpoch).c_str(), 
                  (appState.currentBreakDurationMs + appState.totalStopByTimeMs) / 1000UL, 
                  appState.currentBreakDurationMs / 1000UL, 
                  appState.totalStopByTimeMs / 1000UL);

      // Check if this is the first sit of the day
      bool isFirstSit = (currentSessionState == PRESENCE_AWAY);
      bool wasFirstSitToday = appStats.firstSitToday;
      
      // Perform rollover check if we are in PRESENCE_BREAK
      bool willRollover = false;
      if (currentSessionState == PRESENCE_BREAK && WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
        int currentDay = timeClient.getDay();
        if (shouldResetDaySession(appState.sitDownEpoch, referenceAwayEpoch, currentDay, appStats.lastNtpDay)) {
          willRollover = true;
          isFirstSit = true;
          uint32_t tempLastAway = appStats.lastAwayEpoch;
          resetDailyStats(tempLastAway, currentDay);
        }
      }
      Logger::log("STATE", "Away->Present: rollover=%d isFirstSit=%d wasFirstToday=%d", willRollover, isFirstSit, wasFirstSitToday);

      bool sitIsLateHours = isLateHoursNow();

      if (appStats.firstSitToday) {
        if (sitIsLateHours) {
          // Late-hours sit: the day hasn't started yet, so HOLD the first-sit flag.
          // Mirror the burn's session bookkeeping so the StopBy rollback can still
          // preserve lastAwayEpoch if this turns out to be a blip.
          appStats.latestBreakDuration = 0;
          appStats.totalBreakTime = 0;
          appState.wasFirstSitThisSession = true;
          appState.isStopByTracking = true;
          appState.originalLastAwayEpoch = appStats.lastAwayEpoch;
          appState.totalStopByTimeMs = 0;
          appStats.previousLatestBreakDuration = 0;
          resetSessionStats();
          Logger::log("STATE", "Away->Present: Late-hours sit: first-sit flag held");
        } else {
          appStats.firstSitToday = false;
          appStats.firstSitEpoch = appState.sitDownEpoch;
          if (appStats.lastAwayEpoch > 0 && appStats.firstSitEpoch >= appStats.lastAwayEpoch) {
            appStats.overnightBreakDuration = appStats.firstSitEpoch - appStats.lastAwayEpoch;
          } else {
            appStats.overnightBreakDuration = appState.currentBreakDurationMs / 1000UL;
          }
          appStats.latestBreakDuration = 0; // Do not count overnight away time as the "latest break duration"
          
          appStats.totalBreakTime = 0;
          appState.wasFirstSitThisSession = true;
          appState.isStopByTracking = true;
          appState.originalLastAwayEpoch = appStats.lastAwayEpoch;
          appState.totalStopByTimeMs = 0;
          appStats.previousLatestBreakDuration = 0;
          Logger::log("STATE", "Away->Present: Handled firstSit: firstSitEpoch=%u overnight=%lu", appStats.firstSitEpoch, appStats.overnightBreakDuration);
          saveDailyStats();
          resetSessionStats();
        }
      } else {
        // Standard break return check using pre-calculated duration from transition
        if (appState.currentBreakDurationMs >= BREAK_MINIMUM_MS) { // Only count break if away > 3 minutes
          appStats.breakCount++;
          appStats.previousLatestBreakDuration = appStats.latestBreakDuration;
          appStats.latestBreakDuration = appState.currentBreakDurationMs;
          appState.isStopByTracking = true;
          Logger::log("STATE", "Away->Present: Handled standard break return: breakCount=%d duration=%lu s", appStats.breakCount, appStats.latestBreakDuration / 1000UL);
          saveDailyStats();
          resetSessionStats();
        } else {
          Logger::log("STATE", "Away->Present: Ignored brief return (breakMs=%lu < min=%lu)", appState.currentBreakDurationMs, BREAK_MINIMUM_MS);
        }
      }

      unsigned long lastAwaySliceMs = (appState.lastStateTransitionTime > 0 && now >= appState.lastStateTransitionTime) ? (now - appState.lastStateTransitionTime) : 0;
      appState.currentPresenceState = targetState;
      currentSessionState = PRESENCE_SITTING;
      appState.lastStateTransitionTime = now;
      appState.continuousPresenceStart = now;
      appState.lastStretchReminderTime = now;
      if (targetState == STATE_FOCUS) {
        appState.continuousStillStart = now;
      }

      // Smooth transition: schedule API welcome/fallback query on sit-down
      // (The display will render the clock faceplate, and the welcome message will show 15s later)
      if (sitIsLateHours) {
        // Late hours: a real break (over the standard leeway) gets a late-hours message
        // instead of the standard greetings. Brief returns under the leeway stay silent.
        if (appState.currentBreakDurationMs >= BREAK_MINIMUM_MS) {
          messageManager.scheduleLateHoursSitMessage(computeEarlyLateString(ts));
        }
      } else if (wasFirstSitToday || willRollover) {
        unsigned long overnightBreak = appState.currentBreakDurationMs / 1000UL;
        String breakStr = (overnightBreak >= OVERNIGHT_THRESHOLD_S) ? formatTime(overnightBreak * 1000) : "";
        messageManager.scheduleFirstSitMessage(breakStr);
      } else {
        if (lastAwaySliceMs >= BREAK_MINIMUM_MS) {
          String tempBreakDuration = formatTime(appState.currentBreakDurationMs);
          double hoursWorked = (double)appStats.totalDeskTime / 3600000.0;
          bool excessive = false;
          if (hoursWorked > EXCESSIVE_BREAKS_MIN_WORKED_HOURS) {
            double breakRate = (double)appStats.breakCount / hoursWorked;
            if (breakRate > EXCESSIVE_BREAKS_LIMIT_PER_HOUR) {
              excessive = true;
            }
          }
          if (excessive && !appStats.excessiveBreaksTriggered) {
            appStats.excessiveBreaksTriggered = true;
            saveDailyStats();
            // Roast is demoted below the welcome greeting: it fires after (never replaces) it.
            messageManager.scheduleMessageWithPriority(
              EVENT_EXCESSIVE_BREAKS,
              tempBreakDuration,
              MessageManager::P_NORMAL, WELCOME_DELAY_MS, MessageManager::R_IMPORTANT
            );
          }
          messageManager.scheduleWelcomeBackMessage(tempBreakDuration);
        }
      }
    } else {
      // User is present, and was already present
      static int candidateState = -1;
      static unsigned long stateConfirmationTime = 0;
      
      if (targetState != appState.currentPresenceState && targetState != STATE_AWAY) {
        if (targetState != candidateState) {
          candidateState = targetState;
          stateConfirmationTime = now;
        } else if (now - stateConfirmationTime >= STICKY_CONFIRM_MS) { // 30 seconds
          if (candidateState == STATE_FOCUS) {
            appState.continuousStillStart = stateConfirmationTime; // Include the 30-second confirmation window
          }
          int prevState = appState.currentPresenceState;
          int confirmedState = candidateState;
          appState.currentPresenceState = confirmedState;
          candidateState = -1;
          // F11 (T5): celebrate a focus session that ends IN PLACE (FOCUS -> BUSY/REGULAR), never on leaving
          if (prevState == STATE_FOCUS && (confirmedState == STATE_BUSY || confirmedState == STATE_REGULAR)) {
            unsigned long focusSessionDuration = now - appState.continuousStillStart;
            if (focusSessionDuration > FOCUS_MINIMUM_MS) {
              messageManager.scheduleMessageWithPriority(
                EVENT_FOCUS_END,
                formatTime(focusSessionDuration),
                MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
              );
            }
          }
        }
      } else {
        candidateState = -1;
      }
    }
      
    // Day-start: a held late-hours sit still active when work hours begin burns the flag silently
    // (the day started with this sit). firstSitEpoch = sitDownEpoch so the late-hours time counts.
    if (appStats.firstSitToday && appState.wasFirstSitThisSession && appState.currentPresenceState != STATE_AWAY && !isLateHoursNow()) {
      appStats.firstSitToday = false;
      appStats.firstSitEpoch = appState.sitDownEpoch;
      if (appStats.lastAwayEpoch > 0 && appStats.firstSitEpoch >= appStats.lastAwayEpoch) {
        appStats.overnightBreakDuration = appStats.firstSitEpoch - appStats.lastAwayEpoch;
      } else {
        appStats.overnightBreakDuration = (now - appState.sitDownTime) / 1000UL;
      }
      appState.wasFirstSitThisSession = false;
      saveDailyStats();
      Logger::log("STATE", "Late-hours sit crossed into work hours: day started firstSitEpoch=%u overnight=%lu", appStats.firstSitEpoch, appStats.overnightBreakDuration);
    }

    // Process MessageManager for proactive scheduling
    // Dispatch direct, bypassing triggerBehaviour's local-quote/AI pipeline
    messageManager.update(millis());
    bool systemBusy = appState.isAILoading || appState.hasNewAIResponse || (millis() < appState.aiScreenEndTime) || appState.pendingWelcomeAlert;
    if (!systemBusy) {
      MessageManager::DueMessage msg = messageManager.getNextDueMessage();
      if (msg.eventType != -1) {
        triggerBehaviour(msg.eventType, msg.content);
      }
    }
      
    // Trigger Stretch alert after 45 minutes of continuous presence
    if (now - appState.lastStretchReminderTime > STRETCH_INTERVAL_MS) {
      appState.lastStretchReminderTime = now;
      messageManager.scheduleMessageWithPriority(
        EVENT_STRETCH,
        "",
        MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
      );
    }

    // Trigger Slacker Roast if sitting > 1 hour and score < 35%
    static unsigned long lastSlackerRoastTime = 0;
    unsigned long continuousSittingTime = now - appState.continuousPresenceStart;
    if (continuousSittingTime > SLACKER_INTERVAL_MS && appStats.productivityScore < 35) {
      if (now - lastSlackerRoastTime > SLACKER_INTERVAL_MS) {
        lastSlackerRoastTime = now;
        messageManager.scheduleMessageWithPriority(
          EVENT_SLACKER,
          "",
          MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
        );
      }
    }

    // Evaluate and update longest sitting streak (minimum 15 minutes)
    unsigned long currentStreak = now - appState.continuousPresenceStart;
    if (appStats.longestSittingStreak >= STREAK_MINIMUM_MS && currentStreak > appStats.longestSittingStreak && !appState.streakAlertTriggered) {
      appState.streakAlertTriggered = true;
      messageManager.scheduleMessageWithPriority(
        EVENT_STREAK_BEATEN,
        formatTime(currentStreak),
        MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
      );
    }
    if (currentStreak > appStats.longestSittingStreak && currentStreak >= STREAK_MINIMUM_MS) {
      appStats.longestSittingStreak = currentStreak;
    }
  } else {
    if (appState.currentPresenceState != STATE_AWAY) {
      // Transition: Present -> Away
      appState.streakAlertTriggered = false;
      
      unsigned long focusSessionDuration = 0;
      if (appState.currentPresenceState == STATE_FOCUS) {
        focusSessionDuration = now - appState.continuousStillStart;
      }
      
      // F11 (T5): no FOCUS_END congrats on leaving -- focus sessions are celebrated in place (see sticky-confirm block)
      
      unsigned long presenceDurationMs = now - appState.sitDownTime;
      Logger::log("STATE", "Present->Away: prevState=%s presDur=%lu s focusDur=%lu s stopByTrack=%d", 
                  presenceStateName(appState.currentPresenceState), presenceDurationMs / 1000UL, focusSessionDuration / 1000UL, appState.isStopByTracking);
      
      bool isLateHours = isLateHoursNow();

      if (appState.isStopByTracking && presenceDurationMs < STOP_BY_THRESHOLD_MS && isLateHours) {
        if (appState.wasFirstSitThisSession) {
          // This was a first-sit STOP-BY! Roll back the first sit today status and overnight break values.
          appStats.firstSitToday = true;
          appStats.firstSitEpoch = 0;
          appStats.overnightBreakDuration = 0;
          appStats.lastAwayEpoch = appState.originalLastAwayEpoch;
          appState.isStopByTracking = false;
          appState.wasFirstSitThisSession = false;
          appState.totalStopByTimeMs = 0;
          
          appState.currentPresenceState = STATE_AWAY;
          appState.lastStateTransitionTime = now;
          Logger::log("STATE", "Present->Away: Rolled back firstSit stop-by! firstSitToday=1 lastAwayEpoch=%s", 
                      Logger::formatEpoch(appStats.lastAwayEpoch).c_str());
          saveDailyStats();
        } else {
          // This was a standard STOP-BY! Roll back the break but start fresh from now.
          int oldBreakCount = appStats.breakCount;
          if (appStats.breakCount > 0) appStats.breakCount--;
          appStats.latestBreakDuration = appStats.previousLatestBreakDuration;
          
          appStats.lastAwayEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
          appState.isStopByTracking = false;
          appState.totalStopByTimeMs = 0;
          appState.originalLastAwayEpoch = appStats.lastAwayEpoch;
          
          appState.currentPresenceState = STATE_AWAY;
          appState.lastStateTransitionTime = now;
          Logger::log("STATE", "Present->Away: Stop-By! breakCount %d->%d restoreLatestBreak=%lu lastAwayEpoch=%s", 
                      oldBreakCount, appStats.breakCount, appStats.latestBreakDuration / 1000UL, Logger::formatEpoch(appStats.lastAwayEpoch).c_str());
          saveDailyStats();
        }
      } else {
        // This was a REAL presence session (>= 8 minutes) or we weren't tracking stop-bys
        appState.currentPresenceState = STATE_AWAY;
        appState.lastStateTransitionTime = now;
        appStats.lastAwayEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
        appState.isStopByTracking = false;
        appState.totalStopByTimeMs = 0;
        appState.originalLastAwayEpoch = appStats.lastAwayEpoch; // Save start of this new break session
        appState.wasFirstSitThisSession = false; // Reset first sit session flag
        Logger::log("STATE", "Present->Away: Real session completed. lastAwayEpoch=%s reset stopByTracking", Logger::formatEpoch(appStats.lastAwayEpoch).c_str());
        saveDailyStats();
      }
      
      currentSessionState = PRESENCE_BREAK;
    } else {
      // User is Away, and was already Away
      // Accumulate break time
      appStats.totalBreakTime += elapsed;
    }
    
    // Clear filters and reset values when user is AWAY
    appState.rawDetectionDist = 0;
    appState.filteredDetectionDist = 0.0;
    appState.sessionDistanceSum = 0;
    appState.sessionDistanceCount = 0;
    appState.sessionDistanceAverage = 0.0;
    detectionDistFilter.clear();
    motionFilter.clear();
  }

  // Update dynamic productivity score
  uint32_t currentEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
  uint32_t workdayElapsed = (appStats.firstSitEpoch > 0 && currentEpoch >= appStats.firstSitEpoch) ? (currentEpoch - appStats.firstSitEpoch) : 0;

  if (appStats.firstSitToday || workdayElapsed < SCORE_INITIAL_PERIOD_S) {
    // Default to 100% initially (first 5 minutes of work)
    appStats.productivityScore = 100;
  } else {
    float hoursElapsed = (float)workdayElapsed / 3600.0f;
    
    // 1. Break frequency penalty (target: 1 break/hour = 25% penalty)
    float penalty_breaks = 25.0f * ((float)appStats.breakCount / hoursElapsed);
    
    // 2. Break duration penalty (target: 10% of workday in breaks = 25% penalty)
    unsigned long activeBreakMs = 0;
    unsigned long workdayElapsedMs = (unsigned long)workdayElapsed * 1000;
    if (workdayElapsedMs > appStats.totalDeskTime) {
      activeBreakMs = workdayElapsedMs - appStats.totalDeskTime;
    }
    float breakTimeRatio = (float)(activeBreakMs / 1000.0f) / (float)workdayElapsed;
    float penalty_time = 25.0f * (breakTimeRatio / BREAK_TIME_TARGET);
    
    // 3. Focus bonus (Focus counts 1.5x)
    float bonus_focus = 0.0f;
    if (appStats.totalDeskTime > 0) {
      bonus_focus = FOCUS_BONUS_MULTIPLIER * (((float)appStats.totalFocusTime * 100.0f) / (float)appStats.totalDeskTime);
    }
    
    float raw_score = 100.0f - penalty_breaks - penalty_time + bonus_focus;
    appStats.productivityScore = (int)constrain(raw_score, 0.0f, 100.0f);
  }

  // Handle NTP Time & Weather Fetch Updates (every 1 hour or until initial NTP sync succeeds)
  unsigned long ntpUpdateInterval = timeClient.isTimeSet() ? NTP_INTERVAL_MS : NTP_RETRY_MS;
  if (WiFi.status() == WL_CONNECTED && now - lastHourlyUpdate > ntpUpdateInterval) {
    timeClient.update();
    HTTPClient http;
    String weatherUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + 
                        String(appConfig.openWeatherLat, 4) + "&units=metric&lon=" + 
                        String(appConfig.openWeatherLon, 4) + "&lang=fr&appid=" + 
                        appConfig.openWeatherKey;
    http.begin(weatherUrl);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, payload);
      if (!error) {
        appState.temp = (float)(jsonBuffer["main"]["temp"]);
        if (jsonBuffer["weather"].is<JsonArray>() && jsonBuffer["weather"].as<JsonArray>().size() > 0) {
          appState.weatherDesc = jsonBuffer["weather"][0]["main"].as<String>();
        }
        time_t rawtime = jsonBuffer["dt"];
        rawtime = rawtime + NTP_TIME_OFFSET; // offset is negative -> subtracts 3h
        ts = *localtime(&rawtime);
        strftime(buf, sizeof(buf), "%a %d/%m", &ts);
      }
    }
    http.end();
    
    // Only register update success if time is verified set
    if (timeClient.isTimeSet()) {
      lastHourlyUpdate = now;
    } else {
      // Retry in 15 seconds
      lastHourlyUpdate = now - ntpUpdateInterval + NTP_RETRY_MS;
    }
  }

  // Periodically save stats to LittleFS (every 60 seconds) if anything has changed
  static unsigned long lastStatsSave = 0;
  static bool statsInit = false;
  static unsigned long lastSavedDeskTime = 0;
  static unsigned long lastSavedFocusTime = 0;
  static unsigned long lastSavedBreakTime = 0;
  static int lastSavedBreakCount = 0;
  static uint32_t lastSavedFirstSitEpoch = 0;
  static unsigned long lastSavedLongestStreak = 0;
  static unsigned long lastSavedMotionTime = 0;
  static int lastSavedMotionCount = 0;
  static String lastSavedUserName = "";

  if (!statsInit) {
    lastSavedDeskTime = appStats.totalDeskTime;
    lastSavedFocusTime = appStats.totalFocusTime;
    lastSavedBreakTime = appStats.totalBreakTime;
    lastSavedBreakCount = appStats.breakCount;
    lastSavedFirstSitEpoch = appStats.firstSitEpoch;
    lastSavedLongestStreak = appStats.longestSittingStreak;
    lastSavedMotionTime = appStats.totalMotionTime;
    lastSavedMotionCount = appStats.motionCount;
    lastSavedUserName = appConfig.userName;
    statsInit = true;
  }

  if (now - lastStatsSave > SAVE_INTERVAL_MS) { // Periodically save stats to LittleFS (every 10 minutes)
    lastStatsSave = now;
    if (appStats.totalDeskTime != lastSavedDeskTime || 
        appStats.totalFocusTime != lastSavedFocusTime || 
        appStats.totalBreakTime != lastSavedBreakTime || 
        appStats.breakCount != lastSavedBreakCount ||
        appStats.firstSitEpoch != lastSavedFirstSitEpoch ||
        appStats.longestSittingStreak != lastSavedLongestStreak ||
        appStats.totalMotionTime != lastSavedMotionTime ||
        appStats.motionCount != lastSavedMotionCount ||
        appConfig.userName != lastSavedUserName) {
      saveDailyStats();
      lastSavedDeskTime = appStats.totalDeskTime;
      lastSavedFocusTime = appStats.totalFocusTime;
      lastSavedBreakTime = appStats.totalBreakTime;
      lastSavedBreakCount = appStats.breakCount;
      lastSavedFirstSitEpoch = appStats.firstSitEpoch;
      lastSavedLongestStreak = appStats.longestSittingStreak;
      lastSavedMotionTime = appStats.totalMotionTime;
      lastSavedMotionCount = appStats.motionCount;
      lastSavedUserName = appConfig.userName;
    }
  }

  // Lunch Reminder check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentHour = ts.tm_hour;
    int currentMin = ts.tm_min;
    int currentDay = timeClient.getDay();
    int learnedLunch = getLearnedLunchHour(currentDay);
    if (currentHour == learnedLunch && currentMin >= 15) {
      // F10 (T4): schedule even while away -- MM lines it up for display once present
      if (!appStats.lunchReminderTriggered && appStats.totalDeskTime > LUNCH_MIN_DESK_MS) {
        appStats.lunchReminderTriggered = true;
        saveDailyStats();
        messageManager.scheduleMessageWithPriority(
          EVENT_LUNCH_REMINDER,
          "",
          MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
        );
      }
    }
  }

  // Goal Completion check
  if (appConfig.targetHours > 0.0f) {
    unsigned long targetMs = (unsigned long)(appConfig.targetHours * 3600.0f * 1000.0f);
    if (appStats.totalDeskTime >= targetMs && !appStats.goalCompletedTriggered) {
      appStats.goalCompletedTriggered = true;
      saveDailyStats();
      messageManager.scheduleMessageWithPriority(
        EVENT_GOAL_COMPLETED,
        "",
        MessageManager::P_HIGH, 0, MessageManager::R_IMPORTANT
      );
    }
  }

  // Morning Kickoff Journal check (5 mins sitting delay)
  if (appState.currentPresenceState != STATE_AWAY && !appStats.morningJournalTriggered) {
    unsigned long sitDuration = now - appState.sitDownTime;
    if (sitDuration >= MORNING_JOURNAL_DELAY_MS) {
      appStats.morningJournalTriggered = true;
      saveDailyStats();
      messageManager.scheduleMessageWithPriority(
        EVENT_JOURNAL,
        "",
        MessageManager::P_HIGH, 0, MessageManager::R_NORMAL
      );
    }
  }

  // Pre-Lunch Journal check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentHour = ts.tm_hour;
    int currentMin = ts.tm_min;
    int currentDay = timeClient.getDay();
    int learnedLunch = getLearnedLunchHour(currentDay);
    bool hasHistory = (appStats.historyDaysCountWeekly[currentDay] > 0);
    int refLunch = hasHistory ? learnedLunch : 12; // fallback to 12 PM
    
    int currentMinsFromMidnight = currentHour * 60 + currentMin;
    int lunchThresholdMins = refLunch * 60 - PRE_LUNCH_JOURNAL_MINS_BEFORE; // 15 mins before learned lunch
    
    if (currentMinsFromMidnight >= lunchThresholdMins && currentMinsFromMidnight < refLunch * 60) {
      // F10 (T4): schedule even while away -- MM lines it up for display once present
      if (!appStats.preLunchJournalTriggered) {
        appStats.preLunchJournalTriggered = true;
        saveDailyStats();
        messageManager.scheduleMessageWithPriority(
          EVENT_JOURNAL,
          "",
          MessageManager::P_HIGH, 0, MessageManager::R_NORMAL
        );
      }
    }
  }

  // End-of-Day Journal check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentHour = ts.tm_hour;
    int currentMin = ts.tm_min;
    int currentDay = timeClient.getDay();
    int learnedEnd = getLearnedWorkdayEnd(currentDay);
    bool hasHistory = (appStats.historyDaysCountWeekly[currentDay] > 0);
    int refEnd = hasHistory ? learnedEnd : 18; // fallback to 6 PM (18:00)
    
    int currentMinsFromMidnight = currentHour * 60 + currentMin;
    int endThresholdMins = (refEnd - END_OF_DAY_JOURNAL_HOURS_BEFORE) * 60; // 1 hour before learned workday end
    
    if (currentMinsFromMidnight >= endThresholdMins && currentMinsFromMidnight < refEnd * 60) {
      // F10 (T4): schedule even while away -- MM lines it up for display once present
      if (!appStats.endOfDayJournalTriggered) {
        appStats.endOfDayJournalTriggered = true;
        saveDailyStats();
        messageManager.scheduleMessageWithPriority(
          EVENT_JOURNAL,
          "",
          MessageManager::P_HIGH, 0, MessageManager::R_NORMAL
        );
      }
    }
  }

  // Task Due check (granular checking at the start of each minute)
  static int lastCheckedDueMinute = -1;
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentHour = ts.tm_hour;
    int currentMin = ts.tm_min;
    if (currentMin != lastCheckedDueMinute) {
      lastCheckedDueMinute = currentMin;
      
      int currentYear = ts.tm_year + 1900;
      int currentMonth = ts.tm_mon + 1;
      int currentDay = ts.tm_mday;
      char dStr[11];
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", currentYear, currentMonth, currentDay);
      
      checkDueTasks(currentHour, currentMin, String(dStr));
    }
  }

  // Nagging check (overdue-task queue): rings every 35 min seated, one task per ring,
  // most-expired-first. The cursor persists across sessions and resets at midnight.
  if (appState.currentPresenceState != STATE_AWAY && WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    if (now - appState.lastNagTime >= NAGGING_TRIGGER_DELAY_MS) {
      int currentYear = ts.tm_year + 1900;
      int currentMonth = ts.tm_mon + 1;
      int currentDay = ts.tm_mday;
      char dStr[11];
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", currentYear, currentMonth, currentDay);
      char mStr[8];
      snprintf(mStr, sizeof(mStr), "%04d-%02d", currentYear, currentMonth);
      int nowMinutes = ts.tm_hour * 60 + ts.tm_min;

      std::vector<OverdueTask> queue = buildOverdueTaskQueue(String(dStr), String(mStr), dateToDays(String(dStr)), currentYear, currentMonth, currentDay, nowMinutes);
      if (appStats.nagQueueIndex < (int)queue.size()) {
        appState.lastNagTime = now;
        appStats.nagQueueIndex++;
        saveDailyStats();
        messageManager.scheduleMessageWithPriority(
          EVENT_NAGGING,
          queue[appStats.nagQueueIndex - 1].text,
          MessageManager::P_NORMAL, 0, MessageManager::R_NORMAL
        );
      }
    }
  }

  // Process MQTT service loop
  loopMqtt();

  // Handle Web Server requests (duplicate — already called at top of loop)
  // server.handleClient();

  // Handle OTA updates in the background (duplicate — already called at top of loop)
  // ArduinoOTA.handle();

  // Update TFT Display
  updateTFTDisplay(now);

  // Periodic Log Flusher to Flash (every 5 seconds)
  static unsigned long lastLogFlushTime = 0;
  if (now - lastLogFlushTime >= 5000) {
    lastLogFlushTime = now;

  }

  delay(LOOP_DELAY_MS);
}