# DeskBuddy Behaviour & Prompt Assessment

Trigger registry, interaction map, prompt-quality findings, and fix log for the DeskBuddy behaviour system. Written 2026-07-31. Fixes F2-F7 implemented alongside this document.

---

## 1. Trigger Registry (13 events)

| ID | Event | Fire site | Condition | Detail payload | Routing |
|----|-------|-----------|-----------|----------------|---------|
| 0 | `EVENT_FIRST_SIT` | `main.cpp:1036` | First sit of the day (rollover or `firstSitToday`) | overnight break string (empty if < 4h) | MM `scheduleFirstSitMessage` (P_URGENT, WELCOME_DELAY_MS, R_IMPORTANT) |
| 1 | `EVENT_WELCOME_BACK` | `main.cpp:1058` | Away >= BREAK_MINIMUM_MS (3m), always (even when excessive-breaks roast also fires) | break duration string | MM `scheduleWelcomeBackMessage` (P_URGENT, WELCOME_DELAY_MS, R_IMPORTANT) -- outranks EXCESSIVE_BREAKS (F8) |
| 2 | `EVENT_STRETCH` | `main.cpp:1094` | `now - lastStretchReminderTime > STRETCH_INTERVAL_MS` (45m) | none | direct `triggerBehaviour` (was) -> MM P_NORMAL/0/R_NORMAL (F4) |
| 3 | `EVENT_FOCUS_END` | `main.cpp:1130` | Present->Away while STATE_FOCUS, focus >= FOCUS_MINIMUM_MS (15s) | focus duration string | direct `triggerBehaviour` (was) -> MM P_NORMAL/0/R_NORMAL (F4) |
| 4 | `EVENT_SLACKER` | `main.cpp:1104` | Sitting > SLACKER_INTERVAL_MS (1h) AND score < 35, throttled 1h | none | direct `triggerBehaviour` (was) -> MM P_NORMAL/0/R_NORMAL (F4) |
| 5 | `EVENT_STREAK_BEATEN` | `main.cpp:1113` | `currentStreak > longestSittingStreak` (both >= 15m), once per session | new record string (F3) | direct `triggerBehaviour` (was) -> MM P_NORMAL/0/R_NORMAL (F4) |
| 6 | `EVENT_LUNCH_REMINDER` | `main.cpp:1343` | Hour == learned lunch AND min >= 15, desk > 30m, once/day | none | direct `triggerBehaviour` (was) -> MM P_NORMAL/0/R_NORMAL (F4) |
| ~~7~~ | ~~`EVENT_MQTT_MESSAGE`~~ | ~~`MqttService.h:65`~~ | ~~Payload on `deskbuddy/display` or `deskbuddy/message`~~ | ~~raw payload~~ | **REMOVED (F7)** -- MQTT display-alert feature deleted; debug triggers now use `TRIGGER` via `deskbuddy/debug/cmd` |
| 8 | `EVENT_EXCESSIVE_BREAKS` | `main.cpp:1048` | Break rate > 1/hr after 0.5h worked, once/day | break duration string | MM `scheduleMessageWithPriority` (P_NORMAL, WELCOME_DELAY_MS, R_IMPORTANT) -- fires *after* the welcome greeting, never replaces it (F8) |
| 9 | `EVENT_GOAL_COMPLETED` | `main.cpp:1354` | `totalDeskTime >= targetHours`, once/day | none | MM (P_HIGH, R_IMPORTANT) |
| 10 | `EVENT_JOURNAL` | `main.cpp:1368/1392/1417` | Morning (5m sit) / pre-lunch (-15m) / end-of-day (-1h), once each | "" -> pages generated | MM (P_HIGH); `triggerBehaviour` returns early (local render) |
| 11 | `EVENT_NAGGING` | `main.cpp:1460` | 2h sitting AND tasks overdue 3+ days, once/day | overdue task names (pipe-delimited) | MM (P_HIGH) |
| 12 | `EVENT_TASK_DUE` | `main.cpp:606` | Active, uncompleted daily task matches `hh:mm` exactly | pipe-delimited task names | direct `triggerBehaviour` (returns early, local render) |

MM = MessageManager. Priorities as of F4: 0/2/4/5/6 all `P_NORMAL, delay 0, R_NORMAL`.

### Trigger interaction map

```
radar -> rawPresent/rawState -> presence state machine (debounce 2s/10s, sticky 30s)
  -> AWAY->PRESENT: FIRST_SIT | WELCOME_BACK | EXCESSIVE_BREAKS  (via MM)
  -> PRESENT:       STRETCH | SLACKER | STREAK_BEATEN            (via MM, F4)
  -> PRESENT->AWAY: FOCUS_END                                    (via MM, F4)
loop polls:         LUNCH | GOAL | JOURNAL x3 | NAGGING | TASK_DUE
MQTT debug/cmd:     TRIGGER <event> [ai|fallback] [detail]  (F7, replaces MQTT display alerts)

All paths converge on triggerBehaviour() (AI.h:277) ->
  EVENT_JOURNAL/TASK_DUE  -> local render, return
  useAI?  (aiMode, dailyCap=30, WiFi)
     YES -> build prompt -> FreeRTOS aiQueryTask -> Groq llama-3.3-70b
            fail or busy -> local quote fallback (F4b)
     NO  -> random local quote from persona array
```

---

## 2. Per-Trigger Findings

- **T1 -- Nudge-storm cadence.** STRETCH fires unconditionally every 45m of continuous presence; SLACKER can fire every 1h; LUNCH once/day. With a busy AI or display they queue behind higher priority and can burst after a gap (e.g. back from away). MM relevance windows (30m normal) bound how stale a queued nudge can be. Acceptable; annoyance budget deferred (P3).
- **T2 -- TASK_DUE has no catch-up.** `checkDueTasks` matches exact `hour==tHour && min==tMin` (`main.cpp:597`). If the user is away or the loop missed the minute, the task is never announced. Would need a "due and not yet fired today" sweep. Deferred.
- **T3 -- NAGGING wall-of-text.** `getHighlyOverdueTaskNames` concatenates ALL overdue daily+monthly tasks with ", " — unbounded length. A long list overflows the 90-char AI budget and the screen. `Curation.h` has a hardcoded date `2026-07-18` in `getTodoObservations` (potential stale debug value).
- **T4 -- One-shot windows missed while away.** LUNCH, pre-lunch journal, and end-of-day journal all gate on `currentPresenceState != STATE_AWAY`; the flag is consumed only while present. The window passes silently if the user is away at that hour.
- **T5 -- FOCUS_END fires only on leave.** A FOCUS->BUSY->leave sequence produces no congrats (only FOCUS->leave at the moment of away does). Focus time that ends in BUSY is never celebrated.

## 3. Prompt & Quote Inventory

- 12 prompt templates (`PROMPT_*`) + 4 preambles + 4 banned-phrase blocks in `Behaviour.h`.
- 12 quote categories x 4 personas x 5 quotes = 240 local quotes.
- Personas: 0=Coach, 1=Critic, 2=Sweet, 3=Friend.
- Flash cost: ~14.4 KB for quote arrays, ~5 KB for prompts/preambles (19.4 KB total string literals).

### Prompt-quality findings

- **Q1 -- Anti-repetition instruction is a no-op.** All preambles demand "never reuse the same opener/verb/structure". The Groq API is stateless per call (no conversation/history sent), so the model cannot honor this. The only cross-call state is `bannedCounter` for persona banned phrases.
- **Q2 -- No output-length enforcement.** The constraint says 75-85 chars (max 90), but nothing clamps the response. `AI_RESPONSE_MAX_CHARS = 90` is only used for `setChatMaxTokens` (tokens, not chars). A 200-char reply renders fully (wrap + scroll). Truncation/deferred.
- **Q3 -- Constraint stacking.** WELCOME_BACK demands the literal `{detail}` be included; SLACKER demands `{score}` be stated. Combined with the length ceiling, the model often has to drop persona color. Both placeholders now resolve (F2), so templates are internally consistent.
- **Q4 -- "the user" substitution risk.** `resolvePromptPlaceholders` randomly swaps `{name}` for "the user" (`AI.h:106`) in templates like "Task '{detail}' is due now. Notify {name}..." -> "Notify the user..." Fine for the LLM; it never reaches the display (only the prompt). Low risk.
- **Q5 -- Voice parity gap.** PROMPT_NAGGING bans "Nudge:" yet `localNagging` and `localTaskDue` quote arrays use "Nudge:" ("Nudge: '{detail}'...", "Nudge: ...is scheduled now"). The AI and local fallback voices diverge for the same event.
- **Q6 -- English hardcoded + broken lang param.** All 240 quotes + 12 prompts are English. The weather URL uses `&leng=fr` (typo -> language ignored, F5 fixed to `&lang=fr`); even with the fix, OpenWeather descriptions stay English while the device targets a Spanish-speaking user (UTC-3 Argentina).
- **Q7 -- No system-role separation.** The whole instruction block is one user-message; there is no `system` role with stable rules and a `user` role with per-event content. The persona preamble must be re-read and re-negotiated every call.

## 4. Defects (severity / status)

| ID | Defect | Sev | Status |
|----|--------|-----|--------|
| D1 | MQTT messages render empty/garbage through `triggerBehaviour` (EVENT_MQTT_MESSAGE not handled -> default local quote overwrites payload; "TODO" substring hack in `Display.h:272`) | High | **RESOLVED (F7)** -- MQTT display-alert feature removed entirely |
| D2 | `{score}` never resolved for AI prompts (`resolvePromptPlaceholders` only handles `{name}`/`{detail}`) | Med | **FIXED (F2)** |
| D3 | Streak detail passes the *old* record, not the new one (`main.cpp:1113`) | Med | **FIXED (F3)** |
| D4 | Direct triggers (stretch/slacker/streak/focus/lunch) silently dropped while AI busy | Med | **FIXED (F4 + F4b)** |
| D5 | Dead code / doc drift (unreachable switch cases, 7 unused MM methods, docs say 15-cap/Gemini/3m-sticky/wrong whitelist) | Low | **FIXED (F6)** |
| D6 | Weather URL `&leng=fr` -> `&lang=fr` | Low | **FIXED (F5)** |
| D7 | Micro-break double penalty (break ratio uses elapsed-vs-desk while `breakCount` also penalized; sub-3m breaks not credited to desk time) | Low | Kept as-is by decision |
| D8 | Focus time lag: `totalFocusTime` accumulates on a 30s-sticky state, so a <30s focus blip inside a session isn't credited | Low | Deferred |
| D9 | Journal "TODO" substring detection in `Display.h:272` (no longer MQTT-related since F7) | Low | Deferred |

## 5. Localization Readiness (prep only -- no code)

Goal: English -> (future) Spanish/more, without breaking RAM budget.

- **Where strings live:** all user-facing text is `const char*` in flash (`Behaviour.h` 19.4 KB, plus faceplate/`Display.h`/`Web.h` UI text). Adding a language roughly **doubles flash, adds ~0 RAM** (PROGMEM arrays).
- **Budget:** flash image today (quotes+prompts ~19.4 KB) is trivial against the 4MB part; LittleFS is separate (`partitions.csv`). RAM cost is the risk: each language string is a `const char*` pointer (4 bytes x ~290 strings x N languages) plus `String` copies only while formatting. Negligible.
- **Recommended future design (when implementing):** a PROGMEM `struct LangStrings { const char* q[12][4][5]; const char* prompts[12]; const char* preambles[4]; const char* banned[4]; };` selected by a `ConfigState.lang` index; replace direct array references with `strings(appConfig.lang).q[...]`. Keep `{name}`/`{detail}`/`{score}` placeholders so formatting logic is untouched. Weather via `&lang=<code>`.
- **Measurement to revisit:** after any translation, re-check `.rodata` via `pio run -v` map output. Current baseline: 19.4 KB string literals in `Behaviour.h`.

## 6. Fix Log

- **F2** (AI.h): `resolvePromptPlaceholders` now resolves `{score}` (+ `{deskTime}`, `{focusTime}`, `{breakTime}`, `{breakCount}`, `{longestStreak}`, `{historyDays}`) for parity with `resolveLocalPlaceholders`, including the FIRST_SIT zero-guard.
- **F3** (main.cpp): `EVENT_STREAK_BEATEN` detail now `formatTime(currentStreak)` (the new record).
- **F4** (main.cpp): STRETCH/SLACKER/STREAK_BEATEN/FOCUS_END/LUNCH routed through `MessageManager` at `P_NORMAL, 0, R_NORMAL`; guard flags still set at schedule time.
- **F4b** (AI.h): when `isAILoading` is true the trigger now falls back to a local quote instead of being silently dropped.
- **F5** (main.cpp): weather URL `&leng=fr` -> `&lang=fr`.
- **F6**: removed unreachable `EVENT_JOURNAL`/`EVENT_TASK_DUE` switch cases in `triggerBehaviour`; deleted 7 unused `MessageManager` methods; synced `doc/architecture.md`, `doc/MQTTcommands.md`, `doc/ConstantsBreakdown.md`, `desk_buddy_flowchart_notes.md`.
- **F7**: removed the MQTT display-alert feature (`deskbuddy/display` + `deskbuddy/message` -> `EVENT_MQTT_MESSAGE` -> MessageManager -> screen). Deleted `EVENT_MQTT_MESSAGE`, `MQTT_DISPLAY_TOPIC`, `MQTT_PUBLISH_TOPIC` and the callback branch in `MqttService.h`. MQTT is now debug-only. Added `TRIGGER <eventType> [ai|fallback] [detail]` to the `deskbuddy/debug/cmd` dispatcher (`MqttDebug.h`), and the settings-page "Debug Trigger Event" panel now publishes that command over MQTT instead of calling `triggerBehaviour` directly (`Web.h handleTriggerEvent`).
- **F8** (main.cpp): return-to-desk greeting now always wins over the excessive-breaks roast. `EVENT_WELCOME_BACK` is scheduled on every real return (Away >= 3m); when the once/day excessive-breaks condition also holds, `EVENT_EXCESSIVE_BREAKS` is demoted to P_NORMAL and queued *behind* the greeting instead of replacing it (`main.cpp:1048-1058`).
- **F9** (MessageManager/Constants): split the merged priority tiers. Previously `P_URGENT` and `P_HIGH` both mapped to the same numeric value (3000), so NAGGING/GOAL/JOURNAL competed with the greeting on equal footing. `P_HIGH` now maps to a new `MSG_PRIORITY_HIGH = 2250` (midpoint), giving a real 4-tier ladder: URGENT 3000 > HIGH 2250 > NORMAL 1500 > LOW 500. Greeting (P_URGENT) now always outranks nagging/journal/goal.
- **F10** (main.cpp): `checkDueTasks` rewritten as a catch-up scheduler (fires `EVENT_TASK_DUE` whenever `dueMins <= nowMins`, not just on the exact minute) routed through MessageManager at `P_HIGH, 0, R_IMPORTANT` with `firedDueDay`/`firedDueKeys` guards; LUNCH/pre-lunch/end-of-day scheduling no longer gated on `STATE_AWAY` so it queues while away.
- **F11** (main.cpp): FOCUS_END celebrated in-place when a confirmed FOCUS session exits to BUSY/REGULAR (sticky-confirm block), instead of on the leave path.
- **F12** (Behaviour/Display/Constants): added `EVENT_PAGE 13` + `MSG_PAGE_MAX_CHARS 110`; `triggerBehaviour` renders raw 2nd-screen text locally (no AI); Display.h splits messages > 110 chars into a follow-up `EVENT_PAGE` screen at `P_HIGH, 8000, R_NORMAL` (skipping JOURNAL/TASK_DUE layouts).
- **F13** (MqttDebug/Display): MQTT `TRIGGER` commands produced no visible reaction on the device. Two causes fixed: (1) `handleTrigger` only accepted numeric event types, so named payloads from the client presets (`TRIGGER LUNCH ai`, `TRIGGER JOURNAL ai`, ...) were rejected with an error and never reached `triggerBehaviour` -- added `parseEventType()` name mapping (`FIRST_SIT`, `WELCOME_BACK`, `STRETCH`, `FOCUS_END`, `SLACKER`, `STREAK_BEATEN`, `LUNCH`/`LUNCH_REMINDER`, `EXCESSIVE_BREAKS`, `GOAL_COMPLETED`, `JOURNAL`, `NAGGING`, `TASK_DUE`, `PAGE`) and a range check, plus a log-stream line on rejection; (2) even successful triggers were suppressed on screen because events 0/1 route through `pendingWelcomeAlert` and every alert is dropped while the device believes the user is away -- added `appState.manualTriggerOverride`, set by `handleTrigger`, which routes manual triggers straight to the alert overlay and forces `targetPage = -2` during the alert even in the away state (`Display.h`).
- **F14** (Curation/Constants): AI prompts previously saw task data only as scattered counts (no names) for normal events, and NAGGING/overdue detail only in the highly-overdue lists. Rebuilt `getTodoObservations` output around a compact `[TASK SYNTHESIS]` block injected into every AI prompt: counts (daily pending today, monthly due today, daily overdue 3d+, monthly overdue this-month/3mo+) plus task names with due times / overdue durations, capped at `TASK_SYNTHESIS_MAX_CHARS 500`. NAGGING prepends a `Highly Overdue Tasks Alert!` framing; the count-only observation lines were removed as redundant; midday past-12 observation kept. Local TASK_DUE/JOURNAL renders unchanged (AI-prompts-only placement). **F14b:** the synthesis bullets and the NAGGING `{detail}` name list are Fisher-Yates shuffled on every call so the AI doesn't keep latching onto the first task in the list.

## 7. Proposed: Unified Elective Scheduler & Annoyance Budget (spec, no code yet)

### 7.1 Relevance windows today

- Semantics (`MessageManager.cpp`): `scheduleTime = millis() + delayMs`; a message is deliverable only while `scheduleTime <= now <= scheduleTime + relevanceWindow` (`getNextDueMessage`, cpp:31-32), otherwise cleared (`clearExpiredMessages`, cpp:14). The relevance window is therefore the catch-up / expiry mechanism.
- Ordering: priority desc, then scheduleTime asc (cpp:20-26). One pop per loop; the `systemBusy` gate (main.cpp:1129) spaces pops while an alert is up. Windows: URGENT 5m, NORMAL 30m, LOW 1h (`Constants.h:73-75`).

Defects found:
- **RW1 -- R_BRIEF inverted/dead.** 4 enum values but `scheduleMessageWithPriority` maps R_BRIEF to the *longest* window (default branch -> RELEVANCE_LOW, 1h). No call site uses it.
- **RW2 -- R_IMPORTANT == R_NORMAL == 30m.** "Importance" changes nothing today (both fall to RELEVANCE_NORMAL).
- **RW3 -- Every elective gets 30m.** All elective call sites pass R_NORMAL/R_IMPORTANT, so STRETCH scheduled at T can fire until T+30m and stack behind the next paced nudge; HIGH electives always pop before earlier NORMAL ones (priority, not age). This is the de-facto burst mechanism.

### 7.2 Groups & windows (decision)

- **Critical -- 12h (`R_IMPORTANT`):** TASK_DUE. Near-total catch-up for a missed due task.
- **Paced -- 1h (`R_NORMAL`):** FIRST_SIT, GOAL_COMPLETED, JOURNAL. Once-a-day milestones; up to 1h catch-up (keeps T4 away-queueing for journals).
- **Slot -- 2m (`R_BRIEF`):** WELCOME_BACK, STRETCH, FOCUS_END, SLACKER, STREAK_BEATEN, LUNCH_REMINDER, EXCESSIVE_BREAKS, NAGGING. Fire-now: if the device can't show it within 2m it is dropped, never re-queued.

### 7.3 Why this replaces the annoyance budget

- The 2m slot window *is* the anti-burst mechanism: a queued STRETCH that loses to a higher-priority message expires within 2m instead of firing late (RW3 solved). No rolling budget counter, no `lastElectivePopTime`, no deferral chaining.
- Slot-message flags are set at *schedule* time, so a dropped message never refires that day.

### 7.4 Window mapping (`MessageManager.cpp:66-71`, `Constants.h:73-75`)

- `MSG_RELEVANCE_BRIEF` (new) 120000       // 2m
- `MSG_RELEVANCE_NORMAL` 3600000          // 1h (was 30m)
- `MSG_RELEVANCE_IMPORTANT` 43200000      // 12h (was 30m)
- `MSG_RELEVANCE_URGENT` 300000           // 5m, unchanged (R_CRITICAL unused)

Mapping: `R_BRIEF` -> BRIEF (fixes RW1: was the 1h default branch); `R_NORMAL` -> NORMAL/1h; `R_IMPORTANT` -> IMPORTANT/12h (fixes RW2: was == R_NORMAL); `R_CRITICAL` -> URGENT/5m (unchanged).

### 7.5 Call-site changes (future implementation)

No change (already match the new meanings):
- TASK_DUE (main.cpp:627) stays `R_IMPORTANT` -> 12h.
- JOURNAL x3 (main.cpp:1429, 1454, 1480) stays `R_NORMAL` -> 1h.
- FIRST_SIT (MessageManager.cpp:103): `R_IMPORTANT` -> `R_NORMAL` (1h).

Change:
- WELCOME_BACK (MessageManager.cpp:87): `R_IMPORTANT` -> `R_BRIEF` (2m).
- GOAL_COMPLETED (main.cpp:1415): `R_IMPORTANT` -> `R_NORMAL` (1h).
- STRETCH (1143), SLACKER (1156), STREAK_BEATEN (1168), FOCUS_END (1116): `R_NORMAL` -> `R_BRIEF`.
- LUNCH_REMINDER (1400), NAGGING (1523): `R_NORMAL` -> `R_BRIEF`.
- EXCESSIVE_BREAKS (1086): `R_IMPORTANT` -> `R_BRIEF` (2m).
- F12 `EVENT_PAGE` 2nd screen (Display.h:554): `R_NORMAL` -> `R_BRIEF` (a follow-up page should be near-immediate).

### 7.6 Effects & tradeoffs

- **WELCOME_BACK at 2m is safe in practice:** the greeting pops at sit+3s as the only `P_URGENT` message; `pendingWelcomeAlert` is only set after its AI response arrives (Display.h:539) and the render waits for `WELCOME_HOLD_MS` (Display.h:583) -- the window is consumed at pop, decoupled from display. Dropped only if AI is busy >2m at that instant.
- **LUNCH at 2m reverts F10/T4 away-catch-up for lunch:** away at lunch+15m -> the once/day flag is set at schedule and the nudge never shows. Journal keeps its 1h catch-up.
- **NAGGING at 2m:** busy at the 2h-sitting mark -> today's nag is skipped. Intentional per anti-burst.
- **EXCESSIVE_BREAKS at 2m:** can be dropped if the greeting occupies the busy window; once/day flag already set.
- **TASK_DUE at 12h:** pops immediately when free (`P_HIGH`); 12h is only a safety net for long away/busy stretches.
- Drift note: `WELCOME_DELAY_MS` comment says "1s" but value is 3000 (`Constants.h:21`).

### 7.7 Open questions

- Journal consolidation (B2): keep three separate 1h slots, or enforce one journal/day? (Morning kickoff is time-of-day-free -- fires 5m into any sit.)
- Confirm `EVENT_PAGE` folds into the 2m slot group.

## 8. Deferred / Out of Scope

Adaptive learning, persona memory, localization implementation, micro-break scoring (D7), focus-time lag (D8), TASK_DUE catch-up (T2), bounded nagging list (T3), missed-window recovery (T4), FOCUS_END on BUSY exit (T5). (Annoyance budget: dropped -- replaced by tiered relevance windows, spec §7.)
