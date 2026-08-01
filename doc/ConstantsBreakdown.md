# Constants.h Breakdown

Every `#define`, function, and struct in `src/Constants.h` with real-world context.

---

## Table of Contents

1. [State Definitions](#1-state-definitions)
2. [Timing Constants](#2-timing-constants)
3. [Performance & Productivity](#3-performance--productivity)
4. [Radar Constants](#4-radar-constants)
5. [Filter Sizes](#5-filter-sizes)
6. [Message Queue Constants](#6-message-queue-constants)
7. [Journal & Curation](#7-journal--curation)
8. [MQTT Constants](#8-mqtt-constants)
9. [Captive Portal](#9-captive-portal)
10. [Utility Functions](#10-utility-functions)
11. [Structs & Namespaces](#11-structs--namespaces)

---

## 1. State Definitions

Integer identifiers for the 5 presence states used throughout the codebase.

```cpp
#define STATE_AWAY        0  // No presence detected, or distance > deskDistanceLimit
#define STATE_FOCUS       1  // In focus zone (< focusDistanceLimit cm) + low motion (< motionRatioLimit%)
#define STATE_BUSY        2  // In focus zone + high motion (>= motionRatioLimit%)
#define STATE_DISTRACTED  3  // Beyond focus zone + high motion
#define STATE_REGULAR     4  // Beyond focus zone + low motion
```

**Where used:** `appState.currentPresenceState`, `getPresenceStateName()`, SIM commands, presence state machine transitions, AI prompt telemetry.

---

## 2. Timing Constants

All values in milliseconds unless noted. Controls debounce windows, trigger intervals, and loop timing.

### Presence Debouncing

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `DEBOUNCE_PRESENCE_MS` | 2000 | 2 seconds | How long radar must detect presence before transitioning from AWAY to PRESENT. Prevents flicker from brief sensor blips. |
| `DEBOUNCE_PRESENCE_OVERNIGHT_MS` | 5000 | 5 seconds | Longer debounce for the first sit-down of the day. Gives the user time to settle in before the welcome alert fires. |
| `DEBOUNCE_AWAY_MS` | 10000 | 10 seconds | How long absence must persist before the session ends and a break is counted. Prevents short walks (bathroom, coffee) from breaking a session. |
| `STICKY_CONFIRM_MS` | 30000 | 30 seconds | State transition lock. If presence state changes (e.g., FOCUS → BUSY), it must stay in the new state for 30s before the transition is confirmed. Prevents rapid toggling. |
| `LATEHOURS_PADDING_MS` | 1800000 | 30 minutes | Padding applied on both sides of the learned workday for late-hours detection. A sit is "late hours" only outside `[start − 30m, end + 30m]`. |

### Session & Break Timing

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `BREAK_MINIMUM_MS` | 180000 | 3 minutes | Minimum absence duration to count as a "break". Shorter absences (bathroom, stretch) don't increment break count. |
| `STOP_BY_THRESHOLD_MS` | 480000 | 8 minutes | If a present session is shorter than 8 minutes and during off-hours, it's treated as a "stop-by" rather than real work. Break count is rolled back. |
| `STREAK_MINIMUM_MS` | 900000 | 15 minutes | Minimum unbroken presence to qualify as a "streak" for the longest-streak tracker. Quick sits don't count. |
| `FOCUS_MINIMUM_MS` | 300000 | 5 minutes | Minimum presence duration in FOCUS state to trigger a focus-end event when the user leaves. Prevents focus events from brief moments. |
| `WELCOME_DELAY_MS` | 3000 | 3 seconds | Delay after first sit-down before showing the welcome/sit-down alert. Gives display time to settle. |

### Behaviour Trigger Intervals

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `STRETCH_INTERVAL_MS` | 3600000 | 60 minutes | Minimum continuous sitting time before a "stretch reminder" event fires. Only triggers if user has been sitting without a break for 60+ minutes. |
| `SLACKER_INTERVAL_MS` | 3600000 | 1 hour | Minimum interval between "slacker roasts" when productivity score is low. Prevents nagging every minute when the user is having a bad day. |
| `EXCESSIVE_BREAKS_MIN_WORKED_HOURS` | 3.0 | 3 hours | Minimum hours worked (desk time) before the excessive-breaks roast can fire. |

### Display & UI Timing

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `AWAY_GRACE_MS` | 60000 | 1 minute | After transitioning to AWAY, the clock face stays visible for 1 minute before switching to the "away" screen. Prevents jarring blank screen on brief stands. |
| `ALERT_DURATION_MS` | 8000 | 8 seconds | Default duration for alert messages on screen (used as fallback; actual duration is dynamically calculated by `getAlertDurationMs()`). |
| `RING_TRANSITION_MS` | 1000 | 1 second | Duration of the mood ring color fade transition around the clock face when state changes. |
| `DISPLAY_THROTTLE_MS` | 500 | 500ms | Minimum time between faceplate redraws. Prevents excessive display updates that would waste CPU cycles. |
| `DEV_REFRESH_MS` | 200 | 200ms | Faster refresh rate for the DEV faceplate (ID 3), which shows raw sensor data that needs near-real-time updates. |
| `METRIC_CYCLE_MS` | 15000 | 15 seconds | How often the bottom stats bar on the Default faceplate cycles to the next metric (desk time, focus time, breaks, etc.). |
| `BOOT_SPLASH_MS` | 4000 | 4 seconds | Minimum splash screen duration at boot. Ensures the user can see the startup logo before the clock face appears. |

### System Timing

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `LOOP_DELAY_MS` | 10 | 10ms | Main loop delay between iterations. The entire `loop()` body runs every ~10ms, giving ~100 ticks/second for sensor reading and display updates. |
| `SAVE_INTERVAL_MS` | 600000 | 10 minutes | Minimum interval between writing `stats.json` to LittleFS. Reduces flash wear while still persisting frequently enough to survive unexpected reboots. |
| `NTP_INTERVAL_MS` | 3600000 | 1 hour | Interval between NTP time syncs and weather API fetches. NTP drift is minimal; weather updates hourly is sufficient. |
| `NTP_RETRY_MS` | 15000 | 15 seconds | If NTP sync fails (no internet at boot), retry every 15 seconds until successful. |
| `FILTER_UPDATE_MS` | 100 | 100ms | How often the radar is polled and the rolling median filters are updated. 10Hz update rate. |
| `WIFI_CHECK_MS` | 10000 | 10 seconds | Interval between WiFi reconnection checks during the main loop. Prevents constant reconnection attempts. |
| `WIFI_TIMEOUT_MS` | 5000 | 5 seconds | WiFi connection timeout during `setup()`. If WiFi doesn't connect in 5s, the captive portal AP mode starts. |

---

## 3. Performance & Productivity

Constants that control AI limits, sensor thresholds, and the productivity score formula.

### AI Limits

| Constant | Value | What It Does |
|----------|-------|--------------|
| `DAILY_AI_LIMIT` | 30 | Maximum Groq API requests per day. Prevents runaway costs. Counter resets on day rollover. Once hit, AI falls back to local quotes. |

### Sensor Thresholds (Defaults)

These are initial defaults. Actual values are loaded from NVS Preferences at boot and configurable via the web dashboard or `SET` commands.

| Constant | Value | What It Does |
|----------|-------|--------------|
| `FILTER_MOTION_THRESHOLD` | 0.5 | Threshold for filtering motion signal noise. Values below this are treated as zero motion. |
| `DISTANCE_LIMIT_DEFAULT` | 120 | Default max distance (cm) from radar to detect presence. User must be within 1.2m to be considered "at desk". |
| `FOCUS_DISTANCE_LIMIT_DEFAULT` | 50 | Default focus zone distance (cm). User within 50cm is considered "in focus zone" (leaning in to work). |
| `MOTION_RATIO_LIMIT_DEFAULT` | 15 | Default motion ratio threshold (%). If more than 15% of the last 3 minutes involved motion, the user is "moving" (BUSY/DISTRACTED). |
| `RECENT_MOTION_WINDOW_S` | 180 | 3-minute rolling window for computing motion ratio. Old motion data falls out of the calculation. |
| `FILTER_WINDOW_DEFAULT` | 2.0 | Default rolling median filter window (seconds). 2 seconds smooths sensor noise while remaining responsive. |
| `OVERNIGHT_THRESHOLD_S` | 14400 | 4 hours. If the user was absent for 4+ hours, the next sit-down is treated as a new workday (triggers day rollover, stats reset). |
| `G0S_SENS_DEFAULT` | 90 | Default static sensitivity for LD2410 gate 0 (closest gate, 0-20cm). High value = very sensitive to still presence nearby. |

### Productivity Score Formula Constants

| Constant | Value | What It Does |
|----------|-------|--------------|
| `BREAK_PENALTY_TARGET` | 1.0 | Target: 1 break per hour. Each deviation penalizes the score by 25%. Formula: `penalty = 25 * (breakCount / hoursElapsed) / target`. |
| `BREAK_TIME_TARGET` | 0.10 | Target: 10% of desk time should be breaks. Deviation penalizes the score by 25%. Formula: `penalty = 25 * (breakRatio / target)`. |
| `FOCUS_BONUS_MULTIPLIER` | 1.5 | Focus time earns 1.5x credit toward the productivity score. Formula: `bonus = 1.5 * (focusTime / deskTime * 100)`. |
| `TARGET_HOURS_DEFAULT` | 8.0 | Default daily desk time goal (hours). Used for the goal-completion trigger and display. |
| `SCORE_INITIAL_PERIOD_S` | 300 | 5 minutes. During the first 5 minutes of a session, the productivity score defaults to 100%. Gives the user a "clean slate" start. |

### Lunch Timing

| Constant | Value | What It Does |
|----------|-------|--------------|
| `LUNCH_MIN_DESK_MS` | 1800000 | 30 minutes. Minimum desk time before a lunch reminder is allowed. Prevents lunch reminders before the user has actually started working. |

The lunch reminder fires at the learned lunch hour + 15 minutes (checked in the main loop, `main.cpp`), routed through `MessageManager` at P_NORMAL with no additional delay.

---

## 4. Radar Constants

Physical characteristics of the LD2410 mmWave radar sensor.

| Constant | Value | What It Does |
|----------|-------|--------------|
| `RADAR_CM_PER_GATE` | 20 | Each detection gate covers 20cm of range. Gate 0 = 0-20cm, Gate 1 = 20-40cm, etc. |
| `RADAR_MIN_GATES` | 2 | Minimum number of active gates. At least gates 0-1 (0-40cm) are always active. |
| `RADAR_MAX_GATES` | 8 | Maximum number of active gates. Full range = 0-160cm (8 gates × 20cm). |

**How it works:** The LD2410 divides its detection range into gates. Each gate has separate moving and static sensitivity settings (`g0mSens`, `g0sSens`, etc.). More active gates = wider detection range but more noise.

---

## 5. Filter Sizes

Buffer sizes for the rolling median filters that smooth radar data.

| Constant | Value | What It Does |
|----------|-------|--------------|
| `DIST_FILTER_SIZE` | 100 | Rolling median buffer for distance readings. At 10Hz update rate, this covers ~10 seconds of data. Sorts a window of the most recent 100 samples to extract the median. |
| `MOTION_FILTER_SIZE` | 10 | Rolling median buffer for motion readings. At 10Hz, covers ~1 second. Smaller window = faster response to motion changes (important for state transitions). |

**Why different sizes:** Distance needs more smoothing (radar distance jitter is noisy). Motion needs faster response (you want to detect the moment the user starts/stops moving). 

---

## 6. Message Queue Constants

Priority and relevance levels for the `MessageManager` scheduling system.

### Priority (display urgency)

| Constant | Value | When Used |
|----------|-------|-----------|
| `MSG_PRIORITY_URGENT` | 3000 | First-sit and welcome-back greetings (P_URGENT) — must always win over everything else. |
| `MSG_PRIORITY_HIGH` | 2250 | Goal completed, journal (incl. queued pages), nagging (P_HIGH) — important, but never outrank the greeting. |
| `MSG_PRIORITY_NORMAL` | 1500 | Stretch, slacker, streak-beaten, focus-end, lunch, excessive-breaks (P_NORMAL) — standard behaviour events. |
| `MSG_PRIORITY_LOW` | 500 | Low-urgency observations (P_LOW) — can wait if something more important is on screen. |

### Relevance (how long a message stays "fresh")

| Constant | Value | What It Does |
|----------|-------|--------------|
| `MSG_RELEVANCE_URGENT` | 300000 | 5 minutes. Time-sensitive triggers. If not displayed within 5 minutes, they're discarded. |
| `MSG_RELEVANCE_NORMAL` | 1800000 | 30 minutes. Standard behaviour events. Relevant for half an hour. |
| `MSG_RELEVANCE_LOW` | 3600000 | 1 hour. Journal prompts and low-priority observations. |

**How they interact:** `MessageManager` picks the highest-priority message that is still within its relevance window. Once relevance expires, the message is silently dropped.

---

## 7. Journal & Curation

Constants controlling when task journal prompts and nagging triggers fire.

| Constant | Value | Real-World | What It Does |
|----------|-------|------------|--------------|
| `MORNING_JOURNAL_DELAY_MS` | 300000 | 5 minutes | After first sit-down in the morning, wait 5 minutes before showing the morning journal prompt. Gives time to settle in. |
| `PRE_LUNCH_JOURNAL_MINS_BEFORE` | 15 | 15 minutes | Show the pre-lunch task review 15 minutes before the learned lunch hour. |
| `END_OF_DAY_JOURNAL_HOURS_BEFORE` | 1 | 1 hour | Show the end-of-day task review 1 hour before the learned workday end. |
| `MIDDAY_TASK_CHECK_HOUR` | 12 | 12:00 PM | At noon, the curation system checks if any daily tasks have been completed and injects that observation into AI prompts. |
| `NAGGING_TRIGGER_DELAY_MS` | 2100000 | 35 minutes | Cadence of the overdue-task nag queue: the first nag fires 35 minutes into a sitting session, then every 35 minutes while seated. Each nag names the next overdue task (most-expired-first). The cursor persists across sessions and resets at midnight. |
| `TASK_OVERDUE_DAYS_LIMIT` | 3 | 3 days | Severity cutoff for the AI task synthesis: daily tasks more than 3 days past due are flagged as "highly overdue" in the AI prompt context. |
| `TASK_OVERDUE_MONTHS_LIMIT` | 3 | 3 months | Severity cutoff for the AI task synthesis: monthly tasks more than 3 months past due are flagged as "highly overdue" in the AI prompt context. |
| `TASK_SYNTHESIS_MAX_CHARS` | 500 | ~10 bullets | Max length of the compact `[TASK SYNTHESIS]` block injected into AI prompt observations (counts + task names). Longer lists are truncated with `...`. |

---

## 8. MQTT Constants

All MQTT connection, topic, and buffer configuration.

### Connection

| Constant | Value | What It Does |
|----------|-------|--------------|
| `MQTT_BROKER_IP` | `"192.168.15.18"` | Default MQTT broker IP address. Overridden by NVS Preferences at boot. |
| `MQTT_BROKER_PORT` | 1883 | Default MQTT broker port (standard unencrypted MQTT). |
| `MQTT_RECONNECT_INTERVAL_MS` | 10000 | 10 seconds between reconnection attempts if the MQTT connection drops. |
| `MQTT_CLIENT_ID` | `"DeskBuddyClient"` | Client identifier sent to the broker. Must be unique on the network. |

### Topics (Subscribed)

| Constant | Value | Direction | What It Does |
|----------|-------|-----------|--------------|
| `MQTT_SUBSCRIBE_TOPIC` | `"deskbuddy/#"` | IN | Wildcard subscription. Receives ALL messages on any `deskbuddy/*` subtopic. |

### Topics (Published)

| Constant | Value | Direction | What It Does |
|----------|-------|-----------|--------------|
| `MQTT_STATUS_TOPIC` | `"deskbuddy/status"` | OUT | Published once on every MQTT connect with payload `"online"`. |
| `MQTT_STATUS_PAYLOAD` | `"online"` | OUT | The payload for the status topic. |
| `MQTT_ECHO_TOPIC` | `"deskbuddy/echo"` | OUT | Echoes every triggered message back for debugging. |
| `MQTT_DEBUG_CMD_TOPIC` | `"deskbuddy/debug/cmd"` | IN | Debug command input (GET/SET/SIM/SYS/TRIGGER plain-text protocol). |
| `MQTT_DEBUG_RESP_TOPIC` | `"deskbuddy/debug/resp"` | OUT | Debug command responses (JSON). |

### Buffers

| Constant | Value | What It Does |
|----------|-------|--------------|
| `MQTT_HISTORY_SIZE` | 50 | Circular buffer size for received MQTT messages. Last 50 messages retained for diagnostics. |
| `SYSTEM_LOG_SIZE` | 15 | Reserved but unused in current code. Originally intended for a local log buffer. |

---

## 9. Captive Portal

| Constant | Value | What It Does |
|----------|-------|--------------|
| `AP_SSID` | `"DeskBuddy-Setup"` | WiFi network name broadcast when the device enters AP mode (after failed WiFi connection). DNS wildcard redirects all HTTP requests to the setup page. |

---

## 10. Utility Functions

### `getAlertDurationMs(int lineCount, bool isPage1)`

Calculates how long an alert message should stay on screen based on how many lines it has.

```cpp
inline unsigned long getAlertDurationMs(int lineCount, bool isPage1 = false) {
  if (isPage1) {
    return 4000UL; // Dashboard summary: always 4 seconds
  }
  unsigned long ms = 3000 + (lineCount * 1800); // 3s base + 1.8s per line
  if (ms < 5000) ms = 5000;   // Minimum 5 seconds
  if (ms > 10000) ms = 10000; // Maximum 10 seconds
  return ms;
}
```

**Logic:**
- Dashboard page (page 1 of journal): always 4 seconds
- Other messages: 3 seconds base + 1.8 seconds per line of text
- Clamped to 5-10 second range

**Example:** A 3-line message = 3000 + (3 × 1800) = 8400ms ≈ 8.4 seconds on screen.

---

## 11. Structs & Namespaces

### `RGB` Struct

```cpp
struct RGB {
  uint8_t r, g, b;
};
```

Simple color struct used by `JournalConfig` for text colors on the TFT display. Values are 0-255 per channel.

### `JournalConfig` Namespace

Configuration for the task journal and due-now screen layouts. Controls Y-position, font, and color for every text element on these screens.

#### Page One Overview (Dashboard)

| Field | Value | What It Does |
|-------|-------|--------------|
| `pageOneTitleY` | 17 | Y-pixel position of the page title |
| `pageOneTitleFont` | `""` | Empty string = use default small system font |
| `pageOneTitleColor` | `{245, 158, 11}` | Yellow (amber) title text |
| `pageOneTaskFont` | `""` | Default small font for task items |
| `pageOneTaskColor` | `{255, 255, 255}` | White task text |

#### Tasks Page (Journal Pages 2+)

| Field | Value | What It Does |
|-------|-------|--------------|
| `tasksTitleY` | 17 | Y-position of the tasks page title |
| `tasksTitleFont` | `""` | Default small font |
| `tasksTitleColor` | `{245, 158, 11}` | Yellow title |
| `tasksTaskFont` | `""` | Default small font for tasks |
| `tasksTaskColor` | `{255, 255, 255}` | White task text |

#### Due Now Page

| Field | Value | What It Does |
|-------|-------|--------------|
| `dueTitleY` | 17 | Y-position of the "Due Now" title |
| `dueTitleFont` | `""` | Default small font |
| `dueTitleColor` | `{245, 158, 11}` | Yellow title |
| `dueTextColor` | `{255, 255, 255}` | White description text |
| `dueTimeY` | 200 | Y-position for the scheduled time display (near bottom of 240px screen) |
| `dueTimeFont` | `""` | Default small font for the time |
| `dueTimeColor` | `{239, 68, 68}` | Red time text (draws attention to urgency) |

**Design note:** All fonts are `""` (empty string) which defaults to the TFT_eSPI built-in small font. Custom VLW fonts from LittleFS could be substituted here for smoother rendering.
