#ifndef CONSTANTS_H
#define CONSTANTS_H

#ifndef DESKBUDDY_VERSION
#define DESKBUDDY_VERSION "0.0.0"
#endif
#ifndef TELEMETRY_ENDPOINT_DEFAULT
#define TELEMETRY_ENDPOINT_DEFAULT ""
#endif

// --- Telemetry Constants ---
#define TELEMETRY_SEND_INTERVAL_MS  3600000UL  // 1 hour between telemetry reports
#define TELEMETRY_FW_CHECK_INTERVAL_MS 21600000UL // 6 hours between firmware checks (also checked on each telemetry send)

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
#define STRETCH_INTERVAL_MS     3600000UL   // 60 min between stretch reminders
#define SLACKER_INTERVAL_MS     4500000UL   // 1h15m between slacker roasts
#define STREAK_MINIMUM_MS        900000UL   // 15 min minimum for streak tracking
#define FOCUS_MINIMUM_MS          300000UL  // 5 min minimum focus session
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
#define LATEHOURS_PADDING_MS       1800000UL // 30 min padding added to the learned workday times: late hours run from learned end +30 min until learned start -30 min
#define LATEHOURS_COOLDOWN_MS     1800000UL // 30 min minimum gap between late-hours sit messages
#define CROSSOVER_THRESHOLD_MS      900000UL // 15 min continuous sit during late hours = real session, burn flag early
#define DISPLAY_THROTTLE_MS           500UL // 500ms minimum between display faceplate redraws
#define DEV_REFRESH_MS                200UL // 200ms refresh for dev faceplate
#define BEACON_PORT                    42042  // UDP beacon port for companion app discovery
#define BEACON_INTERVAL_MS             30000UL // 30s between beacon broadcasts
#define LOOP_DELAY_MS                  10UL // 10ms main loop delay

// --- Audio Constants ---
#define AUDIO_PIN                         21

#define METRIC_CYCLE_MS             15000UL // 15s cycle for dashboard bottom metric
#define SCORE_INITIAL_PERIOD_S        300   // 300s (5 min): productivity = 100% initially
#define DISTRACTED_FAR_MIN_MS      300000UL // 5 min: present-but-far before the relaxed (Distracted) mood fires

// --- Performance & Productivity Constants ---
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
#define EXCESSIVE_BREAKS_MIN_WORKED_HOURS 3.0f  // minimum hours worked before the excessive-breaks roast can fire
#define NTP_TIME_OFFSET               -10800   // Argentina time (UTC-3)

// --- Radar Constants ---
#define RADAR_CM_PER_GATE                  20
#define RADAR_MIN_GATES                     2
#define RADAR_MAX_GATES                     8

// --- Filter Sizes ---
#define DIST_FILTER_SIZE                  100
#define MOTION_FILTER_SIZE                 10

// --- Message Queue Constants ---
// Priority ranks (sort keys only — not durations).
#define MSG_PRIORITY_URGENT               3000UL
#define MSG_PRIORITY_HIGH                 2250UL
#define MSG_PRIORITY_NORMAL               1500UL
#define MSG_PRIORITY_LOW                   500UL

// Relevance = TTL after scheduleTime (how long a queued msg stays valid).
#define MSG_RELEVANCE_SHORT             300000UL  // 5 min  — brief / time-critical
#define MSG_RELEVANCE_NORMAL           1800000UL  // 30 min — default
#define MSG_RELEVANCE_LONG             3600000UL  // 1 hr   — important, keep until return

// Max chars a standard message screen holds before it spills to a 2nd screen (F12)
#define MSG_PAGE_MAX_CHARS                 110

// --- Journal & Curation Constants ---
#define MORNING_JOURNAL_DELAY_MS        300000UL  // 5 minutes sitting delay for morning kickoff
#define PRE_LUNCH_JOURNAL_MINS_BEFORE        15   // Minutes before lunch to trigger pre-lunch journal
#define END_OF_DAY_JOURNAL_HOURS_BEFORE       1   // Hours before workday end to trigger end-of-day journal
#define MIDDAY_TASK_CHECK_HOUR               12   // 12:00 PM midday threshold for task check observations
#define NAGGING_TRIGGER_DELAY_MS       3600000UL  // Base 60 min cadence for the overdue-task nag queue
#define NAGGING_MIN_INTERVAL_MS         900000UL  // 15 min minimum delay ceiling/floor (Normal)
#define POINTS_TRIGGER_DELAY_MS       1080000UL  // 18 min cadence for the seated points check-in (first ring 18m into a session, then every 18m while seated)
#define POINTS_THROTTLE_MS           13260000UL  // 221 min cooldown between points check-ins (~3 per 10h day)

// Chatty mode (aiMode == 2): increased interval cadences
#define CHATTY_NAGGING_TRIGGER_DELAY_MS 2220000UL  // Base 37 min cadence (down from 60 min)
#define CHATTY_NAGGING_MIN_INTERVAL_MS   480000UL  // 8 min minimum delay ceiling/floor (Chatty)
#define CHATTY_POINTS_TRIGGER_DELAY_MS    540000UL  // 9 min (down from 18 min)
#define CHATTY_POINTS_THROTTLE_MS       5400000UL  // 90 min (down from 221 min)

// Curation nudge (aiMode >= 1): observation-driven trigger
#define CURATION_TRIGGER_INTERVAL_MS    3000000UL  // 50 min continuous sitting (Normal)
#define CURATION_THROTTLE_MS           7200000UL  // 120 min cooldown (Normal)
#define CHATTY_CURATION_TRIGGER_INTERVAL_MS 2400000UL  // 40 min (Chatty)
#define CHATTY_CURATION_THROTTLE_MS         3600000UL  // 60 min cooldown (Chatty)
#define TASK_OVERDUE_DAYS_LIMIT               3   // Overdue limit in days for daily tasks
#define TASK_OVERDUE_MONTHS_LIMIT             3   // Overdue limit in months for monthly tasks
#define TASK_SYNTHESIS_MAX_CHARS            500   // Max length of the compact task synthesis injected into AI observations
#define TASK_LIST_MAX_PAGE_LINES              8   // Total lines per page (1 title + 6 items). Lower for larger fonts.

// --- MQTT Service Constants ---
#define MQTT_BROKER_IP             "192.168.15.18"
#define MQTT_BROKER_PORT                  1883
#define MQTT_RECONNECT_INTERVAL_MS       10000UL
#define MQTT_CLIENT_ID          "DeskBuddyClient"
#define MQTT_STATUS_TOPIC       "deskbuddy/status"
#define MQTT_STATUS_PAYLOAD              "online"
#define MQTT_SUBSCRIBE_TOPIC         "deskbuddy/#"
#define MQTT_ECHO_TOPIC        "deskbuddy/echo"

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

#ifndef ELEMENT_CONFIG_STRUCT
#define ELEMENT_CONFIG_STRUCT
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct ElementConfig {
  const char* font;
  uint16_t color;
  int x;
  int y;
  int clearX;
  int clearY;
  int clearW;
  int clearH;
};
#endif

// Structured Event View Data Models
struct TaskDueViewData {
  String headerText;  // "TASK DUE" or "OVERDUE"
  String taskText;    // Task title
  String dueTimeStr;  // "02:30 PM"
  bool isOverdue;
};

struct JournalDashboardViewData {
  String titleStr;
  int dueTodayCount;
  int dailyCount;
  int monthlyCount;
  int diligenceScore;
};

JournalDashboardViewData getJournalDashboardData();

#endif // CONSTANTS_H
