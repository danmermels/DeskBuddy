# DeskBuddy MQTT Commands Manual

Complete reference for all MQTT topics, commands, and message formats.

---

## Table of Contents

1. [Broker Configuration](#1-broker-configuration)
2. [Topic Map Overview](#2-topic-map-overview)
3. [Display Alert Topics](#3-display-alert-topics)
4. [Debug Command Platform](#4-debug-command-platform)
5. [Simulation Platform](#5-simulation-platform)
6. [System Commands](#6-system-commands)
7. [Publishing Topics](#7-publishing-topics)
8. [AI Debug Trace](#8-ai-debug-trace)
9. [Log Stream](#9-log-stream)
10. [Message Queue & Throttling](#10-message-queue--throttling)
11. [Message History Buffer](#11-message-history-buffer)
12. [Home Assistant Integration](#12-home-assistant-integration)

---

## 1. Broker Configuration

| Setting | Value |
|---------|-------|
| Default Broker | `192.168.15.18` |
| Default Port | `1883` |
| Client ID | `DeskBuddyClient` |
| Protocol | MQTT 3.1.1 (PubSubClient) |
| Auth | None (plain TCP) |
| Reconnect Interval | 10 seconds |

Configurable via NVS Preferences (`mqttBroker`, `mqttPort`) or the web dashboard at `/settings` / `/credentials`.

---

## 2. Topic Map Overview

### Inbound (Subscribed)

| Topic | Handler | Priority |
|-------|---------|----------|
| `deskbuddy/#` | Wildcard subscription (all subtopics below) | -- |
| `deskbuddy/display` | MessageManager (display alert) | HIGH |
| `deskbuddy/message` | MessageManager (display alert) | HIGH |
| `deskbuddy/debug/cmd` | `handleDebugCommand()` dispatcher | -- |

### Outbound (Published)

| Topic | Trigger | Format |
|-------|---------|--------|
| `deskbuddy/status` | On MQTT connect | `"online"` |
| `deskbuddy/echo` | After displaying an MQTT alert | String (echoed message) |
| `deskbuddy/heap` | Every 60 seconds | `{"freeHeap":NNN,"minFreeHeap":NNN}` |
| `deskbuddy/debug/resp` | After every debug command | JSON (varies by command) |
| `deskbuddy/debug/ai/request` | When AI query is sent | Multi-line text |
| `deskbuddy/debug/ai/response` | When AI response arrives | Multi-line text |
| `deskbuddy/log/<category>` | Every `Logger::log()` call | `[HH:MM:SS] message` |

### Self-Loop Prevention

`deskbuddy/heap` messages are silently ignored in the callback to prevent feedback loops when the device publishes its own heap telemetry.

---

## 3. Display Alert Topics

Publish a string to either topic to show a message on the TFT display. Both topics behave identically.

### Topics

```
deskbuddy/display
deskbuddy/message
```

### Payload Format

Plain string. Supports multi-line via `\n`. Special formatting is NOT parsed from MQTT payloads (color codes, two-column layout, etc. are only processed for AI-generated messages).

### Behaviour

- Routed through `MessageManager` at **HIGH priority**, **URGENT relevance**
- Bypasses all scheduled message queue — displayed immediately if no other urgent message is active
- Display duration is auto-calculated based on line count (3s base + 1.8s per line)
- After display, an echo is published to `deskbuddy/echo`

### Example

```bash
# MQTT CLI / mosquitto_pub
mosquitto_pub -h 192.168.15.18 -t "deskbuddy/display" -m "Meeting in 5 minutes!"
mosquitto_pub -h 192.168.15.18 -t "deskbuddy/message" -m "Lunch break!\nGo stretch your legs"
```

---

## 4. Debug Command Platform

All debug commands are sent as plain-text strings to `deskbuddy/debug/cmd`. All responses are published to `deskbuddy/debug/resp` as JSON.

### Syntax

```
<TOPLEVEL> [SUBCOMMAND] [ARGS...]
```

Commands are case-insensitive. Top-level keywords: `GET`, `SET`, `SIM`, `SYS`.

---

### 4.1 GET Commands

Query system state. Response format: `{"ok":true, ...}` or `{"ok":false,"error":"..."}`.

#### `GET state` — Current Presence State

```json
{
  "ok": true,
  "state": "FOCUS",
  "rawDist": 42,
  "filtDist": 41.3,
  "present": true,
  "moving": false,
  "stable": true
}
```

| Field | Type | Description |
|-------|------|-------------|
| `state` | string | Current presence state name |
| `rawDist` | int | Raw radar detection distance (cm), 0 = no detection |
| `filtDist` | float | Filtered (rolling median) distance |
| `present` | bool | Raw presence detection flag |
| `moving` | bool | Motion detected by radar |
| `stable` | bool | Debounced presence flag |

#### `GET radar` — Raw Radar Data

```json
{
  "ok": true,
  "rawDist": 42,
  "filtDist": 41.3,
  "present": true,
  "moving": true,
  "static": false,
  "sim": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `rawDist` | int | Raw detection distance (cm) |
| `filtDist` | float | Filtered distance |
| `present` | bool | Presence detected |
| `moving` | bool | Moving target detected |
| `static` | bool | Static (still) presence detected |
| `sim` | bool | Simulation mode active |

#### `GET filters` — Sensor Filter State

```json
{
  "ok": true,
  "filtDist": 41.3,
  "filterWindow": 2.0,
  "distAvg": 42.1,
  "distCount": 150
}
```

| Field | Type | Description |
|-------|------|-------------|
| `filtDist` | float | Current filtered distance |
| `filterWindow` | float | Rolling median window size (seconds) |
| `distAvg` | float | Session average distance |
| `distCount` | int | Number of distance samples in session |

#### `GET stats` — Daily Statistics

```json
{
  "ok": true,
  "deskTime": "4h32m",
  "focusTime": "2h15m",
  "breakTime": "45m",
  "breakCount": 3,
  "score": 72,
  "motionTime": "1h20m",
  "motionCount": 45,
  "longestStreak": "52m",
  "latestBreak": "15m",
  "firstSit": true,
  "dailyAiCount": 5,
  "fsWrites": 120,
  "fsReads": 45
}
```

| Field | Type | Description |
|-------|------|-------------|
| `deskTime` | string | Total desk time (formatted `XhYm` or `Ym`) |
| `focusTime` | string | Total focus time |
| `breakTime` | string | Total break time |
| `breakCount` | int | Number of breaks taken |
| `score` | int | Current productivity score (0-100) |
| `motionTime` | string | Total time with motion detected |
| `motionCount` | int | Number of motion episodes |
| `longestStreak` | string | Longest unbroken sitting streak |
| `latestBreak` | string | Duration of most recent break |
| `firstSit` | bool | True if user sat down today |
| `dailyAiCount` | int | AI requests made today (cap: 30) |
| `fsWrites` | int | Total LittleFS write operations (lifetime) |
| `fsReads` | int | Total LittleFS read operations (lifetime) |

#### `GET config` — All Configuration

```json
{
  "ok": true,
  "aiMode": 1,
  "aiPersona": 0,
  "clockFace": 4,
  "buddyFontIdx": 0,
  "userName": "dan",
  "targetHours": 8.0,
  "focusDistLim": 50,
  "motionRatioLim": 15,
  "distLimit": 120,
  "filterWindow": 2.0,
  "hasMail": false,
  "time24h": true,
  "g0mSens": 50, "g0sSens": 10,
  "g1mSens": 40, "g1sSens": 10,
  "g2mSens": 30, "g2sSens": 10,
  "g3mSens": 25, "g3sSens": 10,
  "g4mSens": 20, "g4sSens": 10,
  "g5mSens": 15, "g5sSens": 10,
  "g6mSens": 10, "g6sSens": 10
}
```

| Field | Type | Description |
|-------|------|-------------|
| `aiMode` | int | 0=Eco(off), 1=Balanced, 2=Frequent |
| `aiPersona` | int | 0=Coach, 1=Critic, 2=Sweet, 3=Friend |
| `clockFace` | int | Active faceplate ID (0-9) |
| `buddyFontIdx` | int | Font variant for DeskBuddy faces |
| `userName` | string | User name displayed in messages |
| `targetHours` | float | Daily desk time goal (hours) |
| `focusDistLim` | int | Focus zone distance threshold (cm) |
| `motionRatioLim` | int | Motion ratio threshold (%) |
| `distLimit` | int | Max desk presence distance (cm) |
| `filterWindow` | float | Rolling median filter window (seconds) |
| `hasMail` | bool | Mail alert flag |
| `time24h` | bool | 24-hour time format |
| `g0mSens`..`g6sSens` | int | LD2410 gate moving/static sensitivities |

#### `GET session` — Current Session Details

```json
{
  "ok": true,
  "deskTime": "1h23m",
  "motionTime": "22m",
  "distAvg": 43.2,
  "distCount": 85,
  "continuousPresence": "1h23m"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `deskTime` | string | Current sit session duration |
| `motionTime` | string | Motion time in current session |
| `distAvg` | float | Average distance this session |
| `distCount` | int | Distance samples this session |
| `continuousPresence` | string | Unbroken presence duration (resets on away) |

#### `GET time` — System Clock

```json
{
  "ok": true,
  "epoch": 1753651200,
  "hour": 14,
  "minute": 32,
  "day": 1,
  "dayName": "Monday",
  "ntpSet": true
}
```

#### `GET system` — Hardware & Network

```json
{
  "ok": true,
  "freeHeap": 185344,
  "minHeap": 142592,
  "uptime": "4h12m",
  "wifiRssi": -42,
  "wifiStatus": "connected",
  "simActive": false,
  "simContinuous": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `freeHeap` | int | Current free heap (bytes) |
| `minHeap` | int | Minimum free heap since boot (bytes) |
| `uptime` | string | Uptime since last reboot |
| `wifiRssi` | int | WiFi signal strength (dBm) |
| `wifiStatus` | string | `"connected"` or `"disconnected"` |
| `simActive` | bool | Simulation mode active |
| `simContinuous` | bool | Simulation running in continuous loop |

#### `GET <variable>` — Generic Variable Lookup

Any single variable name can be queried directly. Supported keys:

| Key | Returns |
|-----|---------|
| `state` / `presence` | Current presence state name |
| `rawDist` | Raw radar distance |
| `filtDist` | Filtered distance |
| `present` | Presence detected |
| `moving` | Motion detected |
| `score` / `productivityScore` | Productivity score |
| `deskTime` | Total desk time |
| `focusTime` | Total focus time |
| `breakTime` | Total break time |
| `breakCount` | Break count |
| `motionTime` | Total motion time |
| `motionCount` | Motion count |
| `longestStreak` | Longest sitting streak |
| `userName` | User name |
| `aiMode` | AI mode |
| `aiPersona` | AI persona |
| `clockFace` | Active faceplate |
| `buddyFontIdx` | Font index |
| `distLimit` | Desk distance limit |
| `focusDistLim` | Focus distance limit |
| `motionRatioLim` | Motion ratio limit |
| `filterWindow` | Filter window size |
| `freeHeap` | Free heap bytes |

**Example:**
```
→ deskbuddy/debug/cmd: GET score
← deskbuddy/debug/resp: {"ok":true,"score":72}
```

---

### 4.2 SET Commands

Update configuration or stats values. Changes are persisted to NVS Preferences (config) or applied to runtime state (stats).

### Syntax

```
SET <key> <value>
```

Keys must be prefixed with `config.` or `stats.`.

#### Config Keys (persisted to NVS)

| Key | Type | Range | Description |
|-----|------|-------|-------------|
| `config.aiMode` | int | 0-2 | AI mode (0=Eco, 1=Balanced, 2=Frequent) |
| `config.aiPersona` | int | 0-3 | Persona (0=Coach, 1=Critic, 2=Sweet, 3=Friend) |
| `config.clockFace` | int | 0-9 | Active faceplate |
| `config.buddyFontIdx` | int | varies | Font variant |
| `config.userName` | string | any | User name (quote-optional) |
| `config.targetHours` | float | 0-24 | Daily desk goal (hours) |
| `config.focusDistLim` | int | 1-300 | Focus zone distance (cm) |
| `config.motionRatioLim` | int | 1-100 | Motion ratio threshold (%) |
| `config.distLimit` | int | 1-300 | Max desk distance (cm) |
| `config.filterWindow` | float | 0.5-10 | Filter window (seconds) |
| `config.hasMail` | bool | `1`/`true`/`0`/`false` | Mail alert flag |
| `config.time24h` | bool | `1`/`true`/`0`/`false` | 24-hour format |
| `config.g0mSens`..`config.g6sSens` | int | 0-100 | LD2410 gate sensitivities |

**Important:** String values with spaces should be quoted: `SET config.userName "John"`

#### Stats Keys (runtime only, not persisted)

| Key | Type | Description |
|-----|------|-------------|
| `stats.breakCount` | int | Break count override |
| `stats.latestBreakDuration` | long | Latest break duration (ms) |
| `stats.previousLatestBreakDuration` | long | Previous break duration (ms) |
| `stats.totalBreakTime` | long | Total break time (ms) |
| `stats.totalDeskTime` | long | Total desk time (ms) |
| `stats.totalFocusTime` | long | Total focus time (ms) |
| `stats.firstSitToday` | bool | First sit flag |
| `stats.overnightBreakDuration` | long | Overnight break duration (ms) |

**Examples:**
```
→ SET config.aiMode 2
← {"ok":true,"key":"aiMode","value":2}

→ SET config.userName "Alice"
← {"ok":true,"key":"userName","value":"Alice"}

→ SET config.targetHours 6.5
← {"ok":true,"key":"targetHours","value":6.5}

→ SET stats.breakCount 0
← {"ok":true,"key":"breakCount","value":0}
```

---

## 5. Simulation Platform

Simulate radar input and presence states without physical hardware. Useful for testing behaviour triggers, display faces, and AI responses.

### Syntax

```
SIM <subcommand> [parameters...]
```

#### Preset Commands

| Command | Distance | Moving | Present | Override State | Description |
|---------|----------|--------|---------|----------------|-------------|
| `SIM away` | 0 | false | false | AWAY | User is away from desk |
| `SIM sit [dist]` | 80 (default) | false | true | (auto) | User is sitting still |
| `SIM focus [dist]` | 40 (default) | false | true | FOCUS | User in focus zone, low motion |
| `SIM busy [dist]` | 40 (default) | true | true | BUSY | User in focus zone, high motion |
| `SIM distracted [dist]` | 120 (default) | true | true | DISTRACTED | User beyond focus, high motion |

The optional `[dist]` parameter sets the simulated detection distance in cm.

#### State Override

```
SIM state <AWAY|FOCUS|BUSY|DISTRACTED|REGULAR>
```

Force a specific presence state regardless of distance/motion. Automatically sets distance and motion flags to match the state.

#### Raw Radar Simulation

```
SIM radar <distance> <moving> <present>
```

Set exact raw values. `moving` and `present` are `0` or `1`.

#### Time Simulation

```
SIM time <epoch>
```

Override the system time for testing time-based triggers (lunch reminders, journal, etc.).

#### Control

| Command | Description |
|---------|-------------|
| `SIM loop` | Enable continuous simulation mode |
| `SIM stop` | Disable simulation, resume real radar input |

**Examples:**
```
→ SIM sit 80
← {"ok":true,"sim":"on","preset":"sit","dist":80}

→ SIM state FOCUS
← {"ok":true,"sim":"on","overrideState":"FOCUS"}

→ SIM radar 45 1 1
← {"ok":true,"sim":"on","dist":45,"moving":1,"present":1}

→ SIM stop
← {"ok":true,"sim":"off"}
```

---

## 6. System Commands

Dangerous operations. All trigger an immediate reboot after sending the response.

### Syntax

```
SYS <subcommand>
```

| Command | Description |
|---------|-------------|
| `SYS reboot` | Restart the ESP32 |
| `SYS reset_stats` | Clear all NVS preferences + delete `stats.json`, then reboot |
| `SYS factory_reset` | Same as `reset_stats` (full factory reset) |

**Example:**
```
→ SYS reboot
← {"ok":true,"msg":"Rebooting..."}
```

**Warning:** `SYS reset_stats` and `SYS factory_reset` clear ALL saved configuration (WiFi, MQTT, API keys, radar sensitivities). You will need to reconfigure via the captive portal on next boot.

---

## 7. Publishing Topics

### `deskbuddy/status` — Online Status

Published once on every MQTT connection (including reconnects).

| Field | Value |
|-------|-------|
| Topic | `deskbuddy/status` |
| Payload | `"online"` |
| QoS | 0 (fire-and-forget) |

### `deskbuddy/echo` — Display Alert Echo

Published after a message from `deskbuddy/display` or `deskbuddy/message` is displayed on screen. Allows external systems to confirm the alert was shown.

| Field | Value |
|-------|-------|
| Topic | `deskbuddy/echo` |
| Payload | String (same as the display alert content) |

### `deskbuddy/heap` — Heap Telemetry

Published every 60 seconds as a periodic health check.

| Field | Value |
|-------|-------|
| Topic | `deskbuddy/heap` |
| Payload | `{"freeHeap":NNN,"minFreeHeap":NNN}` |

| Field | Type | Description |
|-------|------|-------------|
| `freeHeap` | int | Current free heap (bytes) |
| `minFreeHeap` | int | Minimum free heap since boot (bytes) |

**Note:** Self-loop prevention: incoming `deskbuddy/heap` messages are silently dropped in the callback.

---

## 8. AI Debug Trace

When AI mode is active and a query is triggered, full request/response payloads are published for debugging. Requires an external MQTT subscriber to capture.

### `deskbuddy/debug/ai/request`

Published just before the Groq API call.

```
---------- AI Request ----------
URL: https://api.groq.com/openai/v1/chat/completions
Body: {"model":"llama-3.3-70b-versatile","messages":[...],"temperature":0.7}
-------------------------------
```

### `deskbuddy/debug/ai/response`

Published when the Groq API responds.

```
---------- AI Response ----------
HTTP Code: 200
Payload: {"choices":[{"message":{"content":"..."}}]}
--------------------------------
```

**Note:** These are large multi-line strings. Use a subscriber that handles long payloads.

---

## 9. Log Stream

Every `Logger::log()` call publishes to a per-category topic.

### Topic Format

```
deskbuddy/log/<CATEGORY>
```

### Payload Format

```
[HH:MM:SS] message text here
```

### Categories

| Category | Description |
|----------|-------------|
| `BEHAVIOUR` | Behaviour trigger events and AI dispatches |
| `MQTT` | MQTT connection, message routing, debug commands |
| `STATE` | Presence state transitions, session changes |

**Example:**
```
Topic:   deskbuddy/log/BEHAVIOUR
Payload: [14:32:05] EVENT_FIRST_SIT triggered
```

Subscribe to all logs:
```
mosquitto_sub -h 192.168.15.18 -t "deskbuddy/log/#"
```

---

## 10. Message Queue & Throttling

### Publish Queue

All outbound MQTT messages (except `deskbuddy/heap` and `deskbuddy/debug/resp`) go through a thread-safe queue:

- **Max size:** 20 messages
- **Overflow policy:** Oldest message discarded, new message appended
- **Lock timeout:** 50ms per enqueue/dequeue operation
- **Processing:** Drained in `loopMqtt()` on the main loop task

### Inbound Throttling

- **Reconnect interval:** 10 seconds between connection attempts
- **History buffer:** Last 50 messages retained in circular buffer (thread-safe via mutex)

---

## 11. Message History Buffer

The last 50 received MQTT messages are stored in a circular buffer for diagnostic purposes.

### Access via Web Dashboard

The `/radar-data` JSON endpoint includes the full history:

```json
{
  "mqttHistory": [
    {
      "topic": "deskbuddy/display",
      "payload": "Meeting soon!",
      "timestamp": 12345678
    },
    ...
  ]
}
```

### Access via MQTT Debug

Use `GET state` to see current connection status. History is only accessible via the web dashboard `/radar-data` endpoint or the web UI's MQTT History section.

---

## 12. Home Assistant Integration

### Example `configuration.yaml`

```yaml
mqtt:
  sensor:
    - name: "DeskBuddy Presence"
      state_topic: "deskbuddy/debug/resp"
      value_template: "{{ value_json.state }}"
      json_attributes_topic: "deskbuddy/debug/resp"
      scan_interval: 10

  switch:
    - name: "DeskBuddy AI Mode"
      command_topic: "deskbuddy/debug/cmd"
      payload_on: "SET config.aiMode 2"
      payload_off: "SET config.aiMode 0"
      state_topic: "deskbuddy/debug/resp"
      value_template: "{{ value_json.aiMode }}"

  button:
    - name: "DeskBuddy Reboot"
      command_topic: "deskbuddy/debug/cmd"
      payload_press: "SYS reboot"

    - name: "DeskBuddy Reset Stats"
      command_topic: "deskbuddy/debug/cmd"
      payload_press: "SYS reset_stats"

  text:
    - name: "DeskBuddy Alert"
      command_topic: "deskbuddy/display"
      mode: text
```

### Automate Presence Detection

```yaml
automation:
  - alias: "DeskBuddy - User Away"
    trigger:
      platform: mqtt
      topic: "deskbuddy/debug/resp"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.state == 'AWAY' }}"
    action:
      - service: light.turn_off
        target:
          entity_id: light.desk_lamp
```

---

## Quick Reference Card

```
DISPLAY:   deskbuddy/display          "Alert message"
MESSAGE:   deskbuddy/message          "Alert message"  (alias)
ECHO:      deskbuddy/echo             ← echoed after display
HEALTH:    deskbuddy/heap             ← {"freeHeap":...}

DEBUG:     deskbuddy/debug/cmd        GET state|radar|filters|stats|config|session|time|system|<key>
                                      SET <config.key> <value> | <stats.key> <value>
                                      SIM away|sit|focus|busy|distracted|radar|state|time|loop|stop
                                      SYS reboot|reset_stats|factory_reset
RESPONSE:  deskbuddy/debug/resp       ← JSON response

AI TRACE:  deskbuddy/debug/ai/request  ← full prompt
           deskbuddy/debug/ai/response ← full response

LOGS:      deskbuddy/log/#             ← [HH:MM:SS] message
```
