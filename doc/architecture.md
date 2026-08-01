# DeskBuddy Architecture

High-level architecture reference for the DeskBuddy smart desk companion firmware.

---

## 1. Project Overview

DeskBuddy is an ESP32-C3-based desktop companion that uses an LD2410 mmWave radar sensor to detect user presence, motion, and distance at a desk. It drives a round 240x240 GC9A01 TFT display to show clock faces, productivity metrics, and AI-generated coaching messages. The firmware tracks work/break sessions, computes a real-time productivity score, learns the user's daily work patterns, and delivers persona-based motivational nudges via local fallback quotes or cloud AI (Groq LLM API). It exposes a web dashboard for configuration and real-time monitoring, and integrates with MQTT for home-automation ecosystems.

---

## 2. Tech Stack

### Hardware
- **MCU:** ESP32-C3 DevKitM-1 (RISC-V, Wi-Fi, BLE)
- **Display:** GC9A01 round 240x240 TFT (SPI, via TFT_eSPI)
- **Sensor:** HLK-LD2410 mmWave radar (Serial1, 256000 baud)
- **Storage:** LittleFS (on-chip flash filesystem)

### Firmware Framework
- **Platform:** PlatformIO with Arduino framework (`espressif32@6.6.0`)
- **Upload:** OTA (`espota`) or serial (`esptool`)
- **Partition table:** Custom `partitions.csv`

### Key Libraries
| Library | Purpose |
|---------|---------|
| `bodmer/TFT_eSPI` | TFT display driver + sprite engine |
| `ncmreynolds/ld2410` | LD2410 radar protocol driver |
| `arduino-libraries/NTPClient` | NTP time synchronization |
| `bblanchon/ArduinoJson` (~6.19) | JSON serialization/deserialization |
| `knolleary/PubSubClient` | MQTT client |
| `avantmaker/ESP32_AI_Connect` | Cloud AI API client (Groq/Gemini/DeepSeek) |
| `denyssene/SimpleKalmanFilter` | Sensor noise filtering |

### External Services
- **AI:** Groq API (`llama-3.3-70b-versatile`) via OpenAI-compatible endpoint
- **Weather:** OpenWeatherMap API (free tier)
- **Time:** NTP (UTC-3 offset for Argentina)

---

## 3. Directory Layout

```
src/
  main.cpp              Entry point: setup(), loop(), global state instances
  Constants.h           All timing, threshold, and system constants
  State.h               Four global state structs (Config, Stats, Runtime, Todo)
  Behaviour.h           Event types (13), local fallback quotes (4 personas), AI prompt templates
  AI.h                  FreeRTOS background task, prompt construction, AI/local fallback dispatch
  Display.h             TFT display update loop, RLE image decoder, message renderer
  Faceplates.h          6+ clock face rendering functions (Default, Minimalist, HiTech, Dev, Aviator, DeskBuddy)
  Radar.h               LD2410 setup, RollingMedianFilter class, sensor config sync
  PresenceAnalysis.h    Day rollover logic, presence accumulation, history blending
  Learning.h            Workday start/end/lunch detection from hourly presence history
  Curation.h            AI prompt enrichment: behavioral observations, task overdue analysis
  Stats.h               Session and daily stats reset functions
  MessageManager.h/.cpp Priority-based message queue with scheduling and relevance windows
  MqttService.h         MQTT connection, pub/sub, message queue, history buffer
  MqttDebug.h           MQTT debug command handler (GET/SET system parameters over MQTT)
  Web.h                 HTTP server: dashboard, TODO manager, settings, credentials, file manager
  Logger.h              Category-tagged logging to Serial + MQTT

Credentials.h          WiFi/API key defaults (gitignored, not in repo)
platformio.ini         Build configuration
partitions.csv         Custom flash partition table
data/                  LittleFS filesystem image (RLE assets, fonts, todo.json)
tools/                 Helper scripts
.agents/               AI agent skills (compress-image, convert-font, create-faceplate)
```

---

## 4. Entry Points

### `setup()` Boot Sequence (`main.cpp:381`)

1. Serial init (115200 baud)
2. Create FreeRTOS mutexes (`aiMutex`, `mqttHistoryMutex`, `mqttPublishQueueMutex`)
3. Spawn persistent `aiQueryTask` FreeRTOS background task (12KB stack)
4. Load all config from NVS Preferences (WiFi, MQTT, API keys, radar gate sensitivities)
5. Mount LittleFS and load `stats.json` (daily statistics)
6. Init TFT display, show splash screen
7. Init LD2410 radar sensor (Serial1, configure gates, sync calibration to flash)
8. Connect WiFi (static or DHCP). On failure: start captive portal AP mode (`DeskBuddy-Setup`)
9. Register mDNS (`deskbuddy.local`)
10. Setup MQTT client and subscribe to `deskbuddy/#`
11. Start NTP client (UTC-3 offset)
12. Setup HTTP web server (25+ routes) and OTA update handler
13. Enforce 4-second minimum splash screen delay

### `loop()` Tick Cycle (`main.cpp:695`)

Runs every ~10ms (`LOOP_DELAY_MS`). In order:

1. **OTA check** -- skip loop body if OTA in progress
2. **Captive portal DNS** -- process DNS requests in AP mode
3. **Web server** -- handle incoming HTTP requests
4. **WiFi reconnect** -- check every 10s, reconnect if disconnected
5. **NTP time sync** -- hourly update (or 15s retry if not yet synced)
6. **Weather fetch** -- hourly OpenWeatherMap API call
7. **Radar read** -- poll LD2410, apply rolling median filter at 10Hz, compute distance/motion
8. **Presence state machine** -- debounce, session transitions (Away/Present), stop-by detection
9. **Behaviour triggers** -- stretch (45m), slacker roast (1h), streak beaten, lunch, goal, journal, nagging, task-due
10. **MessageManager** -- process scheduled message queue, dispatch due messages
11. **MQTT loop** -- reconnect, process publish queue
12. **Display update** -- render active faceplate or alert screen
13. **Stats persistence** -- save to LittleFS every 10 minutes (if changed)
14. **Delay** -- 10ms

---

## 5. Core State Management

Four global state structs are defined in `State.h` and instantiated in `main.cpp:51-54`:

### `ConfigState appConfig`
User preferences and system configuration. Loaded from NVS Preferences at boot.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `targetHours` | float | 8.0 | Daily desk time goal (hours) |
| `aiMode` | int | 1 | 0=Eco(off), 1=Balanced, 2=Frequent |
| `aiPersona` | int | 0 | 0=Coach, 1=Critic, 2=Sweet, 3=Friend |
| `clockFace` | int | 0 | Active faceplate variant (0-9) |
| `userName` | String | "human" | Name used in messages and prompts |
| `focusDistanceLimit` | int | 50 | cm threshold for Focus state |
| `motionRatioLimit` | int | 15 | % motion threshold for Busy/Distracted |
| `deskDistanceLimit` | int | 120 | cm max presence distance |
| `filterWindow` | float | 2.0 | Rolling median window (seconds) |
| `wifiSsid/wifiPass` | String | -- | WiFi credentials |
| `wifiStaticEnabled` | bool | true | Use static IP configuration |
| `mqttBroker/mqttPort` | String/int | 192.168.15.18:1883 | MQTT broker |
| `groqApiKey` | String | -- | Groq API key for AI |
| `openWeatherKey` | String | -- | OpenWeatherMap API key |
| `g0mSens`..`g6sSens` | int | varies | LD2410 per-gate sensitivities |

### `StatsState appStats`
Daily statistics. Persisted to `stats.json` via LittleFS.

Key fields: `totalDeskTime`, `totalFocusTime`, `totalBreakTime`, `breakCount`, `productivityScore`, `longestSittingStreak`, `firstSitEpoch`, `hourlyPresenceHistoryWeekly[7][24]`, `presenceMsCurrentDay[24]`, `dailyAiRequestCount`, `fsWriteCount`, daily trigger flags.

### `RuntimeState appState`
Ephemeral runtime state. Not persisted.

Key fields: `currentPresenceState`, `filteredDetectionDist`, `sensorPresenceDetected`, `sensorMovingTargetDetected`, `isAILoading`, `aiResponse`, `hasNewAIResponse`, `currentPrompt`, `mqttConnected`, `aiMutex`, `mqttHistory[50]`, `simulationMode`, `captivePortalMode`.

### `TodoState appTodo`
Raw JSON string for the task list (`{"daily":[],"monthly":[]}`). Read from `/todo.json` on LittleFS.

---

## 6. Presence Detection & State Machine

### Radar Input Pipeline

```
LD2410 (Serial1 256k) --> radar.read()
  --> sensorPresenceDetected / movingTargetDetected / detectionDistance
  --> RollingMedianFilter (detectionDistFilter, motionFilter)
  --> filteredDetectionDist, filteredMovingTarget (10Hz update)
```

- **RollingMedianFilter** (`Radar.h:15`): Pre-allocated buffer (100 samples for distance, 10 for motion). Sorts a sliding window for median extraction. Avoids per-call heap allocation.

### Raw State Derivation

```
rawPresent = sensorPresenceDetected && (distance == 0 || distance <= deskDistanceLimit)

if rawPresent:
  recentMotionRatio = sum(motionBuckets) / sum(deskBuckets) over 180s window
  inFocusZone = (distance > 0 && distance < focusDistanceLimit)
  highMotion = (recentMotionRatio > motionRatioLimit)

  rawState = inFocusZone ? (highMotion ? BUSY : FOCUS)
                          : (highMotion ? DISTRACTED : REGULAR)
else:
  rawState = AWAY
```

### The 5 Presence States

| ID | Name | Trigger |
|----|------|---------|
| 0 | `STATE_AWAY` | No presence detected, or distance > deskDistanceLimit |
| 1 | `STATE_FOCUS` | In focus zone (< focusDistanceLimit cm) + low motion (< motionRatioLimit%) |
| 2 | `STATE_BUSY` | In focus zone + high motion (>= motionRatioLimit%) |
| 3 | `STATE_DISTRACTED` | Beyond focus zone + high motion |
| 4 | `STATE_REGULAR` | Beyond focus zone + low motion |

### Debouncing

- **Presence ON:** 2s debounce (5s overnight on first sit)
- **Presence OFF:** 10s debounce
- **State transitions (e.g., Focus -> Busy):** 30s sticky confirmation window (`STICKY_CONFIRM_MS`)

### Session Lifecycle

```
AWAY --[presence stable]--> PRESENT (triggers EVENT_FIRST_SIT or EVENT_WELCOME_BACK)
  --[away stable]--> AWAY (saves session, starts break timer)

Stop-By Detection: If present session < 8 min and during off-hours,
  roll back the break count and treat as a brief return.

Late-Hours First Sit: A first sit during late hours holds the flag
  (no greeting, no day-start). It burns at the workday crossing
  (silent, firstSitEpoch = sitDownEpoch) or on a work-hours sit-down
  (normal FIRST_SIT). LATEHOURS_SIT greets instead during quiet hours.

Day Rollover: On first sit after midnight (or 4+ hour absence):
  mergeCurrentDayPresence() --> resetDailyStats() --> new workday
```

### Productivity Score Formula

```
raw = 100 - breakPenalty - timePenalty + focusBonus

breakPenalty = 25 * (breakCount / hoursElapsed)       // target: 1 break/hr
timePenalty  = 25 * (activeBreakRatio / 0.10)         // target: 10% break time
focusBonus   = 1.5 * (focusTime / deskTime * 100)     // 1.5x multiplier

score = constrain(raw, 0, 100)
// First 5 minutes: defaults to 100%
```

---

## 7. AI & Behaviour System

### Event Types

| ID | Event | Trigger Condition |
|----|-------|-------------------|
| 0 | `EVENT_FIRST_SIT` | User sits down for the first time today |
| 1 | `EVENT_WELCOME_BACK` | User returns after a break (>= 3 min) |
| 2 | `EVENT_STRETCH` | 45 minutes of continuous sitting |
| 3 | `EVENT_FOCUS_END` | User leaves after a focus session (>= 5 min) |
| 4 | `EVENT_SLACKER` | Sitting > 1 hour with productivity score < 35% |
| 5 | `EVENT_STREAK_BEATEN` | Longest sitting streak record broken |
| 6 | `EVENT_LUNCH_REMINDER` | At learned lunch hour + 15 min, if desk time > 30 min |
| 8 | `EVENT_EXCESSIVE_BREAKS` | Break rate > 1/hour |
| 9 | `EVENT_GOAL_COMPLETED` | Desk time >= target hours |
| 10 | `EVENT_JOURNAL` | Morning/pre-lunch/end-of-day task review |
| 11 | `EVENT_NAGGING` | Tasks overdue by 3+ days, after 2h sitting |
| 12 | `EVENT_TASK_DUE` | Scheduled daily task matches current time |
| 13 | `EVENT_PAGE` | Follow-up screen for messages split at `MSG_PAGE_MAX_CHARS` (110) |
| 14 | `EVENT_LATEHOURS_SIT` | Any sit-down during late hours (outside learned workday +/- 30m); replaces FIRST_SIT/WELCOME_BACK |

### AI Decision Flow (`AI.h:277`)

```
triggerBehaviour(eventType, detail)
  --> Is event a Journal or Task Due? --> handle locally (no AI)
  --> Should use AI? (aiMode, dailyCap=30, WiFi available)
      YES --> Build structured prompt:
                [ROLE] persona preamble + shared banned-phrase block
                [LIVE TELEMETRY] name, time, weather, desk/focus/break stats
                [OBSERVATIONS] curation discrepancies
                [ACTION REQUIRED] event-specific template
              --> Notify FreeRTOS task --> Groq API call
              --> On success: post response to appState.aiResponse
              --> On failure or AI busy: fall back to local quote
      NO --> Pick random local quote from persona array
```

**Routing:** Every event path now goes through `MessageManager` before `triggerBehaviour()` (see `main.cpp` loop). FIRST_SIT/WELCOME_BACK/LATEHOURS_SIT use P_URGENT (3000), GOAL/JOURNAL/NAGGING use P_HIGH (2250), and STRETCH/SLACKER/STREAK_BEATEN/FOCUS_END/LUNCH/EXCESSIVE_BREAKS use P_NORMAL (1500, delay 0, R_NORMAL) — a true 4-tier numeric ladder (URGENT 3000 > HIGH 2250 > NORMAL 1500 > LOW 500, F9). WELCOME_BACK always fires on return-to-desk; EXCESSIVE_BREAKS (P_NORMAL) is queued behind it so the greeting is never replaced by the roast (F8). If an AI query is already in flight when a trigger arrives, `triggerBehaviour` falls back to a local quote instead of dropping the event.

**aiMode whitelist (mode 1 = Balanced):** `EVENT_FIRST_SIT`, `EVENT_WELCOME_BACK`, `EVENT_LATEHOURS_SIT`, `EVENT_STRETCH`, `EVENT_LUNCH_REMINDER`, `EVENT_EXCESSIVE_BREAKS`, `EVENT_GOAL_COMPLETED`, `EVENT_NAGGING`. Mode 2 = all events use AI; mode 0 = local quotes only.

### 4 Personas

| ID | Name | Style |
|----|------|-------|
| 0 | Coach | Tony Robbins but quieter. Direct, raises the bar. |
| 1 | Critic | Sharp-tongued roast. Laugh first, sting second. |
| 2 | Sweet | Warm motherly companion. Soft guilt, genuine warmth. |
| 3 | Friend | Bill Murray energy. Deadpan, philosophical non-sequiturs. |

Each event type has 5 pre-written local fallback quotes per persona (20 quotes per event). AI prompts include a shared banned-phrase block (all personas) to avoid cliches.

### FreeRTOS Task

- **Task:** `aiQueryTask` (12KB stack, priority 1)
- **Notification:** `xTaskNotifyGive(aiQueryTaskHandle)` triggers execution
- **Session safety:** Stale responses discarded if `querySessionId != currentSitDownSessionId` or user went away
- **Timeout:** Safety reset of `isAILoading` after 45 seconds

---

## 8. Display Pipeline

### Faceplate Variants

| ID | Name | Type |
|----|------|------|
| 0 | Default | Digital clock, weather, rotating stats bar, mood ring |
| 1 | Minimalist | Clean digital with minimal chrome |
| 2 | HiTech | Technical/futuristic digital layout |
| 3 | DEV | Developer/debug mode with raw sensor data |
| 4 | Aviator | Analog clock with sprite-based watch hands |
| 5-9 | DeskBuddy variants | Character-themed (DeskBuddy, DeskAura, DeskCat, DeskWho, DeskBit) |

### RLE Image Format

Images on LittleFS use PackBits-RLE compression (16-bit RGB565):

```
Offset 0:  [2 bytes] Width (uint16 LE)
Offset 2:  [2 bytes] Height (uint16 LE)
Offset 4+: Packets...

Packet:
  Header byte:
    Bit 7 (0x80): 1 = repeating run, 0 = literal run
    Bits 0-6:      count (0-127, actual count = value + 1)

  Repeating run (bit7=1): header + [2 bytes color]
  Literal run (bit7=0):   header + [count * 2 bytes colors]
```

- Maximum run length: 128 pixels
- Supports optional color tinting via `overrideColor` (luminance-based recoloring)

### Sprite Watch Hands

The Aviator face (ID 4) uses `TFT_eSprite` for double-buffered watch hands:
- `hourHandSprite`, `minuteHandSprite`, `secondHandSprite`, `centerBgSprite`
- Loaded from RLE files (`buddy_eye_o.rle`, etc.)
- Cleaned up on faceplate switch to free RAM

### Message Rendering (`Display.h:258`)

- **Color codes:** `[RED]`, `[GREEN]`, `[YELLOW]`, `[BLUE]`, `[ORANGE]`, `[GREY]`, `[WHITE]` -- inline tags parsed per-line
- **Two-column layout:** `|` separator splits left/right columns (right-aligned / left-aligned)
- **Round display wrapping:** Line width scales with `sqrt(120^2 - dy^2)` to fit the circular screen
- **Page types:** Standard (auto-wrap), Journal Dashboard (explicit `\n`), Journal Tasks, Due Now (with red time field)

---

## 9. Communication

### MQTT

**Topics:**

| Topic | Direction | Purpose |
|-------|-----------|---------|
| `deskbuddy/#` | Subscribe | Wildcard subscription for all commands |
| `deskbuddy/status` | Publish | Online status on connect |
| `deskbuddy/echo` | Publish | Echo of triggered messages |
| `deskbuddy/heap` | Publish | Heap telemetry (every 60s) |
| `deskbuddy/log/<category>` | Publish | System logs by category |
| `deskbuddy/debug/cmd` | Subscribe | Debug commands (GET/SET/SIM/SYS/TRIGGER) |
| `deskbuddy/debug/resp` | Publish | Debug command responses |
| `deskbuddy/debug/ai/request` | Publish | Full AI request payload (debug) |
| `deskbuddy/debug/ai/response` | Publish | Full AI response (debug) |

**Publish Queue:** Thread-safe `std::queue<MqttQueueMessage>` protected by `mqttPublishQueueMutex`. Max 20 entries; oldest discarded on overflow.

**History Buffer:** Circular buffer of 50 `MqttMessage` entries (topic + payload + timestamp), protected by `mqttHistoryMutex`.

### Web Server

**Pages:**

| Route | Handler | Description |
|-------|---------|-------------|
| `/` | `handleRoot` | Real-time dashboard (250ms polling) |
| `/todo` | `handleTodo` | TODO task manager (daily + monthly) |
| `/settings` | `handleSettings` | Configuration panel + radar chart |
| `/credentials` | `handleCredentials` | WiFi, MQTT, API key management |
| `/file-manager` | `handleFileManager` | LittleFS file upload/download/delete |
| `/setup` | `handleSetup` | Captive portal WiFi provisioning |

**API Endpoints:**

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/radar-data` | GET | Full JSON state (presence, stats, config, AI, occupancy) |
| `/api/tasks` | GET | Load `todo.json` |
| `/api/tasks/save` | POST | Save `todo.json` |
| `/save-settings` | POST | Update all config fields |
| `/save-credentials` | POST | Update WiFi/MQTT/API keys + reboot |
| `/wifi-scan` | GET | Scan available WiFi networks |
| `/trigger-event` | GET | Manually trigger a behaviour event |
| `/reset-stats` | GET | Reset daily statistics |
| `/reset-esp` | GET | Reboot device |
| `/factory-reset` | GET | Clear all preferences + stats + reboot |
| `/files` | GET | List LittleFS files |
| `/download` | GET | Download a file |
| `/delete-file` | GET | Delete a file (stats.json protected) |
| `/upload` | POST | Upload file to LittleFS |

### Captive Portal

On WiFi connection failure, DeskBuddy starts an AP named `DeskBuddy-Setup` with DNS wildcard (`*` -> AP IP). OS captive portal probes (`/generate_204`, `/hotspot-detect.html`, etc.) redirect to `/setup` for WiFi provisioning.

### OTA Updates

ArduinoOTA is initialized in `setup()`. During OTA, the main loop returns immediately (`otaInProgress` flag). Upload via `espota` protocol on the configured IP.

---

## 10. Persistence

### LittleFS Files

#### `/stats.json`

Atomic write pattern: write to `stats.json.tmp`, then `rename()` to `stats.json`. Saved every 10 minutes (if changed) and on session transitions.

```json
{
  "firstSitToday": true,
  "firstSitEpoch": 0,
  "breakCount": 0,
  "totalDeskTime": 0,
  "totalFocusTime": 0,
  "totalBreakTime": 0,
  "overnightBreakDuration": 0,
  "lastAwayEpoch": 0,
  "dailyAiRequestCount": 0,
  "lastNtpDay": -1,
  "longestSittingStreak": 0,
  "latestBreakDuration": 0,
  "totalMotionTime": 0,
  "motionCount": 0,
  "historyDaysCountWeekly": [0,0,0,0,0,0,0],
  "hourlyPresenceHistoryWeekly": [[0..23],[0..23],...x7],
  "presenceMsCurrentDay": [0..23],
  "lunchReminderTriggered": false,
  "excessiveBreaksTriggered": false,
  "goalCompletedTriggered": false,
  "morningJournalTriggered": false,
  "preLunchJournalTriggered": false,
  "endOfDayJournalTriggered": false,
  "naggingTriggeredToday": false,
  "fsWriteCount": 0,
  "fsReadCount": 0,
  "fsWritesToday": 0
}
```

#### `/todo.json`

```json
{
  "daily": [
    {
      "text": "Task description",
      "hour": 14,
      "minute": 30,
      "recurrent": true,
      "startDate": "2026-07-27",
      "endDate": "",
      "completedDates": ["2026-07-25"]
    }
  ],
  "monthly": [
    {
      "text": "Monthly goal",
      "day": 15,
      "recurrent": true,
      "startMonth": "2026-07",
      "endMonth": "",
      "completedMonths": ["2026-06"]
    }
  ]
}
```

### NVS Preferences

Namespace: `deskbuddy`. Used for configuration that survives `stats.json` resets.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `aiMode` | int | 1 | AI mode (0/1/2) |
| `aiPersona` | int | 0 | Persona (0-3) |
| `clockFace` | int | 0 | Active faceplate |
| `targetHours` | float | 8.0 | Daily desk goal |
| `userName` | String | "human" | User name |
| `focusDistLim` | int | 50 | Focus distance (cm) |
| `motionRatioLim` | int | 15 | Motion ratio threshold (%) |
| `distLimit` | int | 120 | Desk distance limit (cm) |
| `filterWindow` | float | 2.0 | Median filter window (s) |
| `hasMail` | bool | false | Mail alert flag |
| `time24h` | bool | true | 24h time format |
| `buddyFontIdx` | int | 0 | Font variant for DeskBuddy faces |
| `wifiSsid` | String | -- | WiFi SSID |
| `wifiPass` | String | -- | WiFi password |
| `wifiStatic` | bool | true | Static IP mode |
| `wifiIp/Gw/Subnet/Dns1/Dns2` | String | varies | Network config |
| `mqttBroker` | String | 192.168.15.18 | MQTT broker IP |
| `mqttPort` | int | 1883 | MQTT broker port |
| `groqKey` | String | -- | Groq API key |
| `geminiKey` | String | -- | Gemini API key |
| `deepseekKey` | String | -- | DeepSeek API key |
| `owKey` | String | -- | OpenWeatherMap key |
| `owLat/owLon` | float | -23.11/-46.53 | Weather coordinates |
| `g0mSens`..`g6sSens` | int | varies | Per-gate radar sensitivities |

---

## 11. Learning & Curation

### Hourly Presence History

- **Accumulation:** `presenceMsCurrentDay[hour]` incremented by loop elapsed time while present (`PresenceAnalysis.h:10`)
- **Blending:** On day rollover, each hour is blended: `history = (history * 3 + todayPct * 2) / 5` (60% historical, 40% today)
- **Day count:** `historyDaysCountWeekly[dayIndex]` increments (max 1000) to weight the blend over time

### Learned Workday Patterns (`Learning.h`)

| Function | Algorithm | Fallback |
|----------|-----------|----------|
| `getLearnedWorkdayStart(day)` | First hour >= 15% presence between 4AM-12PM | 8:00 |
| `getLearnedWorkdayEnd(day)` | Last hour >= 15% presence from 4PM onwards | 18:00 |
| `getLearnedLunchHour(day)` | Hour between 11AM-2PM with lowest presence | 12:00 |

### Curation Observations (`Curation.h`)

The `getCurationObservations()` function generates behavioral observation strings injected into AI prompts:

- **Break frequency:** Compares actual break rate to 1/hour target
- **Break duration:** Compares break time ratio to 10% target
- **Task synthesis (`getTodoObservations`):** injects a compact `[TASK SYNTHESIS]` block into every AI prompt — counts (daily pending today, monthly due today, daily overdue 3d+, monthly overdue this-month/3mo+) plus the task names with times/overdue durations (capped at `TASK_SYNTHESIS_MAX_CHARS`). NAGGING prepends a `Highly Overdue Tasks Alert!` framing.
- **Midday task check:** At 12:00 PM, adds a past-midday observation when daily tasks remain
- **Pattern anomalies:** Detects unusual deviations from learned occupancy patterns

---

## 12. Thread Safety

### Mutex Map

| Mutex | Protects | Created In |
|-------|----------|------------|
| `appState.aiMutex` | `appState.currentPrompt`, `appState.aiResponse`, `appState.hasNewAIResponse`, `appState.lastResponseIsAi`, `appState.lastTriggeredEventType`, `appState.lastTriggeredEventDetail`, `appState.currentUserName`, `aiQuerySessionId` | `main.cpp:391` |
| `appState.mqttHistoryMutex` | `appState.mqttHistory[]` circular buffer | `main.cpp:395` |
| `mqttPublishQueueMutex` | `mqttPublishQueue` (std::queue) | `main.cpp:398` |

### FreeRTOS Task

| Task | Stack | Priority | Purpose |
|------|-------|----------|---------|
| `aiQueryTask` | 12288 bytes | 1 | Background AI API queries via Groq |

The main `loopTask` (Arduino default) runs all other subsystems. The AI task blocks on `ulTaskNotifyTake` until `triggerBehaviour()` wakes it via `xTaskNotifyGive`.

### Shared State Protocol

- AI task reads `currentPrompt` under mutex, releases mutex, then performs HTTP request (no lock held during I/O)
- AI task writes response to `aiResponse` + sets `hasNewAIResponse` under mutex
- Display loop reads response under mutex, clears `hasNewAIResponse`, releases mutex
- Web server reads `aiResponse` under mutex for `/radar-data` endpoint

---

## 13. Data Flow Diagram

```
                    ┌─────────────┐
                    │  LD2410     │
                    │  mmWave     │
                    │  Radar      │
                    └──────┬──────┘
                           │ Serial1 (256k baud)
                           v
                  ┌────────────────┐
                  │  Radar Read    │
                  │  + Rolling     │
                  │  Median Filter │
                  └───────┬────────┘
                          │ rawPresent, rawState, distance, motion
                          v
              ┌───────────────────────┐
              │  Presence State       │
              │  Machine              │
              │  (debounce, session   │──> accumulatePresence()
              │   transitions,        │──> mergeCurrentDayPresence()
              │   stop-by detection)  │──> resetDailyStats()
              └───────┬───────────────┘
                      │ eventType + detail
                      v
          ┌───────────────────────┐
          │  triggerBehaviour()   │
          │  (AI.h)               │
          └───┬───────────────┬───┘
              │               │
     Use AI?  │               │  Local Fallback
              v               v
     ┌──────────────┐  ┌──────────────┐
     │ FreeRTOS     │  │ Pick random  │
     │ Groq API     │  │ quote from   │
     │ Query Task   │  │ persona*event│
     └──────┬───────┘  └──────┬───────┘
            │                  │
            v                  v
     ┌─────────────────────────────┐
     │  appState.aiResponse        │
     │  appState.hasNewAIResponse  │
     │  (mutex-protected)          │
     └─────┬───────────────┬───────┘
           │               │
           v               v
  ┌─────────────┐  ┌──────────────┐
  │  Display    │  │  MQTT        │
  │  Update     │  │  Publish     │
  │  (faceplate │  │  (echo topic)│
  │   or alert) │  │              │
  └─────────────┘  └──────────────┘

   ┌────────────────────────────────────┐
   │  Web Dashboard (/radar-data)       │
   │  250ms polling --> full JSON state  │
   └────────────────────────────────────┘

   ┌────────────────────────────────────┐
   │  MQTT Subscribe (deskbuddy/#)      │
   │  --> MessageManager queue          │
   │  --> triggerBehaviour(EVENT_MQTT)  │
   └────────────────────────────────────┘
```

---

## 14. File Format Reference

### RLE Image Binary Format

```
[2 bytes] uint16 LE  - Image width (W)
[2 bytes] uint16 LE  - Image height (H)
[N bytes] Packet data (total pixels = W * H)

Packet structure:
  Byte 0: Header
    Bit 7:     Run type (1=repeating, 0=literal)
    Bits 0-6:  Count minus 1 (0-127, so 1-128 pixels)

  If repeating (bit7=1):
    + [2 bytes] RGB565 color (repeated for count pixels)

  If literal (bit7=0):
    + [count * 2 bytes] RGB565 color stream (one per pixel)
```

- **Color format:** RGB565 big-endian (16-bit: 5R 6G 5B)
- **Max run:** 128 pixels per packet
- **Tinting:** Optional `overrideColor` recolors pixels based on luminance of original green/blue channels

### `stats.json` Schema

See [Section 10](#10-persistence) for full field listing. Key arrays:
- `hourlyPresenceHistoryWeekly[7][24]`: uint8 percent presence per hour per day-of-week
- `presenceMsCurrentDay[24]`: uint32 milliseconds accumulated per hour today
- `historyDaysCountWeekly[7]`: int days of data blended per day-of-week

### `todo.json` Schema

See [Section 10](#10-persistence) for full structure. Key patterns:
- **Daily tasks:** Time-based (hour + minute), one-shot or recurrent (date range)
- **Monthly tasks:** Day-of-month, one-shot or recurrent (month range)
- **Recurrent tracking:** `completedDates[]` / `completedMonths[]` arrays
- **Soft delete (recurrent):** Set `endDate`/`endMonth` instead of removing
