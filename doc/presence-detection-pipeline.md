# Presence Detection Pipeline

Complete 10-layer presence detection pipeline across a typical day. Every layer runs on each main loop tick (~10ms).

---

## Layer 1: Radar Input — every 10ms

```
LD2410 (Serial1, 256k baud)
  → sensorPresenceDetected (bool)
  → sensorStaticPresenceDetected (bool)
  → movingTargetDetected (bool)
  → detectionDistance (uint16, cm)
```

Raw values read from the HLK-LD2410 mmWave radar chip on every tick. No processing yet. Simulation mode (`SIM` MQTT command) injects synthetic values bypassing the physical sensor.

---

## Layer 2: Rolling Median Filters — every 100ms (10Hz)

| Filter | Window | What it smooths |
|--------|--------|-----------------|
| Motion filter | Median of 10 samples (`MOTION_FILTER_SIZE`) | `movingTargetDetected` → `filteredMovingTarget` |
| Distance filter | Median of N samples (N = `filterWindow` × 10) | `detectionDistance` → `filteredDetectionDist` |

Motion threshold: values < 0.5 (`FILTER_MOTION_THRESHOLD`) are clamped to zero (noise rejection).

Distance uses a rolling median buffer with a configurable window (default 2.0s → 20 samples). Motion uses a smaller fixed window (10 samples ≈ 1s) for faster response.

Session stats accumulate in parallel: `sessionDistanceSum`, `sessionDistanceCount`, `sessionDistanceAverage`.

---

## Layer 3: Motion Ratio — 1-second rolling buckets, 180s window

```
Every second: advance bucket head, zero new bucket
Each loop:    add elapsed ms to current desk bucket (+ motion bucket if moving)

recentMotionRatio = sum(motionBuckets[window]) / sum(deskBuckets[window]) × 100
```

Configurable window size via `appConfig.motionWindow` (1-180s). Each bucket captures 1 second. The 180s rolling window (`RECENT_MOTION_WINDOW_S`) means the ratio reflects ~3 minutes of activity.

This is the "are you fidgeting" metric. Used at Layer 4 to distinguish BUSY (active) from FOCUS (still).

---

## Layer 4: Raw State Classification — every loop

```
if !sensorPresenceDetected OR distance > deskDistanceLimit:
    rawState = STATE_AWAY

elif distance < focusDistanceLimit (default 50cm):
    rawState = motionRatio > motionRatioLimit (default 15%) ? STATE_BUSY : STATE_FOCUS

else:
    rawState = STATE_REGULAR
```

The 5 raw states:

| ID | Name | Trigger |
|----|------|---------|
| 0 | `STATE_AWAY` | No presence, or distance > deskDistanceLimit |
| 1 | `STATE_FOCUS` | In focus zone + low motion |
| 2 | `STATE_BUSY` | In focus zone + high motion |
| 3 | `STATE_DISTRACTED` | Far from desk for 5+ min (applied at Layer 6) |
| 4 | `STATE_REGULAR` | Beyond focus zone + any motion level |

---

## Layer 5: Presence Debouncing

| Direction | Debounce | Why |
|-----------|----------|-----|
| Away → Present (normal) | 2s (`DEBOUNCE_PRESENCE_MS`) | Ignore sensor blips |
| Away → Present (first sit today) | 5s (`DEBOUNCE_PRESENCE_OVERNIGHT_MS`) | Extra settle-in time overnight |
| Present → Away | 10s (`DEBOUNCE_AWAY_MS`) | Don't count short walks as breaks |

`rawPresent` must persist unchanged for the debounce window before `stablePresence` flips. The overnight variant prevents the overnight first-sit greeting from firing on a phantom detection.

---

## Layer 6: Distracted Mood — 5-minute far-from-desk timer

```
if stablePresence AND distance >= focusDistanceLimit:
    start farFromDeskSince timer
    if timer >= 5 minutes (DISTRACTED_FAR_MIN_MS):
        OVERRIDE rawState → STATE_DISTRACTED
else:
    reset farFromDeskSince timer
```

This is the "you've been leaning back for a while" detector. Motion plays no role on the far side — it's purely distance-based. Resets instantly when the user leans back into the focus zone or leaves.

---

## Layer 7: Sticky Confirmation — 30-second sub-state lock

```
if rawState != currentPresenceState AND rawState != STATE_AWAY:
    candidate = rawState, start 30s timer
    if candidate persists for 30s (STICKY_CONFIRM_MS):
        commit transition to new state
        if leaving FOCUS → BUSY or REGULAR:
            fire EVENT_FOCUS_END (celebrated in-place, never on leaving)
```

Prevents rapid toggling between FOCUS/BUSY/REGULAR/DISTRACTED from sensor boundary jitter. The 30s window means you must sit still (or fidget) for half a minute before the state officially changes.

FOCUS_END is only fired on in-place transitions (FOCUS→BUSY or FOCUS→REGULAR while still seated). Leaving the desk while in FOCUS does NOT trigger it — focus sessions are celebrated at their natural end.

---

## Layer 8: Session Lifecycle — Away ↔ Present

### Away → Present (sit-down)

```
set sitDownTime, sitDownEpoch (epoch time)
set continuousPresenceStart, lastStretchReminderTime = now
set lastNagTime, lastPointsCadenceTime = now
increment currentSitDownSessionId (for stale AI query rejection)
reset session stats (resetSessionStats)
calculate currentBreakDurationMs from lastAwayEpoch → sitDownEpoch
```

Triggers on arrival:
- **FIRST_SIT**: if `firstSitToday == true` and NOT late hours → day-start journey
- **WELCOME_BACK**: if break ≥ 3 min and NOT first sit → return from break
- **LATEHOURS_SIT**: if late hours and break ≥ 3 min → quiet-hours greeting
- **EXCESSIVE_BREAKS**: if break rate > 1/hr after 3h worked → queued behind greeting

### Present → Away (leave desk)

| Condition | Classification | Action |
|-----------|---------------|--------|
| `isStopByTracking` AND < 8 min AND late hours AND `wasFirstSitThisSession` | **First-sit stop-by** | Roll back `firstSitToday`, restore `lastAwayEpoch`, clear `heldFirstSitEpoch`. Discard entire session. |
| `isStopByTracking` AND < 8 min AND late hours | **Standard stop-by** | Roll back `breakCount`, restore `previousLatestBreakDuration`. Don't count this as a break. |
| Otherwise | **Real session** | Set `lastAwayEpoch`, increment `breakCount` if ≥ 3 min away, save stats. |

**Stop-by threshold** (`STOP_BY_THRESHOLD_MS`): 8 minutes — a brief late-hours visit that shouldn't count as a real work session or break.

---

## Layer 9: Day Rollover — midnight or 4+ hour absence

```
if referenceAwayEpoch AND sitDownEpoch differ by > 4 hours (OVERNIGHT_THRESHOLD_S)
   OR NTP day changed AND absence spanned midnight:
    mergeCurrentDayPresence()  → blend today's hourly presence into 7-day weekly history
    resetDailyStats()          → zero daily counters, persist to stats.json
```

This is the "new day" detector. The 4-hour overnight gap triggers it even without NTP sync.

Blending formula: `hourlyPresenceHistoryWeekly[day][h] = (history × 3 + todayPct × 2) / 5` — 60% historical weight, 40% today's data.

---

## Layer 10: Late Hours — outside [learnedStart − 30min, learnedEnd + 30min]

```
isLateHoursNow():
    workdayStart = learnedStart × 60 − LATEHOURS_PADDING_MS / 60000  (30 min before learned start)
    workdayEnd   = learnedEnd × 60 + LATEHOURS_PADDING_MS / 60000    (30 min after learned end)
    return currentMinutes < workdayStart OR currentMinutes >= workdayEnd
```

All three learned values come from `hourlyPresenceHistoryWeekly`:
- **Workday start**: first hour ≥ 15% presence between 4AM–12PM (fallback: 8 AM)
- **Workday end**: last hour ≥ 15% presence from 4PM onwards (fallback: 6 PM)
- **Lunch hour**: lowest-presence hour between 11AM–2PM (fallback: noon)

### Late-hours behavior cascade

| Trigger | When | Action |
|---------|------|--------|
| **Hold** | Any late-hours sit-down | Save `heldFirstSitEpoch` (first sit only). Fire `EVENT_LATEHOURS_SIT`. Reset session stats but PRESERVE `firstSitToday = true`. |
| **Crossover** | 15 min stable continuous presence in late hours (`CROSSOVER_THRESHOLD_MS`) | **Burn flag early.** Fire `EVENT_FIRST_SIT` with proper overnight break. Day officially starts at `heldFirstSitEpoch`. |
| **Boundary** | Learned boundary crossed while seated with flag still held | **Burn flag.** Fire `EVENT_FIRST_SIT` (safety net for cases where crossover never triggered). |
| **Stop-by** | Leave within 8 min (`STOP_BY_THRESHOLD_MS`) | **Roll back** — restore `firstSitToday = true`, clear `heldFirstSitEpoch`. Pretend this never happened. |

---

## Continuous Presence Tracking

`continuousPresenceStart` is set on every Away→Present transition. It drives:

| Trigger | Timer |
|---------|-------|
| STRETCH | 60 min |
| SLACKER | 1h15m |
| STREAK_BEATEN | 15 min record |
| CURATION | 50 min / 120 min (Normal), 40 min / 60 min (Chatty) |
| CROSSOVER | 15 min |
| NAGGING | 60 min (Normal), 37 min (Chatty) |

---

## Full-Day Trace

```
╔══════╦═══════════╦══════════════════════════════════════════════════════╗
║ Time ║  Event    ║  Pipeline processing                                ║
╠══════╬═══════════╬══════════════════════════════════════════════════════╣
║ T+0  ║ Radar     ║ L1: detect presence, d=35cm, moving=false          ║
║ T+0  ║ Raw       ║ L4: inFocusZone=true, highMotion=false → FOCUS     ║
║ T+0  ║ Debounce  ║ L5: rawPresent changed, start 5s overnight timer   ║
║ T+5s ║ Stable    ║ L5: debounce expires, stablePresence=true           ║
║ T+5s ║ Session   ║ L8: Away→Present. sitDownTime, sessionId++         ║
║      ║           ║     L10: lateHours → hold flag, LATEHOURS_SIT      ║
║      ║           ║     heldFirstSitEpoch saved. Reset session stats.  ║
║      ║ Metrics   ║ L8: accumulatePresence(hour, elapsed)               ║
║      ║           ║     totalDeskTime += elapsed                        ║
║ T+15 ║ Crossover ║ L10: 15min stable → burn flag, FIRST_SIT fires     ║
║      ║           ║     Day officially starts at heldFirstSitEpoch      ║
║ T+20 ║ State     ║ L4: motionRatio drops below 15% → rawState=FOCUS   ║
║      ║           ║ L7: 30s sticky starts for BUSY→FOCUS transition    ║
║ T+50 ║ State     ║ L7: sticky expires → committed to FOCUS            ║
║      ║           ║     continuousStillStart = 30s ago (conf window)    ║
║ T+60 ║ Stretch   ║ 60min continuous → STRETCH fires                    ║
║ T+90 ║ Leave     ║ L5: rawPresent=false, start 10s away debounce       ║
║ T+100║ Away      ║ L5: debounce expires → stablePresence=false        ║
║      ║           ║ L8: Present→Away. 90min session > 8min.            ║
║      ║           ║     Real session. breakCount++. lastAwayEpoch set. ║
║      ║           ║ L7: leaving FOCUS → no FOCUS_END (in-place only)   ║
║ T+200║ Return    ║ L5: rawPresent=true, start 2s debounce              ║
║ T+202║ Stable    ║ L5: debounce expires                                ║
║ T+202║ Session   ║ L8: Away→Present. breakDuration=100s.              ║
║      ║           ║     breakCount++ (≥3min break). WELCOME_BACK fires ║
║      ║           ║     continuousPresenceStart reset.                  ║
║ T+240║ Far       ║ L6: distance > focusLimit, start farFromDesk timer ║
║ T+300║ Distracted║ L6: 5min far → override to STATE_DISTRACTED        ║
║ T+360║ Leave     ║ L5+L8: 10s debounce → Real session completed       ║
║ T+365║ Day end   ║ L8: lastAwayEpoch saved.                           ║
║      ║           ║ L9: next morning, absent 4+ hrs → day rollover     ║
╚══════╩═══════════╩══════════════════════════════════════════════════════╝
```

---

## State Transition Diagram

```
                    ┌─────────────────────────────────────┐
                    │            STICKY 30s               │
                    │   ┌──────────┐    ┌──────────┐      │
                    │   │  FOCUS   │◄──►│   BUSY   │      │
                    │   │ (< 50cm, │    │ (< 50cm, │      │
                    │   │ low mot) │    │high mot) │      │
                    │   └────┬─────┘    └────┬─────┘      │
                    │        │               │             │
                    │        │   ┌───────────┘             │
                    │        ▼   ▼                         │
        AWAY ◄────► │   ┌──────────┐    ┌──────────────┐  │
    debounce:       │   │ REGULAR  │◄──►│  DISTRACTED  │  │
    2s on / 10s off │   │ (far,    │    │  (far, 5min  │  │
                    │   │ low mot) │    │   detach)    │  │
                    │   └──────────┘    └──────────────┘  │
                    └─────────────────────────────────────┘

AWAY ↔ PRESENT: session lifecycle (L8)
↔ within PRESENT: sticky 30s (L7)
REGULAR → DISTRACTED: 5min far-from-desk (L6)
```

---

## Productivity Score Formula

```
raw = 100 − breakPenalty − timePenalty + focusBonus

breakPenalty = 25 × (breakCount / hoursElapsed) / 1.0         # target: 1 break/hr
timePenalty  = 25 × (breakRatio / 0.10)                       # target: 10% break time
focusBonus   = 1.5 × (focusTime / deskTime × 100)            # 1.5× multiplier

score = constrain(raw, 0, 100)
# First 5 minutes (SCORE_INITIAL_PERIOD_S): defaults to 100%
```

---

## Key Constants Reference

| Constant | Value | Purpose |
|----------|-------|---------|
| `FILTER_MOTION_THRESHOLD` | 0.5 | Motion noise floor |
| `RECENT_MOTION_WINDOW_S` | 180 | Rolling window for motion ratio |
| `FOCUS_DISTANCE_LIMIT_DEFAULT` | 50 | cm threshold for focus zone |
| `MOTION_RATIO_LIMIT_DEFAULT` | 15 | % motion threshold for BUSY |
| `DISTANCE_LIMIT_DEFAULT` | 120 | cm max presence distance |
| `FILTER_WINDOW_DEFAULT` | 2.0 | seconds for distance median window |
| `DEBOUNCE_PRESENCE_MS` | 2000 | 2s presence ON debounce |
| `DEBOUNCE_PRESENCE_OVERNIGHT_MS` | 5000 | 5s first-sit presence ON debounce |
| `DEBOUNCE_AWAY_MS` | 10000 | 10s presence OFF debounce |
| `STICKY_CONFIRM_MS` | 30000 | 30s state transition lock |
| `DISTRACTED_FAR_MIN_MS` | 300000 | 5min before DISTRACTED state |
| `BREAK_MINIMUM_MS` | 180000 | 3min minimum absence to count as break |
| `STOP_BY_THRESHOLD_MS` | 480000 | 8min late-hours mini-session threshold |
| `STREAK_MINIMUM_MS` | 900000 | 15min minimum sitting streak |
| `FOCUS_MINIMUM_MS` | 300000 | 5min minimum focus session |
| `OVERNIGHT_THRESHOLD_S` | 14400 | 4h absence triggers day rollover |
| `LATEHOURS_PADDING_MS` | 1800000 | 30min padding on learned workday boundaries |
| `CROSSOVER_THRESHOLD_MS` | 900000 | 15min stable late-hours sit → early flag burn |
| `SCORE_INITIAL_PERIOD_S` | 300 | 5min initial score override to 100% |
