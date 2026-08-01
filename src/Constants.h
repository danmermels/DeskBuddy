#ifndef CONSTANTS_H
#define CONSTANTS_H

// --- User State Definitions ---
#define STATE_AWAY        0
#define STATE_FOCUS       1
#define STATE_BUSY        2
#define STATE_DISTRACTED  3
#define STATE_REGULAR     4

// --- Timing Constants (milliseconds) ---
#define DEBOUNCE_PRESENCE_MS      2000UL
#define DEBOUNCE_PRESENCE_OVERNIGHT_MS 5000UL
#define DEBOUNCE_AWAY_MS         10000UL
#define BREAK_MINIMUM_MS         180000UL   // 3 min minimum for break counting
#define STOP_BY_THRESHOLD_MS     480000UL   // 8 min threshold for stop-by detection
#define STRETCH_INTERVAL_MS     2700000UL   // 45 min between stretch reminders
#define SLACKER_INTERVAL_MS     3600000UL   // 1 hr between slacker roasts
#define STREAK_MINIMUM_MS        900000UL   // 15 min minimum for streak tracking
#define FOCUS_MINIMUM_MS          300000UL  // 5 min minimum focus session
#define WELCOME_DELAY_MS           3000UL   // 1s delay before welcome alert on sit-down
#define WELCOME_HOLD_MS             5000UL  // 5s grace after sit-down before welcome overlay (lets clock face show first)
#define AWAY_GRACE_MS              60000UL  // 1 min grace period showing clock after away
#define ALERT_DURATION_MS           8000UL  // 8s alert message display
#define SAVE_INTERVAL_MS          600000UL  // 10 min between stats saves
#define NTP_INTERVAL_MS          3600000UL  // 1 hr between NTP/weather updates
#define NTP_RETRY_MS                15000UL // 15s retry for initial NTP sync
#define FILTER_UPDATE_MS              100UL // 100ms between radar filter updates
#define WIFI_CHECK_MS               10000UL // 10s between WiFi reconnection checks
#define BOOT_SPLASH_MS               4000UL // 4s minimum splash screen at boot
#define WIFI_TIMEOUT_MS              5000UL // 5s WiFi connect timeout in setup
#define RING_TRANSITION_MS           1000UL // 1s ring color transition
#define STICKY_CONFIRM_MS           30000UL // 0.5 min sticky state confirmation window
#define DISPLAY_THROTTLE_MS           500UL // 500ms minimum between display faceplate redraws
#define DEV_REFRESH_MS                200UL // 200ms refresh for dev faceplate
#define LOOP_DELAY_MS                  10UL // 10ms main loop delay
#define METRIC_CYCLE_MS             15000UL // 15s cycle for dashboard bottom metric
#define SCORE_INITIAL_PERIOD_S        300   // 300s (5 min): productivity = 100% initially

// --- Performance & Productivity Constants ---
#define DAILY_AI_LIMIT                    30
#define MAX_TFT_MSGS                      10
#define FILTER_MOTION_THRESHOLD          0.5f
#define BREAK_PENALTY_TARGET              1.0f   // target: 1 break/hour = 25% penalty
#define BREAK_TIME_TARGET                 0.10f  // target: 10% of workday in breaks = 25% penalty
#define FOCUS_BONUS_MULTIPLIER            1.5f
#define TARGET_HOURS_DEFAULT              8.0f
#define DISTANCE_LIMIT_DEFAULT            120
#define FOCUS_DISTANCE_LIMIT_DEFAULT       50
#define MOTION_RATIO_LIMIT_DEFAULT         15
#define RECENT_MOTION_WINDOW_S             180  // 180s (3 minutes) rolling motion window for state transitions
#define FILTER_WINDOW_DEFAULT             2.0f
#define OVERNIGHT_THRESHOLD_S            14400UL  // 4 hours - overnight/sleep threshold for first-sit greeting
#define G0S_SENS_DEFAULT                   90
#define LUNCH_MIN_DESK_MS            1800000UL  // 30 min minimum desk time before lunch reminder
#define NTP_TIME_OFFSET               -10800   // Argentina time (UTC-3)

// --- Radar Constants ---
#define RADAR_CM_PER_GATE                  20
#define RADAR_MIN_GATES                     2
#define RADAR_MAX_GATES                     8

// --- Filter Sizes ---
#define DIST_FILTER_SIZE                  100
#define MOTION_FILTER_SIZE                 10

// --- Message Queue Constants ---
#define MSG_PRIORITY_URGENT               3000UL
#define MSG_PRIORITY_HIGH                 2250UL
#define MSG_PRIORITY_NORMAL               1500UL
#define MSG_PRIORITY_LOW                   500UL

#define MSG_RELEVANCE_URGENT            300000UL  // 5 min
#define MSG_RELEVANCE_NORMAL           1800000UL  // 30 min
#define MSG_RELEVANCE_LOW              3600000UL  // 1 hr

// Max chars a standard message screen holds before it spills to a 2nd screen (F12)
#define MSG_PAGE_MAX_CHARS                 110

// --- Journal & Curation Constants ---
#define MORNING_JOURNAL_DELAY_MS        300000UL  // 5 minutes sitting delay for morning kickoff
#define PRE_LUNCH_JOURNAL_MINS_BEFORE        15   // Minutes before lunch to trigger pre-lunch journal
#define END_OF_DAY_JOURNAL_HOURS_BEFORE       1   // Hours before workday end to trigger end-of-day journal
#define MIDDAY_TASK_CHECK_HOUR               12   // 12:00 PM midday threshold for task check observations
#define NAGGING_TRIGGER_DELAY_MS       7200000UL  // 2 hours sitting delay for nagging trigger
#define TASK_OVERDUE_DAYS_LIMIT               3   // Overdue limit in days for daily tasks
#define TASK_OVERDUE_MONTHS_LIMIT             3   // Overdue limit in months for monthly tasks
#define TASK_SYNTHESIS_MAX_CHARS            500   // Max length of the compact task synthesis injected into AI observations

// --- MQTT Service Constants ---
#define MQTT_BROKER_IP             "192.168.15.18"
#define MQTT_BROKER_PORT                  1883
#define MQTT_RECONNECT_INTERVAL_MS       10000UL
#define MQTT_CLIENT_ID          "DeskBuddyClient"
#define MQTT_STATUS_TOPIC       "deskbuddy/status"
#define MQTT_STATUS_PAYLOAD              "online"
#define MQTT_SUBSCRIBE_TOPIC         "deskbuddy/#"
#define MQTT_ECHO_TOPIC        "deskbuddy/echo"

#define SYSTEM_LOG_SIZE                     15

// --- MQTT Debug Platform Topics ---
#define MQTT_DEBUG_CMD_TOPIC      "deskbuddy/debug/cmd"
#define MQTT_DEBUG_RESP_TOPIC     "deskbuddy/debug/resp"

// --- Captive Portal ---
#define AP_SSID                 "DeskBuddy-Setup"

inline unsigned long getAlertDurationMs(int lineCount, bool isPage1 = false) {
  if (isPage1) {
    return 4000UL; // Dashboard summary page lasts exactly 4 seconds
  }
  unsigned long ms = 3000 + (lineCount * 1800);
  if (ms < 5000) ms = 5000;
  if (ms > 10000) ms = 10000; // Capped at 10 seconds
  return ms;
}

struct RGB {
  uint8_t r, g, b;
};

// ============================================================================
// TASK JOURNAL & DUE NOW SCREEN CONFIGURATIONS
// ============================================================================
namespace JournalConfig {
  // --- Page One Overview (Dashboard) ---
  constexpr int pageOneTitleY = 17;
  constexpr const char* pageOneTitleFont = "";          // Empty string "" defaults to small system font
  constexpr RGB pageOneTitleColor = {245, 158, 11}; // Yellow
  constexpr const char* pageOneTaskFont = "";           // Empty string "" defaults to small system font
  constexpr RGB pageOneTaskColor = {255, 255, 255};  // White

  // --- Tasks Page (Journal Page 2+) ---
  constexpr int tasksTitleY = 17;
  constexpr const char* tasksTitleFont = "";            // Empty string "" defaults to small system font
  constexpr RGB tasksTitleColor = {245, 158, 11};   // Yellow
  constexpr const char* tasksTaskFont = "";             // Empty string "" defaults to small system font
  constexpr RGB tasksTaskColor = {255, 255, 255};    // White

  // --- DUE Now Page ---
  constexpr int dueTitleY = 17;
  constexpr const char* dueTitleFont = "";              // Empty string "" defaults to small system font
  constexpr RGB dueTitleColor = {245, 158, 11};     // Yellow
  constexpr RGB dueTextColor = {255, 255, 255};     // White
  
  // Hour / Time Field
  constexpr int dueTimeY = 200;                       // Position Y for the time
  constexpr const char* dueTimeFont = "";             // Font for the time
  constexpr RGB dueTimeColor = {239, 68, 68};       // Red
}

// ============================================================================
// NAGGING ALERT TITLE CONFIGURATION (red "DUE NOW" on top of the nag message)
// ============================================================================
namespace NaggingConfig {
  constexpr const char* titleText = "DUE NOW";        // Text of the title
  constexpr uint8_t titleFont = 2;                    // TFT_eSPI built-in font index (0-7; 2=small, 4=medium)
  constexpr int titleX = 120;                         // X center of the title
  constexpr int titleY = 24;                          // Y center of the title
  constexpr RGB titleColor = {239, 68, 68};         // Red
  constexpr int msgStartY = 55;                       // Y where the AI nag message begins
}

#endif // CONSTANTS_H
