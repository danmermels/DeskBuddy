#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include <NTPClient.h>
#include "Constants.h"

extern NTPClient timeClient;

// RGB Color structure
#ifndef RGB_COLOR_STRUCT
#define RGB_COLOR_STRUCT
struct RGBColor {
  uint8_t r, g, b;
  bool operator==(const RGBColor& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGBColor& o) const { return !(*this == o); }
};
#endif

// System Log structure
#ifndef LOG_ENTRY_STRUCT
#define LOG_ENTRY_STRUCT
struct LogEntry {
  uint32_t timestamp; // NTP Epoch or millis()
  char category[16];
  char message[128];
};
#endif


struct ConfigState {
  float targetHours = 8.0;
  int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
  int aiPersona = 0; // 0 = Coach, 1 = Critic, 2 = Sweet, 3 = Friend
  int clockFace = 0;
  String userName = "human";
  int focusDistanceLimit = FOCUS_DISTANCE_LIMIT_DEFAULT;
  int motionRatioLimit = MOTION_RATIO_LIMIT_DEFAULT;
  int deskDistanceLimit = DISTANCE_LIMIT_DEFAULT;
  float filterWindow = 2.0;
  bool hasMail = false;
  bool time24h = true;
  int buddyFontIndex = 0; // 0 = GoodTiming20, 1 = GoodTiming15 (or other 2nd font)

  // Radar Gate Sensitivities
  int g0mSens = 90;
  int g0sSens = 90;
  int g1mSens = 60;
  int g1sSens = 40;
  int g2mSens = 50;
  int g2sSens = 40;
  int g3mSens = 40;
  int g3sSens = 40;
  int g4mSens = 45;
  int g4sSens = 40;
  int g5mSens = 50;
  int g5sSens = 40;
  int g6mSens = 50;
  int g6sSens = 40;

  // WiFi credentials
  String wifiSsid = "";
  String wifiPass = "";
  bool wifiStaticEnabled = true;
  String wifiIp = "192.168.15.160";
  String wifiGw = "192.168.15.1";
  String wifiSubnet = "255.255.255.0";
  String wifiDns1 = "1.1.1.1";
  String wifiDns2 = "8.8.8.8";

  // MQTT broker
  String mqttBroker = "192.168.15.18";
  int mqttPort = 1883;

  // API keys
  String groqApiKey = "";
  String geminiApiKey = "";
  String deepseekApiKey = "";
  String openWeatherKey = "";
  float openWeatherLat = -23.11;
  float openWeatherLon = -46.53;
};

struct StatsState {
  bool firstSitToday = true;
  uint32_t firstSitEpoch = 0;
  int breakCount = 0;
  unsigned long totalDeskTime = 0;
  unsigned long totalFocusTime = 0;
  unsigned long totalBreakTime = 0;
  unsigned long overnightBreakDuration = 0;
  uint32_t lastAwayEpoch = 0;
  int dailyAiRequestCount = 0;
  unsigned long longestSittingStreak = 0;
  unsigned long latestBreakDuration = 0;
  unsigned long totalMotionTime = 0;
  unsigned long motionCount = 0;
  int productivityScore = 0;
  
  uint8_t hourlyPresenceHistoryWeekly[7][24] = {0};
  uint32_t presenceMsCurrentDay[24] = {0};
  int historyDaysCountWeekly[7] = {0};
  
  bool lunchReminderTriggered = false;
  bool excessiveBreaksTriggered = false;
  bool goalCompletedTriggered = false;
  bool morningJournalTriggered = false;
  bool preLunchJournalTriggered = false;
  bool endOfDayJournalTriggered = false;
  bool naggingTriggeredToday = false;
  String dueFiredDay = "";
  String dueFiredKeys = "";
  int lastNtpDay = -1;
  int lastMidnightCheckDay = -1;
  unsigned long previousLatestBreakDuration = 0;
  uint32_t fsWriteCount = 0;
  uint32_t fsReadCount = 0;
  uint32_t fsWritesToday = 0;
};

struct RuntimeState {
  int currentPresenceState = 0; // STATE_AWAY (defined in Behaviour/main)
  float filteredDetectionDist = 0.0;
  unsigned long currentBreakDurationMs = 0;
  bool isStopByTracking = false;
  bool wasFirstSitThisSession = false;
  uint32_t originalLastAwayEpoch = 0;
  unsigned long totalStopByTimeMs = 0;
  unsigned long lastStateTransitionTime = 0;
  unsigned long lastLoopTime = 0;
  unsigned long continuousPresenceStart = 0;
  unsigned long continuousStillStart = 0;
  unsigned long lastStretchReminderTime = 0;
  volatile bool otaInProgress = false;
  bool streakAlertTriggered = false;
  
  unsigned long sitDownTime = 0;
  uint32_t sitDownEpoch = 0;
  bool rolloverPending = false;
  unsigned long requiredValidationBufferMs = 180000UL;

  int rawDetectionDist = 0;
  bool sensorPresenceDetected = false;
  bool sensorMovingTargetDetected = false;
  bool sensorStaticPresenceDetected = false;

  unsigned long sessionDeskTime = 0;
  unsigned long sessionMotionTime = 0;
  unsigned long sessionDistanceSum = 0;
  unsigned long sessionDistanceCount = 0;
  float sessionDistanceAverage = 0.0;

  int lastTriggeredEventType = 0; // EVENT_FIRST_SIT
  
  volatile bool isAILoading = false;
  unsigned long lastAiQueryStartTime = 0;
  volatile bool mqttConnected = false;
  volatile bool pendingWelcomeAlert = false;
  volatile bool manualTriggerOverride = false;
  String aiResponse = "";
  volatile bool hasNewAIResponse = false;
  volatile bool lastResponseIsAi = false;
  String currentPrompt = "";
  String lastTriggeredEventDetail = "";
  String currentUserName = "human";
  int temp = 21;
  String weatherDesc = "Clear";
  
  unsigned long aiScreenEndTime = 0;
  RGBColor currentRingColor = {80, 80, 80};
  RGBColor startRingColor = {80, 80, 80};
  RGBColor targetRingColor = {80, 80, 80};
  unsigned long ringTransitionStart = 0;
  unsigned long ringTransitionDuration = RING_TRANSITION_MS;
  
  SemaphoreHandle_t aiMutex = NULL;
  
  // Debug Simulation
  bool simulationMode = false;
  bool simulationContinuous = false;
  int simulatedDistance = 0;
  bool simulatedMoving = false;
  bool simulatedPresent = false;
  int simulatedStateOverride = -1;
  unsigned long simulatedEpoch = 0;

  // Captive Portal
  bool captivePortalMode = false;

  // Web activity safety tracker
  unsigned long lastWebActivityTime = 0;


};

struct TodoState {
  String rawJson = "{\"daily\":[],\"monthly\":[]}";
};

struct TftMessageRecord {
  uint32_t epoch;
  char text[96];
  uint8_t eventType;
  bool isAi;
};

struct TftMessageHistory {
  static constexpr int MAX_MSGS = MAX_TFT_MSGS;
  TftMessageRecord buffer[MAX_MSGS];
  int head = 0;
  int count = 0;

  void record(const char* text, uint8_t eventType, bool isAi) {
    buffer[head].epoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : (millis() / 1000);
    strncpy(buffer[head].text, text, sizeof(buffer[head].text) - 1);
    buffer[head].text[sizeof(buffer[head].text) - 1] = '\0';
    buffer[head].eventType = eventType;
    buffer[head].isAi = isAi;
    head = (head + 1) % MAX_MSGS;
    if (count < MAX_MSGS) count++;
  }
};

// Global State extern declarations (instantiated in main.cpp)
extern ConfigState appConfig;
extern StatsState appStats;
extern RuntimeState appState;
extern TodoState appTodo;
extern TftMessageHistory tftMsgHistory;

#endif // STATE_H
