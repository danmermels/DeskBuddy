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

// ============================================================
// Named Constants (Imported from Constants.h)
// ============================================================
#include "Constants.h"

// Maximum AI response character count, shared with Display.h and Gemini.h
extern const int AI_RESPONSE_MAX_CHARS = 45;
extern const int DISPLAY_CHARS_PER_LINE = 13;

// ============================================================
// Subsystem Headers (extern globals are linked from this file)
// ============================================================
#include "Behaviour.h"
#include "Learning.h"
#include "PresenceAnalysis.h"
#include "MessageManager.h"
#include "../Credentials.h"
#include "MqttService.h"
#include "Display.h"
#include "Radar.h"
#include "Stats.h"
#include "Gemini.h"
#include "Web.h"
#include "Faceplates.h"

// ============================================================
// TODO: Consolidate ~100 globals into a DeskBuddyState context
//       struct to eliminate extern web across all subsystem headers.
// ============================================================

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
ld2410 radar;
WebServer server(80);

// MessageManager instance
MessageManager messageManager;

// Rolling Median Filters
RollingMedianFilter detectionDistFilter(DIST_FILTER_SIZE);
RollingMedianFilter motionFilter(MOTION_FILTER_SIZE);
float filteredDetectionDist = 0.0;

// Configuration limits
int deskDistanceLimit = DISTANCE_LIMIT_DEFAULT;
int focusDistanceLimit = FOCUS_DISTANCE_LIMIT_DEFAULT;
int motionRatioLimit = MOTION_RATIO_LIMIT_DEFAULT;

// Productivity & Timing Metrics
unsigned long totalDeskTime = 0;
unsigned long totalFocusTime = 0;
unsigned long totalBreakTime = 0;
int breakCount = 0;
int productivityScore = 0;
unsigned long latestBreakDuration = 0;
unsigned long overnightBreakDuration = 0;
uint32_t lastAwayEpoch = 0;
unsigned long currentBreakDurationMs = 0;
bool isStopByTracking = false;
uint32_t originalLastAwayEpoch = 0;
unsigned long totalStopByTimeMs = 0;
unsigned long previousLatestBreakDuration = 0;
int lastMidnightCheckDay = -1;
volatile uint32_t currentSitDownSessionId = 0;
uint32_t geminiQuerySessionId = 0;

int currentPresenceState = STATE_AWAY;
unsigned long lastStateTransitionTime = 0;
unsigned long lastLoopTime = 0;
unsigned long continuousPresenceStart = 0;
unsigned long continuousStillStart = 0;
unsigned long lastStretchReminderTime = 0;

// Asynchronous Gemini AI Variables
volatile bool isAILoading = false;
String aiResponse = "";
volatile bool hasNewAIResponse = false;
volatile bool lastResponseIsAi = false;
String currentPrompt = "";
String lastTriggeredEventDetail = "";
String currentUserName = "human";
SemaphoreHandle_t geminiMutex = NULL;
volatile bool otaInProgress = false;

// MQTT Globals
WiFiClient wifiClient;
PubSubClient mqttClient;

// MQTT History Buffer
MqttMessage mqttHistory[MQTT_HISTORY_SIZE];
int mqttHistoryHead = 0;
int mqttHistoryCount = 0;
SemaphoreHandle_t mqttHistoryMutex = NULL;

// Persistent Preferences & Settings
Preferences preferences;
float targetHours = 8.0;
int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
int aiPersona = 0; // 0 = Coach, 1 = Critic, 2 = Nerd, 3 = Zen
int clockFace = 0;
int dailyAiRequestCount = 0;
String userName = "human";
bool firstSitToday = true;
uint32_t firstSitEpoch = 0;
unsigned long longestSittingStreak = 0;
bool streakAlertTriggered = false;
int lastNtpDay = -1;
int lastTriggeredEventType = EVENT_FIRST_SIT;
float filterWindow = 2.0;
bool hasMail = false;
bool time24h = true;

// Learning and Workday Session variables
uint8_t hourlyPresenceHistory[24] = {0};
uint32_t presenceMsCurrentDay[24] = {0};
int historyDaysCount = 0;
bool lunchReminderTriggered = false;

// Active session rollover variables
unsigned long sitDownTime = 0;
uint32_t sitDownEpoch = 0;
bool rolloverPending = false;
  unsigned long requiredValidationBufferMs = VALIDATION_BUFFER_MS;

// File system access counters (to estimate flash lifecycles)
uint32_t fsWriteCount = 0;
uint32_t fsReadCount = 0;

// Radar Gate Sensitivity Thresholds (0-100)
int g0mSens = 100;
int g0sSens = 50;
int g1mSens = 100;
int g1sSens = 50;
int g2mSens = 100;
int g2sSens = 50;
int g3mSens = 80;
int g3sSens = 50;
int g4mSens = 100;
int g4sSens = 50;
int g5mSens = 100;
int g5sSens = 50;
int g6mSens = 100;
int g6sSens = 50;

// Raw radar values
int rawDetectionDist = 0;
bool sensorPresenceDetected = false;
bool sensorMovingTargetDetected = false;
bool sensorStaticPresenceDetected = false;

// Session-specific and cumulative motion tracking
unsigned long sessionDeskTime = 0;
unsigned long sessionMotionTime = 0;
unsigned long totalMotionTime = 0;
int motionCount = 0;

// Session-specific distance tracking
unsigned long sessionDistanceSum = 0;
unsigned long sessionDistanceCount = 0;
float sessionDistanceAverage = 0.0;

// Animated Ring Colors & Parameters
const RGBColor stateColors[] = {
  {80, 80, 80},     // STATE_AWAY: Dark Grey
  {0, 120, 255},    // STATE_FOCUS: Deep Blue
  {0, 220, 80},     // STATE_BUSY: Forest Green
  {255, 50, 50},    // STATE_DISTRACTED: Soft Red
  {200, 200, 200}   // STATE_REGULAR: Soft White
};

RGBColor currentRingColor = {80, 80, 80};
RGBColor startRingColor = {80, 80, 80};
RGBColor targetRingColor = {80, 80, 80};
unsigned long ringTransitionStart = 0;
const unsigned long ringTransitionDuration = 1000; // 1 second

// NTP Client & Weather Data
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned long lastHourlyUpdate = 0;
int temp = 0;
String weatherDesc = "Clear";
struct tm ts;
char buf[80];

// Static IP Configuration
IPAddress local_IP(192, 168, 15, 160);  // Static IP for DeskBuddy
IPAddress gateway(192, 168, 15, 1);     // Gateway
IPAddress subnet(255, 255, 255, 0);     // Subnet Mask
IPAddress primaryDNS(1, 1, 1, 1);       // Primary DNS
IPAddress secondaryDNS(8, 8, 8, 8);     // Secondary DNS

// UI Pages
int uiPage = 0;
unsigned long aiScreenEndTime = 0;

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
  fs::File file = LittleFS.open("/stats.json.tmp", "w");
  if (!file) {
    return;
  }
  DynamicJsonDocument doc(4096);
  doc["firstSitToday"] = firstSitToday;
  doc["firstSitEpoch"] = firstSitEpoch;
  doc["breakCount"] = breakCount;
  doc["totalDeskTime"] = totalDeskTime;
  doc["totalFocusTime"] = totalFocusTime;
  doc["totalBreakTime"] = totalBreakTime;
  doc["overnightBreakDuration"] = overnightBreakDuration;
  doc["lastAwayEpoch"] = lastAwayEpoch;
  doc["dailyAiRequestCount"] = dailyAiRequestCount;
  doc["lastNtpDay"] = lastNtpDay;
  doc["longestSittingStreak"] = longestSittingStreak;
  doc["latestBreakDuration"] = latestBreakDuration;
  doc["isStopByTracking"] = isStopByTracking;
  doc["originalLastAwayEpoch"] = originalLastAwayEpoch;
  doc["totalStopByTimeMs"] = totalStopByTimeMs;
  doc["previousLatestBreakDuration"] = previousLatestBreakDuration;
  doc["lastMidnightCheckDay"] = lastMidnightCheckDay;
  doc["userName"] = userName;
  doc["deskDistanceLimit"] = deskDistanceLimit;
  doc["focusDistanceLimit"] = focusDistanceLimit;
  doc["motionRatioLimit"] = motionRatioLimit;
  doc["hasMail"] = hasMail;
  doc["time24h"] = time24h;
  doc["targetHours"] = targetHours;
  doc["historyDaysCount"] = historyDaysCount;
  doc["lunchReminderTriggered"] = lunchReminderTriggered;
  doc["fsWriteCount"] = fsWriteCount + 1; // Anticipate this successful save
  doc["fsReadCount"] = fsReadCount;

  JsonArray historyArray = doc.createNestedArray("hourlyPresenceHistory");
  for (int h = 0; h < 24; h++) {
    historyArray.add(hourlyPresenceHistory[h]);
  }
  JsonArray msArray = doc.createNestedArray("presenceMsCurrentDay");
  for (int h = 0; h < 24; h++) {
    msArray.add(presenceMsCurrentDay[h]);
  }

  if (serializeJson(doc, file) == 0) {
    file.close();
    return;
  }
  file.close();

  if (LittleFS.exists("/stats.json")) {
    LittleFS.remove("/stats.json");
  }
  if (LittleFS.rename("/stats.json.tmp", "/stats.json")) {
    fsWriteCount++;
  }
}

// Load daily statistics from LittleFS
void loadDailyStats() {
  fsReadCount++;
  if (!LittleFS.exists("/stats.json")) {
    return;
  }
  fs::File file = LittleFS.open("/stats.json", "r");
  if (!file) {
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (!error) {
    firstSitToday = doc["firstSitToday"] | true;
    firstSitEpoch = doc["firstSitEpoch"] | 0;
    breakCount = doc["breakCount"] | 0;
    totalDeskTime = doc["totalDeskTime"] | 0UL;
    totalFocusTime = doc["totalFocusTime"] | 0UL;
    totalBreakTime = doc["totalBreakTime"] | 0UL;
    overnightBreakDuration = doc["overnightBreakDuration"] | 0UL;
    lastAwayEpoch = doc["lastAwayEpoch"] | 0;
    dailyAiRequestCount = doc["dailyAiRequestCount"] | 0;
    lastNtpDay = doc["lastNtpDay"] | -1;
    longestSittingStreak = doc["longestSittingStreak"] | 0UL;
    latestBreakDuration = doc["latestBreakDuration"] | 0UL;
    isStopByTracking = doc["isStopByTracking"] | false;
    originalLastAwayEpoch = doc["originalLastAwayEpoch"] | 0;
    totalStopByTimeMs = doc["totalStopByTimeMs"] | 0UL;
    previousLatestBreakDuration = doc["previousLatestBreakDuration"] | 0UL;
    lastMidnightCheckDay = doc["lastMidnightCheckDay"] | -1;
    // Configuration parameters are loaded on boot from Preferences, not stats.json
    historyDaysCount = doc["historyDaysCount"] | 0;
    lunchReminderTriggered = doc["lunchReminderTriggered"] | false;
    fsWriteCount = doc["fsWriteCount"] | 0;
    fsReadCount = doc["fsReadCount"] | fsReadCount;

    if (doc.containsKey("hourlyPresenceHistory")) {
      JsonArray historyArray = doc["hourlyPresenceHistory"].as<JsonArray>();
      for (int h = 0; h < 24 && h < historyArray.size(); h++) {
        hourlyPresenceHistory[h] = historyArray[h];
      }
    }
    if (doc.containsKey("presenceMsCurrentDay")) {
      JsonArray msArray = doc["presenceMsCurrentDay"].as<JsonArray>();
      for (int h = 0; h < 24 && h < msArray.size(); h++) {
        presenceMsCurrentDay[h] = msArray[h];
      }
    }
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
      WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
      WiFi.begin(SSID, PASS);
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

  // Setup Mutex for Gemini Thread Safety
  geminiMutex = xSemaphoreCreateMutex();
  Serial.println("[DIAGNOSTICS] Mutex created");

  // Setup Mutex for MQTT History Thread Safety
  mqttHistoryMutex = xSemaphoreCreateMutex();



  // Load persistent configurations
  preferences.begin("deskbuddy", false);
  aiMode = preferences.getInt("aiMode", 1);
  aiPersona = preferences.getInt("aiPersona", 0);
  clockFace = preferences.getInt("clockFace", 0);
  targetHours = preferences.getFloat("targetHours", 8.0);
  userName = preferences.getString("userName", "human");
  focusDistanceLimit = preferences.getInt("focusDistLim", 50);
  motionRatioLimit = preferences.getInt("motionRatioLim", 15);
  deskDistanceLimit = preferences.getInt("distLimit", 120);
  filterWindow = preferences.getFloat("filterWindow", 2.0);
  hasMail = preferences.getBool("hasMail", false);
  time24h = preferences.getBool("time24h", true);
  g0mSens = preferences.getInt("g0mSens", 90);
  g0sSens = preferences.getInt("g0sSens", 90);
  g1mSens = preferences.getInt("g1mSens", 60);
  g1sSens = preferences.getInt("g1sSens", 40);
  g2mSens = preferences.getInt("g2mSens", 50);
  g2sSens = preferences.getInt("g2sSens", 40);
  g3mSens = preferences.getInt("g3mSens", 40);
  g3sSens = preferences.getInt("g3sSens", 40);
  g4mSens = preferences.getInt("g4mSens", 45);
  g4sSens = preferences.getInt("g4sSens", 40);
  g5mSens = preferences.getInt("g5mSens", 50);
  g5sSens = preferences.getInt("g5sSens", 40);
  g6mSens = preferences.getInt("g6mSens", 50);
  g6sSens = preferences.getInt("g6sSens", 40);
  preferences.end();

  // Initialize LittleFS & load daily stats
  if (LittleFS.begin(true)) {
    loadDailyStats();
  }

  // Initialize TFT Display & show splash screen
  tft.init();
  tft.setRotation(0);
  drawRLEImage("/away.rle", 0, 0);
  
  unsigned long bootStartTime = millis();

  // Initialize Radar Sensor Subsystem
  setupRadar();

  // Set Hostname & Configure static IP
  WiFi.setHostname("DeskBuddy");
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(SSID, PASS);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS) {
    delay(100);
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
      otaInProgress = true;
    })
    .onEnd([]() {
    })
    .onProgress([](unsigned int progress, unsigned int total) {
    })
    .onError([](ota_error_t error) {
    });
  ArduinoOTA.begin();

  lastLoopTime = millis();
  lastStateTransitionTime = millis();
  
  // Force NTP and Weather update on the very first loop execution
  lastHourlyUpdate = millis() - NTP_INTERVAL_MS - 1000;

  // Ensure splash screen displays for at least 4 seconds total at boot
  unsigned long elapsedBoot = millis() - bootStartTime;
  if (elapsedBoot < BOOT_SPLASH_MS) {
    delay(BOOT_SPLASH_MS - elapsedBoot);
  }
}

void loop(void) {
  // Poll critical background systems
  ArduinoOTA.handle();
  if (otaInProgress) {
    delay(50);
    return;
  }
  server.handleClient();
  checkWiFiConnection();

  unsigned long now = millis();
  unsigned long elapsed = now - lastLoopTime;
  lastLoopTime = now;

  // Keep local time struct and date string updated from NTP
  if (timeClient.isTimeSet()) {
    if (sitDownEpoch == 0) {
      sitDownEpoch = timeClient.getEpochTime();
    }
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      ts = *ptm;
      strftime(buf, sizeof(buf), "%a %d/%m", ptm);
    }
  }

  // Initialize lastNtpDay if not set yet
  if (lastNtpDay == -1 && WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    lastNtpDay = timeClient.getDay();
    saveDailyStats();
  }

  // Midnight diagnostics reset
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentDay = timeClient.getDay();
    if (lastMidnightCheckDay == -1) {
      lastMidnightCheckDay = currentDay;
    } else if (currentDay != lastMidnightCheckDay) {
      fsReadCount = 0;
      fsWriteCount = 0;
      lastMidnightCheckDay = currentDay;
      saveDailyStats();
    }
  }

  static bool stablePresence = false;
  static unsigned long lastPresenceChangeTime = 0;

  bool rawPresent = false;
  int rawState = STATE_AWAY;
  sensorPresenceDetected = false;
  sensorMovingTargetDetected = false;
  sensorStaticPresenceDetected = false;

  // Read from the physical radar sensor
  radar.read();
  if (radar.isConnected()) {
    sensorPresenceDetected = radar.presenceDetected();
    sensorStaticPresenceDetected = radar.stationaryTargetDetected();

    if (radar.presenceDetected()) {
      rawDetectionDist = radar.detectionDistance();
    } else {
      rawDetectionDist = 0;
    }

    // Update filtered values at a fixed 10Hz frequency (every 100ms)
    static unsigned long lastFilterUpdate = 0;
    static bool filteredMovingTarget = false;
    if (now - lastFilterUpdate >= FILTER_UPDATE_MS) {
      lastFilterUpdate = now;
      
      // Filter motion detection
      motionFilter.add(radar.movingTargetDetected() ? 1.0f : 0.0f);
      filteredMovingTarget = (motionFilter.getMedian(MOTION_FILTER_SIZE) > FILTER_MOTION_THRESHOLD);

      if (rawDetectionDist > 0) {
        detectionDistFilter.add((float)rawDetectionDist);
        // Accumulate session distance stats
        sessionDistanceSum += rawDetectionDist;
        sessionDistanceCount++;
        sessionDistanceAverage = (float)sessionDistanceSum / sessionDistanceCount;
      }
      int samples = (int)(filterWindow * 10.0f);
      if (samples < 1) samples = 1;
      if (samples > DIST_FILTER_SIZE) samples = DIST_FILTER_SIZE;
      if (rawDetectionDist > 0) {
        filteredDetectionDist = detectionDistFilter.getMedian(samples);
      }
    }
    
    sensorMovingTargetDetected = filteredMovingTarget;
  }

  rawPresent = sensorPresenceDetected;
  if (rawPresent) {
    float currentDist = (sessionDistanceAverage > 0.0) ? sessionDistanceAverage : (float)rawDetectionDist;
    bool inFocusZone = (currentDist > 0.0 && currentDist < focusDistanceLimit);
    int motionRatio = (sessionDeskTime > 0) ? (sessionMotionTime * 100) / sessionDeskTime : 0;
    if (motionRatio > 100) motionRatio = 100;
    bool highMotion = (motionRatio > motionRatioLimit);

    if (inFocusZone) {
      rawState = highMotion ? STATE_BUSY : STATE_FOCUS;
    } else {
      rawState = highMotion ? STATE_DISTRACTED : STATE_REGULAR;
    }
  } else {
    rawState = STATE_AWAY;
  }

  // Debouncing logic to filter sensor instability/boundary jitter
  if (rawPresent != stablePresence) {
    unsigned long debounceLimit = rawPresent ? DEBOUNCE_PRESENCE_MS : DEBOUNCE_AWAY_MS;
    if (now - lastPresenceChangeTime > debounceLimit) {
      stablePresence = rawPresent;
    }
  } else {
    lastPresenceChangeTime = now;
  }

  bool targetPresent = stablePresence;
  int targetState = stablePresence ? ((rawState != STATE_AWAY) ? rawState : STATE_REGULAR) : STATE_AWAY;

  // Handle Presence State Machine Transitions
  if (targetPresent) {
    if (!rolloverPending) {
      accumulatePresence(ts.tm_hour, elapsed);
    }
    // Accumulate desk time if static presence is detected
    if (sensorStaticPresenceDetected) {
      totalDeskTime += elapsed;
      sessionDeskTime += elapsed;
    }
    
    // Accumulate focus time
    if (currentPresenceState == STATE_FOCUS) {
      totalFocusTime += elapsed;
    }

    // Accumulate motion time
    if (sensorMovingTargetDetected) {
      sessionMotionTime += elapsed;
      totalMotionTime += elapsed;
    }

    // Count motion occurrences
    static bool lastMovingState = false;
    if (sensorMovingTargetDetected) {
      if (!lastMovingState) {
        motionCount++;
        lastMovingState = true;
      }
    } else {
      lastMovingState = false;
    }

    if (currentPresenceState == STATE_AWAY) {
      // Transition: Away -> Present
      sitDownTime = now;
      sitDownEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
      rolloverPending = true;
      requiredValidationBufferMs = getDynamicValidationBufferMs(ts.tm_hour);
      currentSitDownSessionId++;
      
      // Calculate currentBreakDurationMs immediately at the transition using transition timestamps
      currentBreakDurationMs = 0;
      uint32_t referenceAwayEpoch = isStopByTracking ? originalLastAwayEpoch : lastAwayEpoch;
      if (referenceAwayEpoch > 0 && sitDownEpoch >= referenceAwayEpoch) {
        unsigned long grossSec = sitDownEpoch - referenceAwayEpoch;
        unsigned long grossMs = grossSec * 1000UL;
        if (grossMs >= totalStopByTimeMs) {
          currentBreakDurationMs = grossMs - totalStopByTimeMs;
        }
      } else if (lastStateTransitionTime > 0 && now >= lastStateTransitionTime) {
        unsigned long grossMs = now - lastStateTransitionTime;
        if (grossMs >= totalStopByTimeMs) {
          currentBreakDurationMs = grossMs - totalStopByTimeMs;
        }
      }

      // Check if this sit-down will trigger a day session rollover
      bool willRollover = false;
      if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
        int currentDay = timeClient.getDay();
        if (shouldResetDaySession(sitDownEpoch, referenceAwayEpoch, currentDay, lastNtpDay)) {
          willRollover = true;
        }
      }

      unsigned long lastAwaySliceMs = (lastStateTransitionTime > 0 && now >= lastStateTransitionTime) ? (now - lastStateTransitionTime) : 0;
      currentPresenceState = targetState;
      lastStateTransitionTime = now;
      continuousPresenceStart = now;
      lastStretchReminderTime = now;
      if (targetState == STATE_FOCUS) {
        continuousStillStart = now;
      }

      // Smooth transition: immediately trigger API welcome/fallback query on sit-down
      // (The display will render the clock faceplate, and the welcome message will show 15s later)
      if (firstSitToday || willRollover) {
        unsigned long overnightBreak = currentBreakDurationMs / 1000UL;
        if (overnightBreak >= OVERNIGHT_THRESHOLD_S) {
          triggerBehaviour(EVENT_FIRST_SIT, formatTime(overnightBreak * 1000));
        } else {
          triggerBehaviour(EVENT_FIRST_SIT, "");
        }
      } else {
        if (lastAwaySliceMs >= BREAK_MINIMUM_MS) {
          String tempBreakDuration = formatTime(currentBreakDurationMs);
          messageManager.scheduleWelcomeBackMessage(tempBreakDuration);
        }
      }
    } else {
      // User is present, and was already present
      if (rolloverPending && now - sitDownTime >= requiredValidationBufferMs) {
        rolloverPending = false;
        
        // Retroactively accumulate the validated presence time
        accumulatePresence(ts.tm_hour, requiredValidationBufferMs);
        
        bool isRollover = false;
        if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
          int currentDay = timeClient.getDay();
          uint32_t referenceAwayEpoch = isStopByTracking ? originalLastAwayEpoch : lastAwayEpoch;
          if (shouldResetDaySession(sitDownEpoch, referenceAwayEpoch, currentDay, lastNtpDay)) {
            isRollover = true;
            uint32_t tempLastAway = lastAwayEpoch;
            mergeCurrentDayPresence();
            resetDailyStats(tempLastAway, currentDay);
          }
        }
        
        if (firstSitToday) {
          firstSitToday = false;
          firstSitEpoch = sitDownEpoch;
          if (lastAwayEpoch > 0 && firstSitEpoch >= lastAwayEpoch) {
            overnightBreakDuration = firstSitEpoch - lastAwayEpoch;
          } else {
            overnightBreakDuration = currentBreakDurationMs / 1000UL;
          }
          latestBreakDuration = 0; // Do not count overnight away time as the "latest break duration"
          
          totalBreakTime = 0;
          isStopByTracking = false;
          originalLastAwayEpoch = 0;
          totalStopByTimeMs = 0;
          previousLatestBreakDuration = 0;
          saveDailyStats();
          resetSessionStats();
        } else {
          // Standard break return check using pre-calculated duration from transition
          if (currentBreakDurationMs >= BREAK_MINIMUM_MS) { // Only count break if away > 3 minutes
            breakCount++;
            previousLatestBreakDuration = latestBreakDuration;
            latestBreakDuration = currentBreakDurationMs;
            isStopByTracking = true;
            saveDailyStats();
            resetSessionStats();
          }
        }
      }
      
      // Sticky state transition check (only checked if rollover is confirmed)
      if (!rolloverPending) {
        static int candidateState = -1;
        static unsigned long stateConfirmationTime = 0;
        
        if (targetState != currentPresenceState && targetState != STATE_AWAY) {
          if (targetState != candidateState) {
            candidateState = targetState;
            stateConfirmationTime = now;
          } else if (now - stateConfirmationTime >= STICKY_CONFIRM_MS) { // 3 minutes
            if (candidateState == STATE_FOCUS) {
              continuousStillStart = stateConfirmationTime; // Include the 3-minute confirmation window
            }
            currentPresenceState = candidateState;
            candidateState = -1;
          }
        } else {
          candidateState = -1;
        }
      }
    }
      
    // Process MessageManager for proactive scheduling
    // Dispatch direct, bypassing triggerBehaviour's local-quote/AI pipeline
    messageManager.update(millis());
    while (true) {
      MessageManager::DueMessage msg = messageManager.getNextDueMessage();
      if (msg.eventType == -1) break;
      xSemaphoreTake(geminiMutex, portMAX_DELAY);
      lastTriggeredEventType = msg.eventType;
      aiResponse = msg.content;
      lastResponseIsAi = false;
      hasNewAIResponse = true;
      xSemaphoreGive(geminiMutex);
    }
      
    // Trigger Stretch alert after 45 minutes of continuous presence
    if (now - lastStretchReminderTime > STRETCH_INTERVAL_MS) {
      triggerBehaviour(EVENT_STRETCH);
      lastStretchReminderTime = now;
    }

    // Trigger Slacker Roast if sitting > 1 hour and score < 35%
    static unsigned long lastSlackerRoastTime = 0;
    unsigned long continuousSittingTime = now - continuousPresenceStart;
    if (continuousSittingTime > SLACKER_INTERVAL_MS && productivityScore < 35) {
      if (now - lastSlackerRoastTime > SLACKER_INTERVAL_MS) {
        triggerBehaviour(EVENT_SLACKER);
        lastSlackerRoastTime = now;
      }
    }

    // Evaluate and update longest sitting streak (minimum 15 minutes)
    unsigned long currentStreak = now - continuousPresenceStart;
    if (longestSittingStreak >= STREAK_MINIMUM_MS && currentStreak > longestSittingStreak && !streakAlertTriggered) {
      streakAlertTriggered = true;
      triggerBehaviour(EVENT_STREAK_BEATEN, formatTime(longestSittingStreak));
    }
    if (currentStreak > longestSittingStreak && currentStreak >= STREAK_MINIMUM_MS) {
      longestSittingStreak = currentStreak;
    }
  } else {
    if (currentPresenceState != STATE_AWAY) {
      // Transition: Present -> Away
      streakAlertTriggered = false;
      
      if (rolloverPending) {
        rolloverPending = false;
        // Quick sit under validation buffer: ignore presence metrics updates, keep old lastAwayEpoch
        currentPresenceState = STATE_AWAY;
        lastStateTransitionTime = now;
      } else {
        unsigned long focusSessionDuration = 0;
        if (currentPresenceState == STATE_FOCUS) {
          focusSessionDuration = now - continuousStillStart;
        }
        
        // Trigger Focus session congrats if user focused for > 15s
        if (focusSessionDuration > FOCUS_MINIMUM_MS) {
          triggerBehaviour(EVENT_FOCUS_END, formatTime(focusSessionDuration));
        }
        
        unsigned long presenceDurationMs = now - sitDownTime;
        if (isStopByTracking && presenceDurationMs < STOP_BY_THRESHOLD_MS) { // 8 minutes threshold
          // This was a STOP-BY!
          if (breakCount > 0) breakCount--;
          latestBreakDuration = previousLatestBreakDuration;
          
          if (totalDeskTime >= presenceDurationMs) {
            totalDeskTime -= presenceDurationMs;
          }
          if (sessionDeskTime >= presenceDurationMs) {
            sessionDeskTime -= presenceDurationMs;
          }
          
          totalStopByTimeMs += presenceDurationMs;
          lastAwayEpoch = originalLastAwayEpoch;
          
          currentPresenceState = STATE_AWAY;
          lastStateTransitionTime = now;
          saveDailyStats();
        } else {
          // This was a REAL presence session (>= 8 minutes) or we weren't tracking stop-bys
          currentPresenceState = STATE_AWAY;
          lastStateTransitionTime = now;
          lastAwayEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
          isStopByTracking = false;
          totalStopByTimeMs = 0;
          originalLastAwayEpoch = lastAwayEpoch; // Save start of this new break session
          saveDailyStats();
        }
      }
    } else {
      // Accumulate break time
      totalBreakTime += elapsed;
    }
    
    // Clear filters and reset values when user is AWAY
    rawDetectionDist = 0;
    filteredDetectionDist = 0.0;
    sessionDistanceSum = 0;
    sessionDistanceCount = 0;
    sessionDistanceAverage = 0.0;
    detectionDistFilter.clear();
    motionFilter.clear();
  }

  // Update dynamic productivity score
  uint32_t currentEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
  uint32_t workdayElapsed = (firstSitEpoch > 0 && currentEpoch >= firstSitEpoch) ? (currentEpoch - firstSitEpoch) : 0;

  if (firstSitToday || workdayElapsed < SCORE_INITIAL_PERIOD_S) {
    // Default to 100% initially (first 5 minutes of work)
    productivityScore = 100;
  } else {
    float hoursElapsed = (float)workdayElapsed / 3600.0f;
    
    // 1. Break frequency penalty (target: 1 break/hour = 25% penalty)
    float penalty_breaks = 25.0f * ((float)breakCount / hoursElapsed);
    
    // 2. Break duration penalty (target: 10% of workday in breaks = 25% penalty)
    unsigned long activeBreakMs = 0;
    unsigned long workdayElapsedMs = (unsigned long)workdayElapsed * 1000;
    if (workdayElapsedMs > totalDeskTime) {
      activeBreakMs = workdayElapsedMs - totalDeskTime;
    }
    float breakTimeRatio = (float)(activeBreakMs / 1000.0f) / (float)workdayElapsed;
    float penalty_time = 25.0f * (breakTimeRatio / BREAK_TIME_TARGET);
    
    // 3. Focus bonus (Focus counts 1.5x)
    float bonus_focus = 0.0f;
    if (totalDeskTime > 0) {
      bonus_focus = FOCUS_BONUS_MULTIPLIER * (((float)totalFocusTime * 100.0f) / (float)totalDeskTime);
    }
    
    float raw_score = 100.0f - penalty_breaks - penalty_time + bonus_focus;
    productivityScore = (int)constrain(raw_score, 0.0f, 100.0f);
  }

  // Handle NTP Time & Weather Fetch Updates (every 1 hour or until initial NTP sync succeeds)
  unsigned long ntpUpdateInterval = timeClient.isTimeSet() ? NTP_INTERVAL_MS : NTP_RETRY_MS;
  if (WiFi.status() == WL_CONNECTED && now - lastHourlyUpdate > ntpUpdateInterval) {
    timeClient.update();
    HTTPClient http;
    http.begin(String(OpenWeatherCall) + OpenWeatherKey);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, payload);
      if (!error) {
        temp = (float)(jsonBuffer["main"]["temp"]);
        if (jsonBuffer["weather"].is<JsonArray>() && jsonBuffer["weather"].as<JsonArray>().size() > 0) {
          weatherDesc = jsonBuffer["weather"][0]["main"].as<String>();
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
  static String lastSavedUserName = "";

  if (!statsInit) {
    lastSavedDeskTime = totalDeskTime;
    lastSavedFocusTime = totalFocusTime;
    lastSavedBreakTime = totalBreakTime;
    lastSavedBreakCount = breakCount;
    lastSavedFirstSitEpoch = firstSitEpoch;
    lastSavedLongestStreak = longestSittingStreak;
    lastSavedUserName = userName;
    statsInit = true;
  }

  if (now - lastStatsSave > SAVE_INTERVAL_MS) { // Periodically save stats to LittleFS (every 10 minutes)
    lastStatsSave = now;
    if (totalDeskTime != lastSavedDeskTime || 
        totalFocusTime != lastSavedFocusTime || 
        totalBreakTime != lastSavedBreakTime || 
        breakCount != lastSavedBreakCount ||
        firstSitEpoch != lastSavedFirstSitEpoch ||
        longestSittingStreak != lastSavedLongestStreak ||
        userName != lastSavedUserName) {
      saveDailyStats();
      lastSavedDeskTime = totalDeskTime;
      lastSavedFocusTime = totalFocusTime;
      lastSavedBreakTime = totalBreakTime;
      lastSavedBreakCount = breakCount;
      lastSavedFirstSitEpoch = firstSitEpoch;
      lastSavedLongestStreak = longestSittingStreak;
      lastSavedUserName = userName;
    }
  }

  // Lunch Reminder check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentHour = ts.tm_hour;
    int currentMin = ts.tm_min;
    int learnedLunch = getLearnedLunchHour();
    if (currentHour == learnedLunch && currentMin >= 15) {
      if (currentPresenceState != STATE_AWAY && !lunchReminderTriggered && totalDeskTime > LUNCH_MIN_DESK_MS) {
        lunchReminderTriggered = true;
        saveDailyStats();
        triggerBehaviour(EVENT_LUNCH_REMINDER);
      }
    }
  }

  // Process MQTT service loop
  loopMqtt();

  // Handle Web Server requests
  server.handleClient();

  // Handle OTA updates in the background
  ArduinoOTA.handle();

  // Update TFT Display
  updateTFTDisplay(now);

  delay(LOOP_DELAY_MS);
}