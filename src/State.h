#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include "Constants.h"

// RGB Color structure
#ifndef RGB_COLOR_STRUCT
#define RGB_COLOR_STRUCT
struct RGBColor {
  uint8_t r, g, b;
  bool operator==(const RGBColor& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGBColor& o) const { return !(*this == o); }
};
#endif

// MQTT Message structure
#ifndef MQTT_MESSAGE_STRUCT
#define MQTT_MESSAGE_STRUCT
struct MqttMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};
#endif

struct ConfigState {
  float targetHours = 8.0;
  int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
  int aiPersona = 0; // 0 = Coach, 1 = Critic, 2 = Nerd, 3 = Zen
  int clockFace = 0;
  String userName = "human";
  int focusDistanceLimit = FOCUS_DISTANCE_LIMIT_DEFAULT;
  int motionRatioLimit = MOTION_RATIO_LIMIT_DEFAULT;
  int deskDistanceLimit = DISTANCE_LIMIT_DEFAULT;
  float filterWindow = 2.0;
  bool hasMail = false;
  bool time24h = true;

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
  int lastNtpDay = -1;
  int lastMidnightCheckDay = -1;
  unsigned long previousLatestBreakDuration = 0;
  uint32_t fsWriteCount = 0;
  uint32_t fsReadCount = 0;
};

struct RuntimeState {
  int currentPresenceState = 0; // STATE_AWAY (defined in Behaviour/main)
  float filteredDetectionDist = 0.0;
  unsigned long currentBreakDurationMs = 0;
  bool isStopByTracking = false;
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
  
  SemaphoreHandle_t geminiMutex = NULL;
  
  // MQTT History Buffer
  MqttMessage mqttHistory[MQTT_HISTORY_SIZE];
  int mqttHistoryHead = 0;
  int mqttHistoryCount = 0;
  SemaphoreHandle_t mqttHistoryMutex = NULL;
};

struct TodoState {
  String rawJson = "{\"daily\":[],\"monthly\":[]}";
};

// Global State extern declarations (instantiated in main.cpp)
extern ConfigState appConfig;
extern StatsState appStats;
extern RuntimeState appState;
extern TodoState appTodo;

#endif // STATE_H
