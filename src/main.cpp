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
#include "Behaviour.h"
#include "../Credentials.h"

// Include Display Subsystem (defines RGBColor structure, image loading, and updateTFTDisplay)
#include "Display.h"

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
ld2410 radar;
WebServer server(80);

// Include Radar Subsystem (defines RollingMedianFilter class and declares filters)
#include "Radar.h"

// Instantiate Rolling Median Filters (declared as extern in Radar.h)
RollingMedianFilter detectionDistFilter(100);
RollingMedianFilter motionFilter(10);
float filteredDetectionDist = 0.0;

// User States
#define STATE_AWAY        0
#define STATE_FOCUS       1
#define STATE_BUSY        2
#define STATE_DISTRACTED  3
#define STATE_REGULAR     4

// Configuration limits
int deskDistanceLimit = 120;
int focusDistanceLimit = 50;
int motionRatioLimit = 15;

// Productivity & Timing Metrics
unsigned long totalDeskTime = 0;
unsigned long totalFocusTime = 0;
unsigned long totalBreakTime = 0;
int breakCount = 0;
int productivityScore = 0;
unsigned long latestBreakDuration = 0;
unsigned long overnightBreakDuration = 0;
uint32_t lastAwayEpoch = 0;

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

// Persistent Preferences & Settings
Preferences preferences;
float targetHours = 8.0;
int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
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
  DynamicJsonDocument doc(768);
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
  doc["userName"] = userName;
  doc["deskDistanceLimit"] = deskDistanceLimit;
  doc["focusDistanceLimit"] = focusDistanceLimit;
  doc["motionRatioLimit"] = motionRatioLimit;
  doc["hasMail"] = hasMail;

  if (serializeJson(doc, file) == 0) {
    file.close();
    return;
  }
  file.close();

  if (LittleFS.exists("/stats.json")) {
    LittleFS.remove("/stats.json");
  }
  LittleFS.rename("/stats.json.tmp", "/stats.json");
}

// Load daily statistics from LittleFS
void loadDailyStats() {
  if (!LittleFS.exists("/stats.json")) {
    return;
  }
  fs::File file = LittleFS.open("/stats.json", "r");
  if (!file) {
    return;
  }
  DynamicJsonDocument doc(768);
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
    if (doc.containsKey("userName")) {
      userName = doc["userName"].as<String>();
    }
    deskDistanceLimit = doc["deskDistanceLimit"] | 120;
    focusDistanceLimit = doc["focusDistanceLimit"] | 50;
    motionRatioLimit = doc["motionRatioLimit"] | 15;
    hasMail = doc["hasMail"] | false;
  }
  file.close();
}

// Include all helper subsystems
#include "Gemini.h"
#include "Web.h"
#include "Faceplates.h"

// Asynchronous WiFi Reconnection Checker
void checkWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
      WiFi.begin(SSID, PASS);
    }
  }
}

void setup(void) {
  // Load persistent configurations
  preferences.begin("deskbuddy", false);
  aiMode = preferences.getInt("aiMode", 1);
  clockFace = preferences.getInt("clockFace", 0);
  targetHours = preferences.getFloat("targetHours", 8.0);
  userName = preferences.getString("userName", "human");
  focusDistanceLimit = preferences.getInt("focusDistLim", 50);
  motionRatioLimit = preferences.getInt("motionRatioLim", 15);
  deskDistanceLimit = preferences.getInt("distLimit", 120);
  filterWindow = preferences.getFloat("filterWindow", 2.0);
  hasMail = preferences.getBool("hasMail", false);
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

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Setup NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  // Setup Web Server Subsystem
  setupWebServer();

  // Setup Mutex for Gemini Thread Safety
  geminiMutex = xSemaphoreCreateMutex();

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
  lastHourlyUpdate = millis() - 3600000 - 1000;

  // Ensure splash screen displays for at least 4 seconds total at boot
  unsigned long elapsedBoot = millis() - bootStartTime;
  if (elapsedBoot < 4000) {
    delay(4000 - elapsedBoot);
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

  // Midnight Reset Check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentDay = timeClient.getDay();
    if (lastNtpDay == -1) {
      lastNtpDay = currentDay;
    } else if (currentDay != lastNtpDay) {
      lastNtpDay = currentDay;
      firstSitToday = true;
      firstSitEpoch = 0;
      breakCount = 0;
      totalDeskTime = 0;
      totalFocusTime = 0;
      totalBreakTime = 0;
      overnightBreakDuration = 0;
      lastAwayEpoch = 0;
      dailyAiRequestCount = 0;
      longestSittingStreak = 0;
      latestBreakDuration = 0;
      totalMotionTime = 0;
      motionCount = 0;
      sessionDeskTime = 0;
      sessionMotionTime = 0;
      sessionDistanceSum = 0;
      sessionDistanceCount = 0;
      sessionDistanceAverage = 0.0;
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
    if (now - lastFilterUpdate >= 100) {
      lastFilterUpdate = now;
      
      // Filter motion detection
      motionFilter.add(radar.movingTargetDetected() ? 1.0f : 0.0f);
      filteredMovingTarget = (motionFilter.getMedian(10) > 0.5f);

      if (rawDetectionDist > 0) {
        detectionDistFilter.add((float)rawDetectionDist);
        // Accumulate session distance stats
        sessionDistanceSum += rawDetectionDist;
        sessionDistanceCount++;
        sessionDistanceAverage = (float)sessionDistanceSum / sessionDistanceCount;
      }
      int samples = (int)(filterWindow * 10.0f);
      if (samples < 1) samples = 1;
      if (samples > 100) samples = 100;
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
    unsigned long debounceLimit = rawPresent ? 2000 : 10000; // 2s to confirm presence, 10s to confirm away
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
      unsigned long breakDuration = now - lastStateTransitionTime;
      
      if (firstSitToday) {
        firstSitToday = false;
        firstSitEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
        if (lastAwayEpoch > 0 && firstSitEpoch >= lastAwayEpoch) {
          overnightBreakDuration = firstSitEpoch - lastAwayEpoch;
        } else {
          overnightBreakDuration = 0;
        }
        
        // Save immediately before behavior logic to prevent data loss
        saveDailyStats();
        
        if (overnightBreakDuration >= 4 * 3600UL) {
          triggerBehaviour(EVENT_FIRST_SIT, formatTime(overnightBreakDuration * 1000));
        } else {
          triggerBehaviour(EVENT_FIRST_SIT, "");
        }
        
        // Reset session metrics on first sit
        sessionDeskTime = 0;
        sessionMotionTime = 0;
        sessionDistanceSum = 0;
        sessionDistanceCount = 0;
        sessionDistanceAverage = 0.0;
      } else if (breakDuration >= 180000UL) { // Only count break if away > 3 minutes
        breakCount++;
        latestBreakDuration = breakDuration;
        
        // Save immediately before behavior logic to prevent data loss
        saveDailyStats();
        
        triggerBehaviour(EVENT_WELCOME_BACK, formatTime(breakDuration));
        
        // Reset session metrics on true break return
        sessionDeskTime = 0;
        sessionMotionTime = 0;
        sessionDistanceSum = 0;
        sessionDistanceCount = 0;
        sessionDistanceAverage = 0.0;
      }
      
      currentPresenceState = targetState;
      lastStateTransitionTime = now;
      continuousPresenceStart = now;
      lastStretchReminderTime = now;
      if (targetState == STATE_FOCUS) {
        continuousStillStart = now;
      }
    } else {
      // If we just entered focus state, record start time
      if (targetState == STATE_FOCUS && currentPresenceState != STATE_FOCUS) {
        continuousStillStart = now;
      }
      currentPresenceState = targetState;
    }
      
    // Trigger Stretch alert after 45 minutes of continuous presence
    if (now - lastStretchReminderTime > 2700000UL) {
      triggerBehaviour(EVENT_STRETCH);
      lastStretchReminderTime = now;
    }

    // Trigger Slacker Roast if sitting > 1 hour and score < 35%
    static unsigned long lastSlackerRoastTime = 0;
    unsigned long continuousSittingTime = now - continuousPresenceStart;
    if (continuousSittingTime > 3600000UL && productivityScore < 35) {
      if (now - lastSlackerRoastTime > 3600000UL) {
        triggerBehaviour(EVENT_SLACKER);
        lastSlackerRoastTime = now;
      }
    }

    // Evaluate and update longest sitting streak
    unsigned long currentStreak = now - continuousPresenceStart;
    if (longestSittingStreak >= 60000UL && currentStreak > longestSittingStreak && !streakAlertTriggered) {
      streakAlertTriggered = true;
      triggerBehaviour(EVENT_STREAK_BEATEN, formatTime(longestSittingStreak));
    }
    if (currentStreak > longestSittingStreak && currentStreak >= 60000UL) {
      longestSittingStreak = currentStreak;
    }
  } else {
    if (currentPresenceState != STATE_AWAY) {
      // Transition: Present -> Away
      streakAlertTriggered = false;
      unsigned long focusSessionDuration = 0;
      if (currentPresenceState == STATE_FOCUS) {
        focusSessionDuration = now - continuousStillStart;
      }
      
      // Trigger Focus session congrats if user focused for > 15s
      if (focusSessionDuration > 15000) {
        triggerBehaviour(EVENT_FOCUS_END, formatTime(focusSessionDuration));
      }
      
      currentPresenceState = STATE_AWAY;
      lastStateTransitionTime = now;
      lastAwayEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
      saveDailyStats();
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

  if (firstSitToday || workdayElapsed < 300) {
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
    float penalty_time = 25.0f * (breakTimeRatio / 0.10f);
    
    // 3. Focus bonus (Focus counts 1.5x)
    float bonus_focus = 0.0f;
    if (totalDeskTime > 0) {
      bonus_focus = 1.5f * (((float)totalFocusTime * 100.0f) / (float)totalDeskTime);
    }
    
    float raw_score = 100.0f - penalty_breaks - penalty_time + bonus_focus;
    productivityScore = (int)constrain(raw_score, 0.0f, 100.0f);
  }

  // Handle NTP Time & Weather Fetch Updates (every 1 hour)
  if (WiFi.status() == WL_CONNECTED && now - lastHourlyUpdate > 3600000) {
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
        rawtime = rawtime - 10800;
        ts = *localtime(&rawtime);
        strftime(buf, sizeof(buf), "%a %d-%m", &ts);
      }
    }
    http.end();
    lastHourlyUpdate = now;
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

  if (now - lastStatsSave > 60000) {
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

  // Update TFT Display
  updateTFTDisplay(now);

  delay(10);
}