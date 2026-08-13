# DeskBuddy Presence Detection Variables & Timers Reference

Complete reference of all firmware variables, constants, flags, and timers involved in radar sampling, presence state classification, session lifecycle management, and behavior triggers.

---

## 1. Radar Sampling & Filtering Variables

| Variable / Function | Type | Description | Reset / Update Cycle |
|---------------------|------|-------------|----------------------|
| `sensorPresenceDetected` | `bool` | Raw presence detection flag read directly from the HLK-LD2410 radar (`radar.presenceDetected()`). | Updated every main loop tick (~10ms). |
| `rawDetectionDist` | `uint16_t` | Raw detection distance (cm) read from LD2410 (`radar.detectionDistance()`). Set to `0` when no target is detected. | Updated every main loop tick (~10ms). Cleared to `0` when `AWAY`. |
| `filteredDetectionDist` | `float` | Rolling median distance computed over `filterWindow` (default 2.0s = 20 samples). | Updated every 100ms (`FILTER_UPDATE_MS`). Cleared to `0.0` when `AWAY`. |
| `sensorMovingTargetDetected` | `bool` | Motion state filtered by rolling median (`motionFilter.getMedian() > 0.5`). | Updated every 100ms (`FILTER_UPDATE_MS`). Cleared to `false` when `AWAY`. |
| `rawPresent` | `bool` | Instantaneous raw presence logic: `sensorPresenceDetected && (rawDetectionDist == 0 \|\| rawDetectionDist <= deskDistanceLimit)`. | Evaluated every main loop tick (~10ms). |
| `lastRawPresent` | `bool` | Previous state of `rawPresent` used to detect transition edges for debouncing. | Updated whenever `rawPresent` flips. |
| `lastRawPresentChangeTime` | `unsigned long` | `millis()` timestamp of the last `rawPresent` state change. | Updated on `rawPresent` transition edge. |
| `stablePresence` | `bool` | Debounced presence state. Flips `true` after `DEBOUNCE_PRESENCE_MS` (2s) or `DEBOUNCE_PRESENCE_OVERNIGHT_MS` (5s); flips `false` after `DEBOUNCE_AWAY_MS` (10s). | Updated when `now - lastRawPresentChangeTime >= debounceLimit`. |
| `rawState` | `int` | Unconfirmed raw presence state (`STATE_AWAY`=0, `STATE_FOCUS`=1, `STATE_BUSY`=2, `STATE_DISTRACTED`=3, `STATE_REGULAR`=4). | Evaluated every loop tick based on distance and motion ratio. |
| `currentPresenceState` | `int` | Committed presence state after passing the 30-second sticky confirmation lock (`STICKY_CONFIRM_MS`). | Updated on sticky timer expiration (30s) or instantly on `AWAY`. |
| `recentMotionBuckets[]` | `uint16_t[180]` | 1-second rolling buckets accumulating motion time (ms) per second over the last `RECENT_MOTION_WINDOW_S` (180s) window. | Current bucket incremented each loop tick when moving; head advances every 1s. Cleared to `0` when `AWAY` via `clearRecentMotionWindow()`. |
| `recentDeskBuckets[]` | `uint16_t[180]` | 1-second rolling buckets accumulating total desk presence time (ms) per second over the last 180s window. Denominator for motion ratio. | Same as `recentMotionBuckets`. Cleared to `0` when `AWAY`. |
| `recentBucketHead` | `size_t` | Current write index into `recentMotionBuckets` and `recentDeskBuckets` circular arrays. | Advances every 1 second (`lastBucketAdvanceTime`). Wraps at `RECENT_MOTION_WINDOW_S`. |
| `lastBucketAdvanceTime` | `unsigned long` | `millis()` timestamp tracking when the rolling bucket head last advanced. | Updated every 1000ms to advance the circular buffer. |
| `recentMotionRatio` | `int` | Calculated windowed motion percentage: `sum(motionBuckets) / sum(deskBuckets) * 100` over `appConfig.motionWindow` seconds. | Calculated every loop tick when `rawPresent`. Stored in `appState.recentMotionRatio`. |
| `candidateState` | `int` (static) | Sub-state pending confirmation during 30-second sticky window. `-1` means no candidate pending. | Set when `rawState != currentPresenceState`. Reset to `-1` on confirm or state mismatch. |
| `stateConfirmationTime` | `unsigned long` (static) | `millis()` timestamp when `candidateState` was first observed. Used to evaluate `STICKY_CONFIRM_MS`. | Set when a new `candidateState` begins. |

---

## 2. Session Lifecycle & State Tracking Variables

| Variable | Type | Description | Reset / Update Cycle |
|----------|------|-------------|----------------------|
| `currentSitDownSessionId` | `uint32_t` | Monotonically incrementing session ID counter. Used to reject stale AI responses from previous sessions. | Incremented on **every** `Away -> Present` transition (`currentSitDownSessionId++`). |
| `sitDownTime` | `unsigned long` | `millis()` timestamp when the current sit-down session began. | Set on `Away -> Present` transition. |
| `sitDownEpoch` | `uint32_t` | NTP epoch timestamp when the current sit-down session began. | Set on `Away -> Present` transition. |
| `lastStateTransitionTime` | `unsigned long` | `millis()` timestamp of the last presence state transition (`Away -> Present`, `Present -> Away`, or sticky state commit). | Updated on every state transition. |
| `currentBreakDurationMs` | `unsigned long` | Calculated break duration (ms) at transition using `sitDownEpoch - referenceAwayEpoch` (or `now - lastStateTransitionTime`). | Computed on `Away -> Present`. Wiped to `0` inside `resetSessionStats()`. |
| `lastAwayEpoch` | `uint32_t` | NTP epoch timestamp when the user last left the desk for a real session (>= 8 min or non-stop-by). | Updated on `Present -> Away` real session completion. |
| `originalLastAwayEpoch` | `uint32_t` | Snapshot of `lastAwayEpoch` taken when a candidate stop-by session begins. | Saved on `Away -> Present` if `isStopByTracking` is true. Restored if stop-by is rolled back. |
| `isStopByTracking` | `bool` | Flag indicating if current session is being tracked as a candidate stop-by (< 8 min). | Set to `true` on `Away -> Present`. Reset to `false` on `Present -> Away`. |
| `wasFirstSitThisSession` | `bool` | Tracks if current sit session holds the first sit of the day during late hours. | Set to `true` on late-hours first sit. Reset to `false` on real session completion or rollback. |
| `firstSitToday` | `bool` | Global flag indicating if the first sit of the day has not yet occurred. | Resets to `true` at midnight / day rollover (`resetDailyStats()`). Sets to `false` on first work-hours sit or crossover burn (requires $\ge 5\text{s}$ continuous presence during work hours). |
| `heldFirstSitEpoch` | `uint32_t` | Preserved sit-down epoch of a late-hours first sit held until workday crossing. | Saved on late-hours first sit. Reset to `0` when burned or rolled back. |
| `totalStopByTimeMs` | `unsigned long` | Accumulated duration (ms) of late-hours stop-by visits subtracted from gross break time. | Accumulated on stop-by rollbacks. Reset to `0` on real session start. |
| `breakDurationMsAtSit` | `unsigned long` | Local transition calculation capturing NTP-based away duration (`sitDownEpoch - referenceAwayEpoch`) before `resetSessionStats()` clears `currentBreakDurationMs`. Immune to phantom radar blips. | Captured on `Away -> Present` transition. Used for `WELCOME_BACK` gate and cadence reset guard. |
| `currentSessionState` | `enum PresenceState` | High-level tri-state session classifier: `PRESENCE_AWAY` (never sat today), `PRESENCE_SITTING` (currently present), `PRESENCE_BREAK` (has sat before, currently away). Controls rollover logic and first-sit behavior. | Set to `PRESENCE_SITTING` on `Away -> Present`. Set to `PRESENCE_BREAK` on `Present -> Away`. Reset to `PRESENCE_AWAY` only at day rollover. |
| `aiQuerySessionId` | `uint32_t` | Copy of `currentSitDownSessionId` captured when an AI query is dispatched to the background task. Compared on response receipt to detect stale sessions. | Copied into the AI task at query launch. |

---

## 3. Continuous Sitting Timers & Cadence Markers

| Variable | Type | Description | Reset / Update Cycle |
|----------|------|-------------|----------------------|
| `continuousPresenceStart` | `unsigned long` | `millis()` timestamp of continuous presence without leaving the desk. Drives `STRETCH`, `SLACKER`, `STREAK_BEATEN`, `CURATION`, and `CROSSOVER` triggers. | Reset to `now` on **every** `Away -> Present` transition. |
| `continuousStillStart` | `unsigned long` | `millis()` timestamp when continuous still (`FOCUS`) state started. | Set when entering `STATE_FOCUS` (includes 30s sticky window). Reset on leaving `STATE_FOCUS`. |
| `lastNagTime` | `unsigned long` | `millis()` timestamp tracking the overdue-task nag queue cadence. | Reset to `now` on `Away -> Present` **only if** returning from a real break (`breakDurationMsAtSit >= BREAK_MINIMUM_MS`); preserved across micro-dropouts. Updated every 60 min seated (`NAGGING_TRIGGER_DELAY_MS`, 37 min in Chatty). |
| `lastPointsCadenceTime` | `unsigned long` | `millis()` timestamp tracking the 18-minute seated cadence for points check-in. | Reset to `now` on `Away -> Present` **only if** returning from a real break (`breakDurationMsAtSit >= BREAK_MINIMUM_MS`); preserved across micro-dropouts. Updated every 18 min seated (`POINTS_TRIGGER_DELAY_MS`, 9 min in Chatty). |
| `lastPointsTime` | `unsigned long` | `millis()` timestamp tracking the 3.7-hour throttle marker for points check-in. | Updated when `EVENT_POINTS` fires (`POINTS_THROTTLE_MS` = 13,260,000ms / 221 min, 90 min in Chatty). |
| `lastStretchReminderTime` | `unsigned long` | `millis()` timestamp tracking the 60-minute stretch reminder cadence. | Reset to `now` on `Away -> Present`; updated every 60 min seated (`STRETCH_INTERVAL_MS`). |
| `farFromDeskSince` | `unsigned long` | `millis()` timestamp when user entered far zone (> focus limit) while present. | Set when present and distance > focus limit. Reset to `0` when in focus zone or `AWAY`. |
| `lastLateHoursSitTime` | `unsigned long` | `millis()` timestamp tracking late-hours greeting cooldown (30 min). | Updated when `EVENT_LATEHOURS_SIT` fires (`LATEHOURS_COOLDOWN_MS`). |
| `lastSlackerRoastTime` | `unsigned long` (static) | `millis()` timestamp tracking the slacker roast cooldown. Prevents repeated roasts closer than `SLACKER_INTERVAL_MS`. | Updated when `EVENT_SLACKER` fires. Persists across sessions (static). |
| `lastCurationNudgeTime` | `unsigned long` (static) | `millis()` timestamp tracking the curation nudge throttle. | Updated when `EVENT_CURATION` fires. Persists across sessions (static). |
| `streakAlertTriggered` | `bool` | Prevents `STREAK_BEATEN` from firing more than once per sit session. | Set to `true` when `EVENT_STREAK_BEATEN` fires. Reset to `false` on `Present -> Away`. |
| `systemBusy` | `bool` | Loop-level gate: `true` if AI is loading, a response is pending display, the alert screen is active, or a welcome alert is pending. Prevents dequeuing a new message while one is in-flight. | Evaluated on every loop tick where user is present. |
| `lastHourlyUpdate` | `unsigned long` | `millis()` timestamp tracking when NTP and weather were last refreshed. | Initialized to `millis() - NTP_INTERVAL_MS` at boot to force an immediate first fetch. Updated every `NTP_INTERVAL_MS` (1 hour) or `NTP_RETRY_MS` (15s) before sync. |

---

## 4. Presence Detection & Debounce Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DISTANCE_LIMIT_DEFAULT` | `120` | Default maximum distance threshold (cm) for presence detection. Configurable per radar `max_gate * 20`. |
| `FOCUS_DISTANCE_LIMIT_DEFAULT` | `50` | Distance limit (cm) separating Focus zone (< 50cm) from Regular zone (> 50cm). |
| `MOTION_RATIO_LIMIT_DEFAULT` | `15` | Motion ratio percentage threshold separating Focus state (< 15%) from Busy state (> 15%). |
| `FILTER_MOTION_THRESHOLD` | `0.5f` | Motion filter cutoff threshold (0.0 to 1.0). Values < 0.5 are clamped to zero. |
| `DEBOUNCE_PRESENCE_MS` | `2000UL` | 2-second presence ON debounce window for standard sits. |
| `DEBOUNCE_PRESENCE_OVERNIGHT_MS` | `5000UL` | 5-second presence ON debounce window for overnight first sit. |
| `DEBOUNCE_AWAY_MS` | `10000UL` | 10-second presence OFF debounce window for departures. |
| `STICKY_CONFIRM_MS` | `30000UL` | 30-second sub-state lock window before committing FOCUS/BUSY/REGULAR transition. |
| `DISTRACTED_FAR_MIN_MS` | `300000UL` | 5-minute continuous present-but-far threshold before overriding raw state to `STATE_DISTRACTED`. |

---

## 5. Session Lifecycle & Behavior Timing Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `BREAK_MINIMUM_MS` | `180000UL` | 3-minute minimum away threshold required to count as a break and allow `WELCOME_BACK` or `LATEHOURS_SIT` greetings. |
| `STOP_BY_THRESHOLD_MS` | `480000UL` | 8-minute late-hours sit threshold below which a session is classified as a stop-by and rolled back. |
| `OVERNIGHT_THRESHOLD_S` | `14400UL` | 4-hour absence threshold triggering day rollover and overnight break string formatting. |
| `CROSSOVER_THRESHOLD_MS` | `900000UL` | 15-minute continuous sit during late hours required to trigger early day-start crossover burn. |
| `LATEHOURS_PADDING_MS` | `1800000UL` | 30-minute padding added to learned workday boundaries for quiet-hours classification. |
| `LATEHOURS_COOLDOWN_MS` | `1800000UL` | 30-minute minimum cooldown between consecutive late-hours sit greetings. |
| `MORNING_JOURNAL_DELAY_MS` | `300000UL` | 5-minute continuous sitting delay after sit-down before morning kickoff journal fires. |
| `STRETCH_INTERVAL_MS` | `3600000UL` | 60-minute continuous sitting interval between stretch reminders. |
| `SLACKER_INTERVAL_MS` | `4500000UL` | 1h15m continuous sitting interval for slacker roast (when score < 35%). |
| `STREAK_MINIMUM_MS` | `900000UL` | 15-minute minimum sitting streak required before `STREAK_BEATEN` can fire. |
| `FOCUS_MINIMUM_MS` | `300000UL` | 5-minute minimum focus session duration required before `FOCUS_END` congrats can fire. |
| `NAGGING_TRIGGER_DELAY_MS` | `3600000UL` | 60-minute seated cadence for overdue-task nag queue (37 min in Chatty mode `CHATTY_NAGGING_TRIGGER_DELAY_MS`). |
| `POINTS_TRIGGER_DELAY_MS` | `1080000UL` | 18-minute seated cadence for points check-in (9 min in Chatty mode `CHATTY_POINTS_TRIGGER_DELAY_MS`). |
| `POINTS_THROTTLE_MS` | `13260000UL` | 3.7-hour (221 min) cooldown throttle between points check-ins (90 min in Chatty mode `CHATTY_POINTS_THROTTLE_MS`). |
| `CURATION_TRIGGER_INTERVAL_MS` | `3000000UL` | 50-minute continuous sitting interval for curation nudge (40 min in Chatty `CHATTY_CURATION_TRIGGER_INTERVAL_MS`). |
| `CURATION_THROTTLE_MS` | `7200000UL` | 120-minute cooldown throttle between curation nudges (60 min in Chatty `CHATTY_CURATION_THROTTLE_MS`). |
