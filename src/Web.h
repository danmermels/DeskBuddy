#ifndef WEB_H
#define WEB_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ld2410.h>
#include <Preferences.h>

// Extern references for global state in main.cpp
extern WebServer server;
extern Preferences preferences;
extern int currentPresenceState;
extern bool sensorPresenceDetected;
extern bool sensorMovingTargetDetected;
extern float filteredDetectionDist;
extern int rawDetectionDist;
extern float sessionDistanceAverage;
extern unsigned long totalDeskTime;
extern unsigned long totalFocusTime;
extern unsigned long totalBreakTime;
extern unsigned long overnightBreakDuration;
extern int breakCount;
extern unsigned long latestBreakDuration;
extern unsigned long longestSittingStreak;
extern uint32_t firstSitEpoch;
extern int productivityScore;
extern int aiMode;
extern int clockFace;
extern float targetHours;
extern String userName;
extern int focusDistanceLimit;
extern int motionRatioLimit;
extern unsigned long sessionDeskTime;
extern unsigned long sessionMotionTime;
extern unsigned long totalMotionTime;
extern int motionCount;
extern int deskDistanceLimit;
extern float filterWindow;
extern ld2410 radar;
extern int g0mSens, g0sSens, g1mSens, g1sSens, g2mSens, g2sSens, g3mSens, g3sSens, g4mSens, g4sSens, g5mSens, g5sSens, g6mSens, g6sSens;
extern SemaphoreHandle_t geminiMutex;
extern String aiResponse;
extern volatile bool lastResponseIsAi;
extern volatile bool isAILoading;
extern bool firstSitToday;
extern uint32_t lastAwayEpoch;
extern int dailyAiRequestCount;
extern unsigned long sessionDistanceSum;
extern unsigned long sessionDistanceCount;

// Extern functions defined in main.cpp
extern const char* getPresenceStateName(int state);
extern String formatTime(unsigned long ms);
extern String formatEpochTime(uint32_t epoch);
extern void saveDailyStats();

// Web Server Route Handlers
inline void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Radar Dashboard</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .header { display: flex; justify-content: space-between; align-items: center; width: 100%; max-width: 450px; margin-bottom: 10px; padding: 0 10px; box-sizing: border-box; }
    .header h1 { margin: 0; font-size: 1.6rem; color: #38bdf8; font-weight: 800; letter-spacing: -0.025em; }
    .cog-btn {
      color: #94a3b8;
      cursor: pointer;
      transition: color 0.2s, transform 0.3s ease;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .cog-btn:hover {
      color: #38bdf8;
      transform: rotate(45deg);
    }
    .cog-btn svg {
      width: 24px;
      height: 24px;
      fill: currentColor;
    }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 450px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    .ai-card {
      background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
      border: 1px solid #38bdf8;
      box-shadow: 0 0 15px rgba(56, 189, 248, 0.15);
      position: relative;
      overflow: hidden;
    }
    h1 { font-size: 1.5rem; color: #38bdf8; text-align: center; margin-bottom: 20px; }
    .metric { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #334155; align-items: center; }
    .metric:last-child { border: none; }
    .label { color: #94a3b8; }
    .value { font-weight: bold; }
    .badge { padding: 4px 8px; border-radius: 6px; font-size: 0.8rem; }
    .badge-present { background: #15803d; color: #bbf7d0; }
    .badge-away { background: #991b1b; color: #fca5a5; }
    .score-high { color: #4ade80; }
    .score-med { color: #fbbf24; }
    .score-low { color: #f87171; }
    .ai-message {
      font-size: 1.2rem;
      font-style: italic;
      color: #38bdf8;
      text-align: center;
      line-height: 1.5;
      margin: 15px 0 5px 0;
      min-height: 2.5rem;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .ai-badge {
      display: none;
      font-size: 0.75rem;
      color: #64748b;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      margin-top: -5px;
      margin-bottom: 10px;
      text-align: center;
      font-weight: bold;
    }
    .ai-loading-container {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      color: #fbbf24;
      font-size: 0.85rem;
    }
    .ai-spinner {
      width: 16px;
      height: 16px;
      border: 2px solid transparent;
      border-top-color: currentColor;
      border-radius: 50%;
      animation: spin 0.8s linear infinite;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>DeskBuddy Dashboard</h1>
    <a href="/settings" class="cog-btn" title="Settings & Calibration">
      <svg viewBox="0 0 24 24">
        <path d="M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"/>
      </svg>
    </a>
  </div>

  <div class="card ai-card">
    <div class="ai-badge" id="aiBadge">AI GENERATED</div>
    <div class="ai-message" id="aiMsg">Loading latest update...</div>
    <div class="ai-loading-container" id="aiLoading" style="display:none;">
      <div class="ai-spinner"></div>
      <span>Gemini is generating response...</span>
    </div>
  </div>

  <div class="card">
    <h1>Presence Metrics</h1>
    <div class="metric">
      <span class="label">Presence Status</span>
      <span class="value badge" id="statusBadge">Away</span>
    </div>
    <div class="metric">
      <span class="label">Distance to Sensor</span>
      <span class="value" id="dist">0 cm</span>
    </div>
  </div>

  <div class="card">
    <h1>Today's Performance</h1>
    <div class="metric">
      <span class="label">Time at Desk</span>
      <span class="value" id="deskTime">0m</span>
    </div>
    <div class="metric">
      <span class="label">Deep Focus Time</span>
      <span class="value" id="focusTime">0m</span>
    </div>
    <div class="metric">
      <span class="label">Active Motion Time</span>
      <span class="value" id="motionTime">0m</span>
    </div>
    <div class="metric">
      <span class="label">Time on Breaks</span>
      <span class="value" id="breakTime">0m</span>
    </div>
    <div class="metric">
      <span class="label">Total Breaks taken</span>
      <span class="value" id="breaks">0</span>
    </div>
    <div class="metric">
      <span class="label">Latest Break duration</span>
      <span class="value" id="latestBreak">0m</span>
    </div>
    <div class="metric">
      <span class="label">First Sitting Time</span>
      <span class="value" id="firstSit">Not registered</span>
    </div>
    <div class="metric">
      <span class="label">Longest Sitting Streak</span>
      <span class="value" id="longestStreak">0m</span>
    </div>
    <div class="metric">
      <span class="label">Productivity Score</span>
      <span class="value" id="score">0%</span>
    </div>
  </div>

  <script>
    function updateMetrics() {
      fetch('/radar-data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('dist').innerText = data.detectionDist + ' cm';
          
          let badge = document.getElementById('statusBadge');
          badge.innerText = data.state;
          if (data.presence) {
            badge.className = "value badge badge-present";
          } else {
            badge.className = "value badge badge-away";
          }
          
          document.getElementById('deskTime').innerText = data.deskTime;
          document.getElementById('focusTime').innerText = data.focusTime;
          document.getElementById('motionTime').innerText = data.totalMotionTime + ' (' + data.motionRatio + '% dynamic)';
          document.getElementById('breakTime').innerText = data.breakTime;
          document.getElementById('breaks').innerText = data.breaks;
          document.getElementById('latestBreak').innerText = data.latestBreak;
          document.getElementById('longestStreak').innerText = data.longestStreak;
          document.getElementById('firstSit').innerText = data.firstSitTime;
          
          let scoreVal = document.getElementById('score');
          scoreVal.innerText = data.score + '%';
          if (data.score >= 80) {
            scoreVal.className = "value score-high";
          } else if (data.score >= 50) {
            scoreVal.className = "value score-med";
          } else {
            scoreVal.className = "value score-low";
          }
          
          // AI Loading states
          let aiMsg = document.getElementById('aiMsg');
          let loadingContainer = document.getElementById('aiLoading');
          if (data.aiLoading) {
            aiMsg.style.display = "none";
            loadingContainer.style.display = "flex";
          } else {
            aiMsg.innerText = data.aiMessage ? data.aiMessage : "No events recorded yet.";
            aiMsg.style.display = "flex";
            loadingContainer.style.display = "none";
          }
          
          let isAi = data.isAiGenerated;
          document.getElementById('aiBadge').style.display = isAi ? "block" : "none";
          
          setTimeout(updateMetrics, 250);
        })
        .catch(err => {
          console.error("Error fetching radar data:", err);
          setTimeout(updateMetrics, 250);
        });
    }
    updateMetrics();
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleSettings() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Settings</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .settings-header { display: flex; align-items: center; width: 100%; max-width: 600px; margin-bottom: 15px; gap: 15px; padding: 0 10px; box-sizing: border-box; }
    .settings-header h1 { margin: 0; font-size: 1.6rem; color: #38bdf8; font-weight: 800; }
    .back-btn {
      color: #94a3b8;
      cursor: pointer;
      transition: color 0.2s, transform 0.2s;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .back-btn:hover {
      color: #38bdf8;
      transform: translateX(-3px);
    }
    .back-btn svg {
      width: 24px;
      height: 24px;
      fill: currentColor;
    }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 600px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    h1 { font-size: 1.5rem; color: #38bdf8; text-align: center; margin-bottom: 20px; }
    .metric { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #334155; align-items: center; }
    .metric:last-child { border: none; }
    .label { color: #94a3b8; }
    .value { font-weight: bold; }
    .settings-input {
      background: #0f172a;
      color: #f8fafc;
      border: 1px solid #334155;
      padding: 6px 10px;
      border-radius: 6px;
      width: 100px;
      text-align: right;
      font-family: inherit;
      font-size: 0.95rem;
    }
    .settings-select {
      background: #0f172a;
      color: #f8fafc;
      border: 1px solid #334155;
      padding: 6px 10px;
      border-radius: 6px;
      font-family: inherit;
      font-size: 0.95rem;
    }
    .btn {
      background: #38bdf8;
      color: #0f172a;
      font-weight: bold;
      border: none;
      padding: 10px 20px;
      border-radius: 6px;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.95rem;
      transition: opacity 0.2s;
    }
    .btn:hover {
      opacity: 0.9;
    }
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: #334155;
      outline: none;
      margin: 10px 0;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #38bdf8;
      cursor: pointer;
      transition: transform 0.1s;
    }
    .slider::-webkit-slider-thumb:hover {
      transform: scale(1.2);
    }
    .chart-container { position: relative; width: 100%; height: 220px; margin-top: 15px; }
    canvas { display: block; background: #0b0f19; border-radius: 8px; border: 1px solid #334155; width: 100%; height: 100%; }
    .legend { display: flex; justify-content: center; flex-wrap: wrap; gap: 12px; margin-top: 12px; font-size: 0.8rem; }
    .legend-item { display: flex; align-items: center; gap: 4px; }
    .legend-color { width: 12px; height: 12px; border-radius: 3px; }
    .toggle-container { display: flex; align-items: center; gap: 6px; font-size: 0.85rem; margin: 4px 0; }
  </style>
</head>
<body>
  <div class="settings-header">
    <a href="/" class="back-btn" title="Back to Dashboard">
      <svg viewBox="0 0 24 24">
        <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/>
      </svg>
    </a>
    <h1>DeskBuddy Settings</h1>
  </div>

  <div class="card">
    <h1>General Settings</h1>
    <form action="/save-settings" method="POST">
      <div class="metric">
        <span class="label">AI Mode</span>
        <select name="aiMode" id="aiModeSelect" class="settings-select">
          <option value="0">Eco (Off)</option>
          <option value="1">Balanced</option>
          <option value="2">Frequent</option>
        </select>
      </div>
      <div class="metric">
        <span class="label">Clock Face Style</span>
        <select name="clockFace" id="clockFaceSelect" class="settings-select">
          <option value="0">Default Digital</option>
          <option value="1">Minimalist</option>
          <option value="2">HiTech</option>
        </select>
      </div>
      <div class="metric">
        <span class="label">User Name</span>
        <input type="text" name="userName" id="userNameInput" class="settings-input" style="width: 150px; text-align: left;">
      </div>
      <div class="metric">
        <span class="label">Daily Target Hours</span>
        <input type="number" step="0.1" min="0.1" max="24.0" name="targetHours" id="targetHoursInput" class="settings-input">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Focus Distance Limit (cm)</span>
          <span class="value" id="focusDistLimVal">50 cm</span>
        </div>
        <input type="range" name="focusDistLim" id="focusDistLimSlider" min="20" max="150" step="5" class="slider" oninput="document.getElementById('focusDistLimVal').innerText = this.value + ' cm'">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Motion Ratio Threshold (%)</span>
          <span class="value" id="motionRatioLimVal">15%</span>
        </div>
        <input type="range" name="motionRatioLim" id="motionRatioLimSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('motionRatioLimVal').innerText = this.value + '%'">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Desk Distance Limit (cm)</span>
          <span class="value" id="distLimitVal">120 cm</span>
        </div>
        <input type="range" name="distLimit" id="distLimitSlider" min="50" max="300" step="5" class="slider" oninput="document.getElementById('distLimitVal').innerText = this.value + ' cm'">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Filter Window (Seconds)</span>
          <span class="value" id="filterWindowVal">2.0s</span>
        </div>
        <input type="range" name="filterWindow" id="filterWindowSlider" min="0.5" max="10.0" step="0.5" class="slider" oninput="document.getElementById('filterWindowVal').innerText = parseFloat(this.value).toFixed(1) + 's'">
      </div>
      <details style="margin-top: 15px; border-top: 1px solid #334155; padding-top: 10px;">
        <summary style="cursor: pointer; color: #38bdf8; font-weight: bold; padding: 5px 0; outline: none;">Gate Sensitivity Trigger Levels (Gates 0-6)</summary>
        <div style="margin-top: 10px; max-height: 350px; overflow-y: auto; padding-right: 5px;">
          <!-- Gate 0 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 0 Moving Sensitivity</span>
              <span class="value" id="g0mSensVal">100</span>
            </div>
            <input type="range" name="g0mSens" id="g0mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 0 Static Sensitivity</span>
              <span class="value" id="g0sSensVal">50</span>
            </div>
            <input type="range" name="g0sSens" id="g0sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0sSensVal').innerText = this.value">
          </div>
          <!-- Gate 1 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Moving Sensitivity</span>
              <span class="value" id="g1mSensVal">100</span>
            </div>
            <input type="range" name="g1mSens" id="g1mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Static Sensitivity</span>
              <span class="value" id="g1sSensVal">50</span>
            </div>
            <input type="range" name="g1sSens" id="g1sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1sSensVal').innerText = this.value">
          </div>
          <!-- Gate 2 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Moving Sensitivity</span>
              <span class="value" id="g2mSensVal">100</span>
            </div>
            <input type="range" name="g2mSens" id="g2mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Static Sensitivity</span>
              <span class="value" id="g2sSensVal">50</span>
            </div>
            <input type="range" name="g2sSens" id="g2sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2sSensVal').innerText = this.value">
          </div>
          <!-- Gate 3 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Moving Sensitivity</span>
              <span class="value" id="g3mSensVal">100</span>
            </div>
            <input type="range" name="g3mSens" id="g3mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Static Sensitivity</span>
              <span class="value" id="g3sSensVal">50</span>
            </div>
            <input type="range" name="g3sSens" id="g3sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3sSensVal').innerText = this.value">
          </div>
          <!-- Gate 4 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Moving Sensitivity</span>
              <span class="value" id="g4mSensVal">80</span>
            </div>
            <input type="range" name="g4mSens" id="g4mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Static Sensitivity</span>
              <span class="value" id="g4sSensVal">50</span>
            </div>
            <input type="range" name="g4sSens" id="g4sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4sSensVal').innerText = this.value">
          </div>
          <!-- Gate 5 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Moving Sensitivity</span>
              <span class="value" id="g5mSensVal">100</span>
            </div>
            <input type="range" name="g5mSens" id="g5mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Static Sensitivity</span>
              <span class="value" id="g5sSensVal">50</span>
            </div>
            <input type="range" name="g5sSens" id="g5sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5sSensVal').innerText = this.value">
          </div>
          <!-- Gate 6 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Moving Sensitivity</span>
              <span class="value" id="g6mSensVal">100</span>
            </div>
            <input type="range" name="g6mSens" id="g6mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g6mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Static Sensitivity</span>
              <span class="value" id="g6sSensVal">50</span>
            </div>
            <input type="range" name="g6sSens" id="g6sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g6sSensVal').innerText = this.value">
          </div>
        </div>
      </details>
      <div style="text-align: center; margin-top: 15px;">
        <button type="submit" class="btn">Save Configuration</button>
      </div>
    </form>
  </div>

  <div class="card">
    <h1>Radar Signal History</h1>
    <div style="display: flex; flex-wrap: wrap; gap: 10px; margin: 10px 0; justify-content: center; border-bottom: 1px solid #334155; padding-bottom: 10px;">
      <div class="toggle-container">
        <input type="checkbox" id="showDetectionDist" checked style="cursor:pointer;" onchange="drawChart()">
        <label for="showDetectionDist" style="cursor:pointer; color:#06b6d4; font-weight:bold;">Detection Dist</label>
      </div>
    </div>
    <div class="toggle-container" style="justify-content: center; margin-bottom: 10px;">
      <input type="checkbox" id="showRawCheckbox" checked style="cursor:pointer;" onchange="drawChart()">
      <label for="showRawCheckbox" style="cursor:pointer; color:#94a3b8;">Show Raw Signals (Dashed Lines)</label>
    </div>
    <div class="chart-container">
      <canvas id="radarChart"></canvas>
    </div>
    <div class="legend">
      <div class="legend-item">
        <span class="legend-color" style="background: #06b6d4;"></span>
        <span style="color:#94a3b8;">Detection Dist (cm)</span>
      </div>
    </div>
  </div>

  <div class="card" style="text-align: center;">
    <h1>System Actions</h1>
    <div style="display: flex; gap: 10px; justify-content: center;">
      <button class="btn" style="background: #eab308; color: #0f172a; flex: 1; font-size: 0.95rem; padding: 10px 12px;" onclick="resetStats()">Reset Daily Stats</button>
      <button class="btn" style="background: #ef4444; color: white; flex: 1; font-size: 0.95rem; padding: 10px 12px;" onclick="resetESP()">Reboot DeskBuddy</button>
    </div>
  </div>

  <script>
    let maxPoints = 240;
    let history = {
      detectionDist: [],
      rawDetectionDist: []
    };
    
    function drawChart() {
      let canvas = document.getElementById('radarChart');
      if (!canvas) return;
      let ctx = canvas.getContext('2d');
      let w = canvas.width = canvas.clientWidth;
      let h = canvas.height = canvas.clientHeight;
      
      ctx.clearRect(0, 0, w, h);
      
      let chartLeft = 35;
      
      // Draw grid lines and labels
      ctx.strokeStyle = '#1e293b';
      ctx.lineWidth = 1;
      ctx.fillStyle = '#64748b';
      ctx.font = '9px sans-serif';
      ctx.textBaseline = 'middle';
      
      let gridLevels = [20, 40, 60, 80];
      gridLevels.forEach(val => {
        let py = h - ((val - 20) / 60) * h;
        if (py < 5) py = 5;
        if (py > h - 5) py = h - 5;
        
        ctx.beginPath();
        ctx.moveTo(chartLeft, py);
        ctx.lineTo(w, py);
        ctx.stroke();
        
        ctx.fillText(val, 5, py);
      });
      
      let showRaw = document.getElementById('showRawCheckbox').checked;
      
      function drawLine(data, color, isDashed = false) {
        if (!data || data.length < 2) return;
        ctx.strokeStyle = color;
        ctx.lineWidth = isDashed ? 1.5 : 2;
        if (isDashed) {
          ctx.setLineDash([4, 4]);
        } else {
          ctx.setLineDash([]);
        }
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
          let x = chartLeft + (i / (maxPoints - 1)) * (w - chartLeft);
          let val = data[i];
          if (val > 80) val = 80;
          if (val < 20) val = 20;
          let y = h - ((val - 20) / 60) * h;
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }
      
      if (document.getElementById('showDetectionDist').checked) {
        drawLine(history.detectionDist, '#06b6d4');
        if (showRaw) drawLine(history.rawDetectionDist, 'rgba(6, 182, 212, 0.85)', true);
      }
      
      ctx.setLineDash([]);
    }

    function updateRadarChartAndSettings() {
      fetch('/radar-data')
        .then(response => response.json())
        .then(data => {
          history.detectionDist.push(data.detectionDist);
          history.rawDetectionDist.push(data.rawDetectionDist);
          
          Object.keys(history).forEach(key => {
            if (history[key].length > maxPoints) history[key].shift();
          });
          
          drawChart();
          
          if (!window.settingsPopulated) {
            document.getElementById('aiModeSelect').value = data.aiMode;
            document.getElementById('clockFaceSelect').value = data.clockFace;
            document.getElementById('targetHoursInput').value = data.targetHours;
            document.getElementById('userNameInput').value = data.userName;
            
            document.getElementById('focusDistLimSlider').value = data.focusDistLim;
            document.getElementById('focusDistLimVal').innerText = data.focusDistLim + ' cm';
            
            document.getElementById('motionRatioLimSlider').value = data.motionRatioLim;
            document.getElementById('motionRatioLimVal').innerText = data.motionRatioLim + '%';
            
            document.getElementById('distLimitSlider').value = data.distLimit;
            document.getElementById('distLimitVal').innerText = data.distLimit + ' cm';

            document.getElementById('filterWindowSlider').value = data.filterWindow;
            document.getElementById('filterWindowVal').innerText = parseFloat(data.filterWindow).toFixed(1) + 's';
            
            document.getElementById('g0mSensSlider').value = data.g0mSens;
            document.getElementById('g0mSensVal').innerText = data.g0mSens;
            document.getElementById('g0sSensSlider').value = data.g0sSens;
            document.getElementById('g0sSensVal').innerText = data.g0sSens;
            
            document.getElementById('g1mSensSlider').value = data.g1mSens;
            document.getElementById('g1mSensVal').innerText = data.g1mSens;
            document.getElementById('g1sSensSlider').value = data.g1sSens;
            document.getElementById('g1sSensVal').innerText = data.g1sSens;
            document.getElementById('g2mSensSlider').value = data.g2mSens;
            document.getElementById('g2mSensVal').innerText = data.g2mSens;
            document.getElementById('g2sSensSlider').value = data.g2sSens;
            document.getElementById('g2sSensVal').innerText = data.g2sSens;

            document.getElementById('g3mSensSlider').value = data.g3mSens;
            document.getElementById('g3mSensVal').innerText = data.g3mSens;
            document.getElementById('g3sSensSlider').value = data.g3sSens;
            document.getElementById('g3sSensVal').innerText = data.g3sSens;

            document.getElementById('g4mSensSlider').value = data.g4mSens;
            document.getElementById('g4mSensVal').innerText = data.g4mSens;
            document.getElementById('g4sSensSlider').value = data.g4sSens;
            document.getElementById('g4sSensVal').innerText = data.g4sSens;

            document.getElementById('g5mSensSlider').value = data.g5mSens;
            document.getElementById('g5mSensVal').innerText = data.g5mSens;
            document.getElementById('g5sSensSlider').value = data.g5sSens;
            document.getElementById('g5sSensVal').innerText = data.g5sSens;

            document.getElementById('g6mSensSlider').value = data.g6mSens;
            document.getElementById('g6mSensVal').innerText = data.g6mSens;
            document.getElementById('g6sSensSlider').value = data.g6sSens;
            document.getElementById('g6sSensVal').innerText = data.g6sSens;
            
            window.settingsPopulated = true;
          }

          setTimeout(updateRadarChartAndSettings, 250);
        })
        .catch(err => {
          console.error("Error fetching radar data:", err);
          setTimeout(updateRadarChartAndSettings, 250);
        });
    }
    
    updateRadarChartAndSettings();

    function resetStats() {
      if (confirm("Are you sure you want to reset your daily stats? This will clear all recorded times and break counts.")) {
        fetch('/reset-stats')
          .then(response => {
            alert("Daily stats have been reset.");
            location.reload();
          })
          .catch(err => alert("Failed to reset daily stats."));
      }
    }

    function resetESP() {
      if (confirm("Are you sure you want to reboot DeskBuddy?")) {
        fetch('/reset-esp')
          .then(response => {
            alert("DeskBuddy is rebooting... You will be redirected in 5 seconds.");
            setTimeout(() => { window.location.href = "/"; }, 5000);
          })
          .catch(err => alert("Failed to trigger reboot."));
      }
    }
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleRadarData() {
  DynamicJsonDocument doc(1024);
  doc["presence"] = (currentPresenceState != STATE_AWAY);
  doc["state"] = getPresenceStateName(currentPresenceState);
  doc["presenceDetected"] = sensorPresenceDetected;
  doc["movingTargetDetected"] = sensorMovingTargetDetected;
  
  // Distance metrics (always return current stored values to avoid single-frame connection dropouts)
  doc["detectionDist"] = (int)filteredDetectionDist;
  doc["rawDetectionDist"] = rawDetectionDist;
  doc["sessionDistAvg"] = (int)sessionDistanceAverage;
  
  doc["deskTime"] = formatTime(totalDeskTime);
  doc["focusTime"] = formatTime(totalFocusTime);
  doc["breakTime"] = formatTime(totalBreakTime);
  doc["overnightBreak"] = formatTime(overnightBreakDuration * 1000);
  doc["breaks"] = breakCount;
  doc["latestBreak"] = formatTime(latestBreakDuration);
  doc["longestStreak"] = formatTime(longestSittingStreak);
  doc["firstSitTime"] = formatEpochTime(firstSitEpoch);
  doc["score"] = productivityScore;
  doc["aiMode"] = aiMode;
  doc["clockFace"] = clockFace;
  doc["targetHours"] = targetHours;
  doc["userName"] = userName;
  doc["focusDistLim"] = focusDistanceLimit;
  doc["motionRatioLim"] = motionRatioLimit;
  doc["motionRatio"] = (sessionDeskTime > 0) ? std::min((int)((sessionMotionTime * 100) / sessionDeskTime), 100) : 0;
  doc["totalMotionTime"] = formatTime(totalMotionTime);
  doc["motionCount"] = motionCount;
  doc["distLimit"] = deskDistanceLimit;
  doc["filterWindow"] = filterWindow;
  
  // Gate sensitivities (only populate if connected, otherwise return 0)
  if (radar.isConnected()) {
    doc["g0mSens"] = g0mSens;
    doc["g0sSens"] = g0sSens;
    doc["g1mSens"] = g1mSens;
    doc["g1sSens"] = g1sSens;
    doc["g2mSens"] = g2mSens;
    doc["g2sSens"] = g2sSens;
    doc["g3mSens"] = g3mSens;
    doc["g3sSens"] = g3sSens;
    doc["g4mSens"] = g4mSens;
    doc["g4sSens"] = g4sSens;
    doc["g5mSens"] = g5mSens;
    doc["g5sSens"] = g5sSens;
    doc["g6mSens"] = g6mSens;
    doc["g6sSens"] = g6sSens;
  } else {
    doc["g0mSens"] = 0;
    doc["g0sSens"] = 0;
    doc["g1mSens"] = 0;
    doc["g1sSens"] = 0;
    doc["g2mSens"] = 0;
    doc["g2sSens"] = 0;
    doc["g3mSens"] = 0;
    doc["g3sSens"] = 0;
    doc["g4mSens"] = 0;
    doc["g4sSens"] = 0;
    doc["g5mSens"] = 0;
    doc["g5sSens"] = 0;
    doc["g6mSens"] = 0;
    doc["g6sSens"] = 0;
  }
  
  // Add AI response thread-safely
  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  doc["aiMessage"] = aiResponse;
  doc["isAiGenerated"] = lastResponseIsAi;
  xSemaphoreGive(geminiMutex);
  doc["aiLoading"] = isAILoading;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

inline void handleSaveSettings() {
  if (server.hasArg("aiMode") && server.hasArg("targetHours")) {
    aiMode = server.arg("aiMode").toInt();
    if (server.hasArg("clockFace")) clockFace = server.arg("clockFace").toInt();
    targetHours = server.arg("targetHours").toFloat();
    if (server.hasArg("userName")) userName = server.arg("userName");
    
    if (server.hasArg("focusDistLim")) focusDistanceLimit = server.arg("focusDistLim").toInt();
    if (server.hasArg("motionRatioLim")) motionRatioLimit = server.arg("motionRatioLim").toInt();
    if (server.hasArg("distLimit")) deskDistanceLimit = server.arg("distLimit").toInt();
    if (server.hasArg("filterWindow")) {
      filterWindow = server.arg("filterWindow").toFloat();
    }
    
    if (server.hasArg("g0mSens")) g0mSens = server.arg("g0mSens").toInt();
    if (server.hasArg("g0sSens")) g0sSens = server.arg("g0sSens").toInt();
    if (server.hasArg("g1mSens")) g1mSens = server.arg("g1mSens").toInt();
    if (server.hasArg("g1sSens")) g1sSens = server.arg("g1sSens").toInt();
    if (server.hasArg("g2mSens")) g2mSens = server.arg("g2mSens").toInt();
    if (server.hasArg("g2sSens")) g2sSens = server.arg("g2sSens").toInt();
    if (server.hasArg("g3mSens")) g3mSens = server.arg("g3mSens").toInt();
    if (server.hasArg("g3sSens")) g3sSens = server.arg("g3sSens").toInt();
    if (server.hasArg("g4mSens")) g4mSens = server.arg("g4mSens").toInt();
    if (server.hasArg("g4sSens")) g4sSens = server.arg("g4sSens").toInt();
    if (server.hasArg("g5mSens")) g5mSens = server.arg("g5mSens").toInt();
    if (server.hasArg("g5sSens")) g5sSens = server.arg("g5sSens").toInt();
    if (server.hasArg("g6mSens")) g6mSens = server.arg("g6mSens").toInt();
    if (server.hasArg("g6sSens")) g6sSens = server.arg("g6sSens").toInt();

    // Store in Preferences (Flash)
    preferences.begin("deskbuddy", false);
    preferences.putInt("aiMode", aiMode);
    preferences.putInt("clockFace", clockFace);
    preferences.putFloat("targetHours", targetHours);
    preferences.putString("userName", userName);
    preferences.putInt("focusDistLim", focusDistanceLimit);
    preferences.putInt("motionRatioLim", motionRatioLimit);
    preferences.putInt("distLimit", deskDistanceLimit);
    preferences.putFloat("filterWindow", filterWindow);
    preferences.putInt("g0mSens", g0mSens);
    preferences.putInt("g0sSens", g0sSens);
    preferences.putInt("g1mSens", g1mSens);
    preferences.putInt("g1sSens", g1sSens);
    preferences.putInt("g2mSens", g2mSens);
    preferences.putInt("g2sSens", g2sSens);
    preferences.putInt("g3mSens", g3mSens);
    preferences.putInt("g3sSens", g3sSens);
    preferences.putInt("g4mSens", g4mSens);
    preferences.putInt("g4sSens", g4sSens);
    preferences.putInt("g5mSens", g5mSens);
    preferences.putInt("g5sSens", g5sSens);
    preferences.putInt("g6mSens", g6mSens);
    preferences.putInt("g6sSens", g6sSens);
    preferences.end();
    
    saveDailyStats();
    
    // Dynamically adjust physical radar gates according to new range limit
    if (radar.isConnected()) {
      radar.setGateSensitivityThreshold(0, g0mSens, g0sSens);
      radar.setGateSensitivityThreshold(1, g1mSens, g1sSens);
      radar.setGateSensitivityThreshold(2, g2mSens, g2sSens);
      radar.setGateSensitivityThreshold(3, g3mSens, g3sSens);
      radar.setGateSensitivityThreshold(4, g4mSens, g4sSens);
      radar.setGateSensitivityThreshold(5, g5mSens, g5sSens);
      radar.setGateSensitivityThreshold(6, g6mSens, g6sSens);
      int requiredGates = (deskDistanceLimit + 19) / 20;
      if (requiredGates < 2) requiredGates = 2;
      if (requiredGates > 8) requiredGates = 8;
      radar.setMaxValues(requiredGates, requiredGates, 5);
    }
    
    // Redirect back to dashboard root page
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Settings Saved");
  } else {
    server.send(400, "text/plain", "Invalid Settings Data");
  }
}

inline void handleResetStats() {
  firstSitToday = true;
  firstSitEpoch = 0;
  breakCount = 0;
  totalDeskTime = 0;
  totalFocusTime = 0;
  totalBreakTime = 0;
  overnightBreakDuration = 0;
  lastAwayEpoch = 0;
  dailyAiRequestCount = 0;
  longestSittingStreak = 0;
  latestBreakDuration = 0;
  totalMotionTime = 0;
  motionCount = 0;
  sessionDeskTime = 0;
  sessionMotionTime = 0;
  sessionDistanceSum = 0;
  sessionDistanceCount = 0;
  sessionDistanceAverage = 0.0;

  saveDailyStats();

  server.send(200, "text/plain", "Daily Stats Reset");
}

inline void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/radar-data", handleRadarData);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/reset-stats", handleResetStats);
  server.on("/reset-esp", []() {
    server.send(200, "text/plain", "Rebooting");
    delay(500);
    ESP.restart();
  });
  server.begin();
}

#endif // WEB_H
