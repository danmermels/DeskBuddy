import urllib.request
import json
import os

diagrams = {
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

os.makedirs("flowcharts", exist_ok=True)

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
            with open(f"flowcharts/{name}.svg", "wb") as f:
                f.write(svg_content)
        print(f"Successfully saved flowcharts/{name}.svg")
    except Exception as e:
        print(f"Failed to render {name}: {e}")
