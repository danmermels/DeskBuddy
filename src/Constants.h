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
#define DEBOUNCE_AWAY_MS         10000UL
#define BREAK_MINIMUM_MS         180000UL   // 3 min minimum for break counting
#define STOP_BY_THRESHOLD_MS     480000UL   // 8 min threshold for stop-by detection
#define STRETCH_INTERVAL_MS     2700000UL   // 45 min between stretch reminders
#define SLACKER_INTERVAL_MS     3600000UL   // 1 hr between slacker roasts
#define STREAK_MINIMUM_MS        900000UL   // 15 min minimum for streak tracking
#define FOCUS_MINIMUM_MS           15000UL  // 15s minimum focus session
#define WELCOME_DELAY_MS           10000UL  // 10s delay before welcome alert on sit-down
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
#define STICKY_CONFIRM_MS          180000UL // 3 min sticky state confirmation window
#define DISPLAY_THROTTLE_MS           500UL // 500ms minimum between display faceplate redraws
#define DEV_REFRESH_MS                200UL // 200ms refresh for dev faceplate
#define LOOP_DELAY_MS                  10UL // 10ms main loop delay
#define METRIC_CYCLE_MS             15000UL // 15s cycle for dashboard bottom metric
#define SCORE_INITIAL_PERIOD_S        300   // 300s (5 min): productivity = 100% initially

// --- Performance & Productivity Constants ---
#define DAILY_AI_LIMIT                    15
#define FILTER_MOTION_THRESHOLD          0.5f
#define BREAK_PENALTY_TARGET              1.0f   // target: 1 break/hour = 25% penalty
#define BREAK_TIME_TARGET                 0.10f  // target: 10% of workday in breaks = 25% penalty
#define FOCUS_BONUS_MULTIPLIER            1.5f
#define TARGET_HOURS_DEFAULT              8.0f
#define DISTANCE_LIMIT_DEFAULT            120
#define FOCUS_DISTANCE_LIMIT_DEFAULT       50
#define MOTION_RATIO_LIMIT_DEFAULT         15
#define FILTER_WINDOW_DEFAULT             2.0f
#define OVERNIGHT_THRESHOLD_S            14400UL  // 4 hours - overnight/sleep threshold for first-sit greeting
#define G0S_SENS_DEFAULT                   90
#define LUNCH_MIN_DESK_MS            1800000UL  // 30 min minimum desk time before lunch reminder
#define LUNCH_REMINDER_DELAY_MS      3600000UL  // 1 hour delay for lunch reminder message
#define NTP_TIME_OFFSET               -10800   // Argentina time (UTC-3)

// --- Radar Constants ---
#define RADAR_CM_PER_GATE                  20
#define RADAR_MIN_GATES                     2
#define RADAR_MAX_GATES                     8

// --- Filter Sizes ---
#define DIST_FILTER_SIZE                  100
#define MOTION_FILTER_SIZE                 10

// --- Message Queue Constants ---
#define MSG_PRIORITY_HIGH                 3000UL
#define MSG_PRIORITY_NORMAL               1500UL
#define MSG_PRIORITY_LOW                   500UL

#define MSG_RELEVANCE_URGENT            300000UL  // 5 min
#define MSG_RELEVANCE_NORMAL           1800000UL  // 30 min
#define MSG_RELEVANCE_LOW              3600000UL  // 1 hr

// --- Journal & Curation Constants ---
#define MORNING_JOURNAL_DELAY_MS        300000UL  // 5 minutes sitting delay for morning kickoff
#define PRE_LUNCH_JOURNAL_MINS_BEFORE        15   // Minutes before lunch to trigger pre-lunch journal
#define END_OF_DAY_JOURNAL_HOURS_BEFORE       1   // Hours before workday end to trigger end-of-day journal
#define MIDDAY_TASK_CHECK_HOUR               12   // 12:00 PM midday threshold for task check observations
#define NAGGING_TRIGGER_DELAY_MS       7200000UL  // 2 hours sitting delay for nagging trigger
#define TASK_OVERDUE_DAYS_LIMIT               3   // Overdue limit in days for daily tasks
#define TASK_OVERDUE_MONTHS_LIMIT             3   // Overdue limit in months for monthly tasks

// --- MQTT Service Constants ---
#define MQTT_BROKER_IP             "192.168.15.18"
#define MQTT_BROKER_PORT                  1883
#define MQTT_RECONNECT_INTERVAL_MS       10000UL
#define MQTT_CLIENT_ID          "DeskBuddyClient"
#define MQTT_STATUS_TOPIC       "deskbuddy/status"
#define MQTT_STATUS_PAYLOAD              "online"
#define MQTT_SUBSCRIBE_TOPIC         "deskbuddy/#"
#define MQTT_DISPLAY_TOPIC     "deskbuddy/display"
#define MQTT_PUBLISH_TOPIC     "deskbuddy/message"
#define MQTT_ECHO_TOPIC        "deskbuddy/echo"
#define MQTT_HISTORY_SIZE                   50

// --- MQTT Debug Platform Topics ---
#define MQTT_DEBUG_CMD_TOPIC      "deskbuddy/debug/cmd"
#define MQTT_DEBUG_RESP_TOPIC     "deskbuddy/debug/resp"

// --- Captive Portal ---
#define AP_SSID                 "DeskBuddy-Setup"

#endif // CONSTANTS_H
