import urllib.request
import json
import os

diagrams = {
    "holistic_architecture": """graph TB
    subgraph SETUP["1. SETUP boot"]
        A1[Power On] --> B1[Init Serial, Mutexes, MQTT binding]
        B1 --> C1[Load Preferences from NVS<br/>aiMode, clockFace, targetHours,<br/>gate sensitivities, etc.]
        C1 --> D1[Init LittleFS + loadDailyStats]
        D1 --> E1[TFT init + Away splash screen]
        E1 --> F1[setupRadar: Serial1 256k<br/>LD2410 config and gate sync]
        F1 --> G1[WiFi connect 5s timeout<br/>Static IP 192.168.15.160]
        G1 --> H1[setupMqtt, setupNTP,<br/>setupWebServer, ArduinoOTA.begin]
        H1 --> I1[Force initial NTP/weather on 1st loop<br/>Boot splash min 4s delay]
    end

    subgraph LOOP["2. MAIN LOOP every ~10ms"]
        direction TB
        J1[Loop start] --> K1[ArduinoOTA.handle]
        K1 --> L1{OTA in progress?}
        L1 -- Yes --> M1[Delay 50ms, return early]
        L1 -- No --> N1[server.handleClient + checkWiFiConnection]

        N1 --> O1[Update local tm struct from NTP]
        O1 --> P1[Midnight reset?<br/>Reset fsRead/write counters]
        P1 --> Q1[Poll LD2410 radar.read<br/>Read presence/moving/static]
        Q1 -->         R1[Every 100ms: update rolling<br/>median filters dist + motion]
        R1 --> S1[Compute rawState based on<br/>distance and motion ratio]
        S1 --> T1[Debounce: stablePresence<br/>2s on / 10s off]
    end

    subgraph STM["3. PRESENCE STATE MACHINE"]
        T1 --> TM1{Stable presence?}

        TM1 -- Yes --> UA1[Accumulate desk time,<br/>focus time, motion time]
        UA1 --> UA2{Was Away?}
        UA2 -- Yes --> UA3[Record sit-down time,<br/>calc break duration]
        UA3 --> UA4{First sit today<br/>or day rollover?}
        UA4 -- Yes --> UA5[triggerBehaviour EVENT_FIRST_SIT]
        UA4 -- No --> UA6[triggerBehaviour EVENT_WELCOME_BACK]

        UA2 -- No --> UA7{Rollover pending?}
        UA7 -- Yes --> UA8[Merge day presence<br/>or count break]
        UA8 --> UA9[Set rolloverPending=false]
        UA7 -- No --> UA10{State changed<br/>3 min sticky?}
        UA10 -- Yes --> UA11[Update currentPresenceState]

        UA11 --> UA12{Continuous presence triggers?}
        UA12 --> UA13[45min: Stretch reminder]
        UA12 --> UA14[1hr + score under 35: Slacker roast]
        UA12 --> UA15[New streak record alert]

        TM1 -- No --> UA16{Was present?}
        UA16 -- Yes --> UA17[Calc focus session duration]
        UA17 --> UA18[Focus over 15s? trigger EVENT_FOCUS_END]
        UA18 --> UA19{Session under 8 min?}
        UA19 -- Yes --> UA20[Stop-by: undo break count,<br/>subtract desk time]
        UA19 -- No --> UA21[Record break,<br/>reset session stats]
        UA16 -- No --> UA22[Accumulate totalBreakTime]
        UA22 --> UA23[Clear distance/motion filters]
    end

    subgraph SCORE["4. PRODUCTIVITY SCORE"]
        X1[Score = 100 first 5 min of workday] --> X2
        X2[penalty = break frequency + duration penalties<br/>bonus = focus time boosts]
        X2 --> X3[Clamp 0-100, update productivityScore]
    end

    subgraph NTPWTHR["5. NTP and WEATHER hourly"]
        W1{WiFi + timer expired?} --> W2
        W2[timeClient.update +<br/>HTTP GET OpenWeather API]
        W2 --> W3[Parse temp, weatherDesc,<br/>update tm struct]
    end

    subgraph SAVE["6. STATS SAVE every 10 min"]
        V1{Desk/focus/break time<br/>changed?} --> V2
        V2[Atomic save to /stats.json<br/>via .tmp rename pattern]
    end

    subgraph LRNCH["7. LUNCH REMINDER"]
        L1{Learned lunch hour?<br/>At desk over 30 min?} --> L2
        L2[triggerBehaviour EVENT_LUNCH_REMINDER]
    end

    subgraph AI["8. AI / BEHAVIOUR SYSTEM"]
        EV1[triggerBehaviour called] --> EV2{AI Mode ON<br/>under 15 req/day?}
        EV2 -- Yes --> EV3[Resolve persona prompt<br/>Create FreeRTOS task]
        EV3 --> EV4[queryGeminiTask:<br/>HTTPS POST to Gemini 2.5 Flash]
        EV4 --> EV5{HTTP 200?}
        EV5 -- Yes --> EV6[Parse response,<br/>set hasNewAIResponse=true]
        EV5 -- No --> EV7[Pick local fallback quote<br/>20 per event type]
        EV2 -- No --> EV7
    end

    subgraph MQTT["9. MQTT SERVICE"]
        MQ1{WiFi connected?} --> MQ2
        MQ2{MQTT broker connected?} -- No --> MQ3[Reconnect every 10s]
        MQ3 --> MQ4[Subscribe deskbuddy hash]
        MQ2 -- Yes --> MQ5[mqttClient.loop]
        MQ5 --> MQ6{Incoming deskbuddy/display?}
        MQ6 -- Yes --> MQ7[Set hasNewAIResponse,<br/>show on screen immediately]
    end

    subgraph WEB["10. WEB SERVER"]
        WB1[handleRoot: Dashboard page<br/>poll radar-data every 250ms]
        WB1 --> WB2[handleSettings: All params,<br/>radar chart, file manager,<br/>trigger debug events]
    end

    subgraph TFT["11. TFT DISPLAY UPDATE"]
        TF1{hasNewAIResponse?} --> TF2
        TF2[Copy message,<br/>publish to MQTT,<br/>set aiScreenEndTime=now+8s]
        TF2 --> TF3{State AWAY<br/>and grace expired?}
        TF3 -- Yes --> TF4[Draw away.rle, return]
        TF3 -- No --> TF5{Alert active?}
        TF5 -- Yes --> TF6[Draw msg_X.rle + text]
        TF5 -- No --> TF7[Clock face selector]
        TF7 --> TF8[0 Default: mood ring +<br/>digital clock + cycling metrics]
        TF7 --> TF9[1 Minimalist: dial ticks +<br/>hour/min/date + status icons]
        TF7 --> TF10[2 HiTech: cyberpunk bitmap +<br/>time/weather/desk hours]
        TF7 --> TF11[3 DEV: debug telemetry,<br/>100ms refresh rate]
    end

    SETUP --> LOOP
    LOOP --> STM
    STM --> SCORE
    LOOP --> NTPWTHR
    STM --> SAVE
    STM --> LRNCH
    STM --> AI
    LOOP --> MQTT
    LOOP --> WEB
    STM --> TFT
    AI --> TFT
    LOOP --> J1""",
    "main_loop_flowchart": """graph TD
    Start([Loop Start]) --> PollOTA[1. Handle OTA Updates]
    PollOTA --> CheckOTA{OTA in progress?}
    CheckOTA -- Yes --> DelayOTA[Delay 50ms & Return] --> Start
    CheckOTA -- No --> PollWeb[2. Handle Web Server Clients]
    PollWeb --> CheckWiFi[3. Check WiFi Connection status]
    CheckWiFi --> ResetCheck[4. Midnight Reset Check]
    ResetCheck --> PollRadar[5. Poll LD2410 Radar Sensor]
    PollRadar --> StateMachine[6. Presence State Machine & Debouncing]
    StateMachine --> Metrics[7. Update Productivity Score]
    Metrics --> Weather[8. Hourly NTP & Weather Fetch]
    Weather --> SaveStats[9. Save Daily Stats if changed]
    SaveStats --> UpdateTFT[10. Update TFT Display]
    UpdateTFT --> End([Loop End - Delay 10ms])""",

    "presence_state_machine": """stateDiagram-v2
    [*] --> STATE_AWAY
    
    STATE_AWAY --> StablePresent : Raw presence detected for 2s\n(If >= 3m: breakCount++ & welcome back alert)
    StablePresent --> STATE_AWAY : Raw presence absent for 10s
    
    state StablePresent {
        [*] --> ZoneCheck
        ZoneCheck --> FocusZone : Distance < focusDistanceLimit
        ZoneCheck --> DeskZone : Distance >= focusDistanceLimit
        
        state FocusZone {
            [*] --> HighMotionF
            HighMotionF --> STATE_BUSY : High Motion (>15%)
            HighMotionF --> STATE_FOCUS : Low Motion (<=15%)
        }
        
        state DeskZone {
            [*] --> HighMotionD
            HighMotionD --> STATE_DISTRACTED : High Motion (>15%)
            HighMotionD --> STATE_REGULAR : Low Motion (<=15%)
        }
    }""",

    "ai_triggers": """graph TD
    Trigger([triggerBehaviour called]) --> CheckAI{Is AI Mode active?}
    
    CheckAI -- Yes --> LimitCheck{Under daily 15-req cap?}
    LimitCheck -- Yes --> StartThread[Launch queryGeminiTask in background]
    LimitCheck -- No --> Fallback[Load Local Fallback Quote]
    CheckAI -- No --> Fallback
    
    StartThread --> HTTPSReq[Send secure HTTPS POST to Gemini 2.5 Flash]
    HTTPSReq --> RespCheck{HTTP 200 & Valid JSON?}
    
    RespCheck -- Yes --> SetNewResp[Store response, set hasNewAIResponse = true]
    RespCheck -- No --> Fallback
    
    Fallback --> SelectLocal[Pick 1 of 20 category-specific quotes]
    SelectLocal --> Personalize[Insert userName]
    Personalize --> SetNewResp""",

    "screen_priority": """graph TD
    Start([Display Update]) --> CheckNewAlert{hasNewAIResponse == true?}
    
    CheckNewAlert -- Yes --> LoadAlert[Set aiScreenEndTime = now + 8s & copy activeAlertMessage]
    CheckNewAlert -- No --> CheckAway{Is state == STATE_AWAY?}
    LoadAlert --> CheckAway
    
    CheckAway -- Yes --> CheckGrace{Away >= 1 minute?}
    CheckGrace -- Yes --> DrawAway[Draw Away Splash /away.rle & Return]
    CheckGrace -- No --> CheckPageChange[Resolve targetPage & forceRedraw]
    
    CheckAway -- No --> AILoadingCheck{Waiting for AI Welcome?}
    
    AILoadingCheck -- Yes --> ReturnAway[Show Away Splash & Return]
    AILoadingCheck -- No --> CheckPageChange[Resolve targetPage & forceRedraw]
    
    CheckPageChange --> ClockFace{clockFace variable}
    
    %% Minimalist Faceplate Branch
    ClockFace -- 1 --> Minimalist[Minimalist Faceplate]
    Minimalist --> MiniAlert{Alert Active?}
    MiniAlert -- Yes --> MiniAlertDraw[if forceRedraw: Draw msg_minimalist.rle + White Text]
    MiniAlert -- No --> MiniThrottle{Elapsed < 500ms?}
    MiniThrottle -- No --> MiniDraw[Draw Minimalist Clock: hours, minutes capsule, dial ticks sweep]
    
    %% Hi-Tech Faceplate Branch
    ClockFace -- 2 --> HiTech[Hi-Tech Faceplate]
    HiTech --> HiTechAlert{Alert Active?}
    HiTechAlert -- Yes --> HiTechAlertDraw[if forceRedraw: Draw msg_hitech.rle + Cyan Text]
    HiTechAlert -- No --> HiTechThrottle{Elapsed < 500ms?}
    HiTechThrottle -- No --> HiTechDraw[Draw Hi-Tech Clock: Cyberpunk bitmap overlay]
    
    %% Default Digital Faceplate Branch
    ClockFace -- 0 --> Default[Default Faceplate]
    Default --> DefaultRing[Animate & Draw Mood Ring at 20Hz]
    DefaultRing --> DefaultAlert{Alert Active?}
    DefaultAlert -- Yes --> DefaultAlertDraw[if forceRedraw or ringRedrawn: Draw msg_default.rle + Skyblue Text & Mood Ring]
    DefaultAlert -- No --> DefaultThrottle{Elapsed < 500ms or ringRedrawn?}
    DefaultThrottle -- Yes/No --> DefaultDraw[Draw Default Clock: Large time, weather, cycling metrics & Mood Ring]"""
}

script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = script_dir if os.path.basename(script_dir) != 'tools' else os.path.dirname(script_dir)
flowcharts_dir = os.path.join(project_root, "flowcharts")
os.makedirs(flowcharts_dir, exist_ok=True)

for name, source in diagrams.items():
    print(f"Rendering {name}...")
    try:
        url = "https://kroki.io"
        payload = {
            "diagram_source": source,
            "diagram_type": "mermaid",
            "output_format": "svg"
        }
        data = json.dumps(payload).encode('utf-8')
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
        }
        req = urllib.request.Request(
            url, 
            data=data, 
            headers=headers
        )
        with urllib.request.urlopen(req) as response:
            svg_content = response.read()
            with open(os.path.join(flowcharts_dir, f"{name}.svg"), "wb") as f:
                f.write(svg_content)
        print(f"Successfully saved {os.path.join(flowcharts_dir, name + '.svg')}")
    except Exception as e:
        print(f"Failed to render {name}: {e}")
