# Bug Report: No AI Triggers Firing — Day Messages Absent

**Date found:** 2026-08-13  
**Introduced in:** Commits `ea32a07` (trigger timing overhaul) + `827dc47` (late-hours sit)  
**Last working build:** Commit `99c3a3d` (localization split)

---

## Overview

After several presence-related refactors, **no automatic AI messages fire during normal daytime work sessions.** Manual debug triggers (`TRIGGER 2 1` via MQTT) still work. Only `TASK_DUE` (event 12) fires automatically. The overnight log shows 51 phantom presence registrations (sessions 28–79) between 00:14 and 07:00.

---

## Bug 1 — `WELCOME_BACK` Never Fires (Primary Bug)

### Location
[main.cpp:L1301 and L1329](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1301)

### What the code does
```cpp
// L1301 — computed BEFORE lastStateTransitionTime is updated
unsigned long lastAwaySliceMs = (appState.lastStateTransitionTime > 0 && now >= appState.lastStateTransitionTime)
                                ? (now - appState.lastStateTransitionTime) : 0;
appState.lastStateTransitionTime = now;  // L1304 — updated here

// L1329 — WELCOME_BACK gate uses lastAwaySliceMs
if (appConfig.aiMode >= 1 && lastAwaySliceMs >= BREAK_MINIMUM_MS) {
    messageManager.scheduleGreetingMessage(EVENT_WELCOME_BACK, tempBreakDuration);
}
```

### Why it breaks
`lastStateTransitionTime` is updated on **every** presence state change — including phantom radar blips from noise while the chair is empty at night. If a phantom blip (2–10 seconds long) fires 30–90 seconds before the user actually sits down, `lastStateTransitionTime` is set to that blip's timestamp. When the user sits:

```
lastAwaySliceMs = now - lastStateTransitionTime  <- seconds since the PHANTOM, not since the user left
```

This evaluates to 30–120 seconds, which is less than `BREAK_MINIMUM_MS` (180,000 ms = 3 minutes). **`WELCOME_BACK` is silently skipped every time.**

### What should be checked instead
`appState.currentBreakDurationMs` is **already correctly computed** at L1208–1222 using `sitDownEpoch - referenceAwayEpoch` (NTP timestamps, immune to phantom blips). This is the right variable. The problem is that `resetSessionStats()` (called at L1295) zeroes it before L1329 is reached, so the developer created `lastAwaySliceMs` as a workaround — but the workaround is broken.

### Fix
Save `currentBreakDurationMs` to a local variable **before** calling `resetSessionStats()`, then use that local for the `WELCOME_BACK` gate at L1329.

```diff
-      unsigned long lastAwaySliceMs = (appState.lastStateTransitionTime > 0 && now >= appState.lastStateTransitionTime)
-                                      ? (now - appState.lastStateTransitionTime) : 0;
+      // Capture break duration before resetSessionStats() clears it
+      unsigned long breakDurationMsAtSit = appState.currentBreakDurationMs;
       appState.currentPresenceState = targetState;
       ...
-      if (appConfig.aiMode >= 1 && lastAwaySliceMs >= BREAK_MINIMUM_MS) {
+      if (appConfig.aiMode >= 1 && breakDurationMsAtSit >= BREAK_MINIMUM_MS) {
```

> NOTE: `currentBreakDurationMs` is already logged correctly (e.g. `break=766 s` in the log). It just needs to be preserved into a local before `resetSessionStats()` is called at L1295.

---

## Bug 2 — Seated Timer Resets on Brief Dropouts (Cascade Bug)

### Location
[main.cpp:L1202–L1306](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1202)

### What the code does
On **every** `Away -> Present` transition — whether it's a real sit-down or a 2-second radar recovery blip — the following are unconditionally reset:

```cpp
appState.lastNagTime = now;           // L1202
appState.lastPointsCadenceTime = now; // L1203
...
appState.continuousPresenceStart = now; // L1305
appState.lastStretchReminderTime = now; // L1306
```

### Why it breaks
Brief radar dropouts — where radar loses the user for 2–10 seconds and then reacquires — cause a `stablePresence: false -> true` transition (since the OFF debounce is 10s, the recovery usually passes the 2s ON debounce). This triggers a full `Away -> Present` transition block, wiping every seated timer back to zero.

This means that if a brief dropout occurs once every 10–20 minutes, the user's seated timers **never accumulate past the point of the last dropout**. Since all periodic triggers (`STRETCH` 60m, `NAGGING` 60m, `POINTS` 18m, `CURATION` 50m) check `now - lastXTime >= interval`, they can never fire.

### Log evidence
From the overnight log, sessions like 14, 15, 16, 17 show 4–12 second breaks being processed as `Away -> Present` transitions, resetting `lastNagTime` and `lastPointsCadenceTime` each time.

### Fix
Check whether the break duration was meaningful before resetting seated timers. If `currentBreakDurationMs < BREAK_MINIMUM_MS` (brief return), **preserve** the seated cadence timers:

```diff
+      unsigned long breakDurationMsAtSit = appState.currentBreakDurationMs;
       appState.continuousPresenceStart = now;
       appState.lastStretchReminderTime = now;
-      appState.lastNagTime = now;
-      appState.lastPointsCadenceTime = now;
+      // Only reset seated cadence timers on real breaks (>= 3 min)
+      if (breakDurationMsAtSit >= BREAK_MINIMUM_MS) {
+        appState.lastNagTime = now;
+        appState.lastPointsCadenceTime = now;
+      }
```

> IMPORTANT: `continuousPresenceStart` and `lastStretchReminderTime` should still reset on any return (real or brief) since they measure uninterrupted sitting time. Only `lastNagTime` and `lastPointsCadenceTime` need the guard, as they are session-level cadences that should survive brief interruptions.

---

## Bug 3 — Workday Crossover Burns `firstSitToday` on Phantom Blip

### Location
[main.cpp:L1396–L1415](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1396)

### What the code does
Every loop tick while `stablePresence == true`:

```cpp
if (appStats.firstSitToday && appState.wasFirstSitThisSession &&
    appState.currentPresenceState != STATE_AWAY && !isLateHoursNow()) {
  appStats.firstSitToday = false;          // Burns the flag
  ...
  messageManager.scheduleGreetingMessage(EVENT_FIRST_SIT, breakStr);
}
```

### Why it breaks
At the workday boundary (e.g. 6:30 AM when `isLateHoursNow()` transitions from `true` to `false`), if a phantom radar blip is active for any duration, the condition `stablePresence == true && !isLateHoursNow()` becomes true. `firstSitToday` is immediately burned, `EVENT_FIRST_SIT` fires to an empty room, and `firstSitToday` stays `false` for the rest of the day. The real first sit later gets routed to `WELCOME_BACK` — which then fails due to Bug 1.

### Fix
Add a minimum continuous presence threshold before burning the crossover. The user must have been present for at least `DEBOUNCE_PRESENCE_OVERNIGHT_MS` (5s) during work hours, not just a radar blip:

```diff
  if (appStats.firstSitToday && appState.wasFirstSitThisSession &&
-     appState.currentPresenceState != STATE_AWAY && !isLateHoursNow()) {
+     appState.currentPresenceState != STATE_AWAY && !isLateHoursNow() &&
+     (now - appState.continuousPresenceStart) >= DEBOUNCE_PRESENCE_OVERNIGHT_MS) {
```

> NOTE: The early crossover check at L1419 already uses `CROSSOVER_THRESHOLD_MS` (15 min) correctly. This fix brings the boundary-crossing burn in line with the same principle: require stable presence before committing.

---

## Impact Summary

| Bug | Triggers affected | Root cause |
|-----|-----------------|------------|
| **Bug 1** | `WELCOME_BACK` never fires | `lastAwaySliceMs` corrupted by phantom blips; `currentBreakDurationMs` wiped before the check |
| **Bug 2** | `NAGGING`, `POINTS`, `STRETCH`, `CURATION` never reach threshold | Seated cadence timers reset on every brief dropout |
| **Bug 3** | `FIRST_SIT` fires to empty room at workday boundary | Workday crossover burns `firstSitToday` on phantom blip |

---

## Files to Change

| File | Line(s) | Change |
|------|---------|--------|
| [main.cpp L1301](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1301) | L1301 | Replace `lastAwaySliceMs` local with `breakDurationMsAtSit = appState.currentBreakDurationMs` (captured before reset) |
| [main.cpp L1329](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1329) | L1329 | Change gate from `lastAwaySliceMs >= BREAK_MINIMUM_MS` to `breakDurationMsAtSit >= BREAK_MINIMUM_MS` |
| [main.cpp L1202](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1202) | L1202–1203 | Guard `lastNagTime` and `lastPointsCadenceTime` reset behind `breakDurationMsAtSit >= BREAK_MINIMUM_MS` |
| [main.cpp L1396](file:///c:/Users/danme/Documents/PlatformIO/Projects/DeskBuddy/src/main.cpp#L1396) | L1396 | Add `(now - continuousPresenceStart) >= DEBOUNCE_PRESENCE_OVERNIGHT_MS` to workday crossover burn condition |
