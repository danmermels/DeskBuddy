#ifndef WEB_H
#define WEB_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ld2410.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "MqttService.h"

// Extern references for global state in main.cpp
extern MqttMessage mqttHistory[MQTT_HISTORY_SIZE];
extern int mqttHistoryHead;
extern int mqttHistoryCount;
extern SemaphoreHandle_t mqttHistoryMutex;
extern PubSubClient mqttClient;
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
extern int aiPersona;
extern int clockFace;
extern bool hasMail;
extern bool time24h;
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
extern uint8_t hourlyPresenceHistory[24];
extern uint32_t presenceMsCurrentDay[24];
extern int historyDaysCount;
extern uint8_t getEffectivePresence(int h);
extern uint32_t fsWriteCount;
extern uint32_t fsReadCount;
extern int getLearnedWorkdayStart();
extern int getLearnedWorkdayStart(const uint8_t* history);
extern int getLearnedWorkdayEnd();
extern int getLearnedWorkdayEnd(const uint8_t* history);
extern int getLearnedLunchHour();
extern int getLearnedLunchHour(const uint8_t* history);

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
    .header { display: flex; justify-content: space-between; align-items: center; width: 100%; max-width: 600px; margin-bottom: 10px; padding: 0 10px; box-sizing: border-box; }
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
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 600px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
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
    <div class="metric">
      <span class="label">AI Queries Today</span>
      <span class="value" id="aiRequests">0 / 15</span>
    </div>
  </div>

  <div class="card">
    <h1>Learned Occupancy Pattern</h1>
    <div class="metric">
      <span class="label">Days Logged</span>
      <span class="value" id="historyDays">0 days</span>
    </div>
    <div style="display: flex; gap: 8px; justify-content: space-between; margin: 15px 0;">
      <div style="text-align: center; flex: 1; background: #0f172a; padding: 10px; border-radius: 8px; border: 1px solid #334155;">
        <div class="label" style="font-size: 0.75rem; margin-bottom: 4px; text-transform: uppercase; letter-spacing: 0.05em;">Start Time</div>
        <div class="value" id="workdayStart" style="color: #38bdf8; font-size: 1.15rem;">08:00</div>
      </div>
      <div style="text-align: center; flex: 1; background: #0f172a; padding: 10px; border-radius: 8px; border: 1px solid #334155;">
        <div class="label" style="font-size: 0.75rem; margin-bottom: 4px; text-transform: uppercase; letter-spacing: 0.05em;">Lunch Hour</div>
        <div class="value" id="lunchHour" style="color: #fbbf24; font-size: 1.15rem;">12:00</div>
      </div>
      <div style="text-align: center; flex: 1; background: #0f172a; padding: 10px; border-radius: 8px; border: 1px solid #334155;">
        <div class="label" style="font-size: 0.75rem; margin-bottom: 4px; text-transform: uppercase; letter-spacing: 0.05em;">End Time</div>
        <div class="value" id="workdayEnd" style="color: #f472b6; font-size: 1.15rem;">18:00</div>
      </div>
    </div>
    <div class="label" style="margin-bottom: 8px; font-size: 0.85rem;">Hourly Presence Probability:</div>
    <div style="display: flex; align-items: flex-end; justify-content: space-between; height: 100px; padding: 10px 5px; background: #0f172a; border-radius: 8px; border: 1px solid #334155; margin-bottom: 5px;">
      <div id="occupancyChart" style="display: flex; align-items: flex-end; justify-content: space-between; width: 100%; height: 100%;">
        <!-- dynamically generated bars -->
      </div>
    </div>
    <div style="position: relative; height: 15px; font-size: 0.7rem; color: #64748b; padding: 0 5px; margin-top: 4px;">
      <span style="position: absolute; left: 0%;">12am</span>
      <span style="position: absolute; left: 25%; transform: translateX(-50%);">6am</span>
      <span style="position: absolute; left: 50%; transform: translateX(-50%);">12pm</span>
      <span style="position: absolute; left: 75%; transform: translateX(-50%);">6pm</span>
      <span style="position: absolute; right: 0%;">11pm</span>
    </div>
  </div>

  <div class="card">
    <h1>MQTT Terminal</h1>
    <div style="display: flex; gap: 8px; margin-bottom: 12px; align-items: center; justify-content: space-between; flex-wrap: wrap;">
      <input type="text" id="mqttTopic" placeholder="topic" class="settings-input" style="flex: 1; min-width: 140px; text-align: left; box-sizing: border-box;" value="deskbuddy/message">
      <input type="text" id="mqttPayload" placeholder="Type message..." class="settings-input" style="flex: 2; min-width: 180px; text-align: left; box-sizing: border-box;" onkeydown="if(event.key === 'Enter') sendMqttMessage()">
      <button class="btn" onclick="sendMqttMessage()" style="padding: 6px 15px;">Send</button>
    </div>
    <div id="mqttConsole" style="background: #0f172a; border-radius: 8px; border: 1px solid #334155; height: 180px; overflow-y: auto; padding: 10px; font-family: monospace; font-size: 0.85rem; color: #38bdf8; display: flex; flex-direction: column; gap: 6px; box-sizing: border-box; text-align: left;">
      <div style="color: #64748b; font-style: italic;">Console initialized. Awaiting MQTT updates...</div>
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
          document.getElementById('aiRequests').innerText = data.dailyAiRequests + ' / 15';

          
          let scoreVal = document.getElementById('score');
          scoreVal.innerText = data.score + '%';
          if (data.score >= 80) {
            scoreVal.className = "value score-high";
          } else if (data.score >= 50) {
            scoreVal.className = "value score-med";
          } else {
            scoreVal.className = "value score-low";
          }

          // Update learned occupancy pattern card
          document.getElementById('historyDays').innerText = data.historyDays + (data.historyDays === 1 ? ' day' : ' days');
          document.getElementById('workdayStart').innerText = String(data.workdayStart).padStart(2, '0') + ':00';
          document.getElementById('lunchHour').innerText = String(data.lunchHour).padStart(2, '0') + ':00';
          document.getElementById('workdayEnd').innerText = String(data.workdayEnd).padStart(2, '0') + ':00';
          
          let chartContainer = document.getElementById('occupancyChart');
          if (chartContainer && data.occupancyHistory) {
            chartContainer.innerHTML = '';
            data.occupancyHistory.forEach((val, idx) => {
              let bar = document.createElement('div');
              bar.style.flex = '1';
              bar.style.margin = '0 1px';
              bar.style.height = val + '%';
              bar.style.background = 'linear-gradient(to top, #38bdf8, #f472b6)';
              bar.style.borderRadius = '2px 2px 0 0';
              bar.title = 'Hour ' + idx + ': ' + val + '%';
              chartContainer.appendChild(bar);
            });
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

    let lastMqttCount = -1;
    function updateMqttHistory() {
      fetch('/mqtt-history')
        .then(response => response.json())
        .then(data => {
          let consoleDiv = document.getElementById('mqttConsole');
          if (data.messages && data.messages.length !== lastMqttCount) {
            consoleDiv.innerHTML = '';
            if (data.messages.length === 0) {
              consoleDiv.innerHTML = '<div style="color: #64748b; font-style: italic;">No messages in history.</div>';
            } else {
              data.messages.forEach(msg => {
                let log = document.createElement('div');
                let elapsedSec = Math.floor(msg.timestamp / 1000);
                let h = Math.floor(elapsedSec / 3600);
                let m = Math.floor((elapsedSec % 3600) / 60);
                let s = elapsedSec % 60;
                let timeFormatted = String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');
                
                log.innerHTML = `<span style="color: #64748b;">[${timeFormatted}]</span> <span style="color: #fbbf24; font-weight: bold;">${msg.topic}:</span> <span style="color: #f8fafc;">${msg.payload}</span>`;
                consoleDiv.appendChild(log);
              });
            }
            consoleDiv.scrollTop = consoleDiv.scrollHeight;
            lastMqttCount = data.messages.length;
          }
          setTimeout(updateMqttHistory, 1000);
        })
        .catch(err => {
          console.error("Error fetching MQTT history:", err);
          setTimeout(updateMqttHistory, 2000);
        });
    }
    updateMqttHistory();

    function sendMqttMessage() {
      let topic = document.getElementById('mqttTopic').value;
      let payload = document.getElementById('mqttPayload').value;
      if (!payload) return;
      
      let formData = new FormData();
      formData.append('topic', topic);
      formData.append('payload', payload);
      
      fetch('/mqtt-publish', {
        method: 'POST',
        body: formData
      })
      .then(response => {
        if (response.ok) {
          document.getElementById('mqttPayload').value = '';
          lastMqttCount = -1; // trigger immediate poll refresh
        } else {
          alert('Failed to publish. Is MQTT broker connected?');
        }
      })
      .catch(err => {
        alert('Error publishing: ' + err);
      });
    }
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
        <span class="label">AI Persona</span>
        <select name="aiPersona" id="aiPersonaSelect" class="settings-select">
          <option value="0">Coach</option>
          <option value="1">Critic</option>
          <option value="2">Nerd</option>
          <option value="3">Zen</option>
        </select>
      </div>
      <div class="metric">
        <span class="label">Clock Face Style</span>
        <select name="clockFace" id="clockFaceSelect" class="settings-select">
          <option value="0">Default Digital</option>
          <option value="1">Minimalist</option>
          <option value="2">HiTech</option>
          <option value="3">DEV Mode</option>
        </select>
      </div>
      <div class="metric">
        <span class="label">Time Format</span>
        <select name="time24h" id="time24hSelect" class="settings-select">
          <option value="1">24-Hour</option>
          <option value="0">12-Hour</option>
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
      <div class="metric">
        <span class="label">Mail Alert Active</span>
        <select name="hasMail" id="hasMailSelect" class="settings-select">
          <option value="0">No Mail</option>
          <option value="1">Mail Active</option>
        </select>
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
          <div style="font-weight: bold; color: #38bdf8; margin: 5px 0 10px 0; border-bottom: 1px solid #334155; padding-bottom: 5px;">Static Gate Sensitivities</div>
          
          <!-- Gate 0 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 0 Static Sensitivity</span>
              <span class="value" id="g0sSensVal">50</span>
            </div>
            <input type="range" name="g0sSens" id="g0sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0sSensVal').innerText = this.value">
          </div>
          <!-- Gate 1 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Static Sensitivity</span>
              <span class="value" id="g1sSensVal">50</span>
            </div>
            <input type="range" name="g1sSens" id="g1sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1sSensVal').innerText = this.value">
          </div>
          <!-- Gate 2 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Static Sensitivity</span>
              <span class="value" id="g2sSensVal">50</span>
            </div>
            <input type="range" name="g2sSens" id="g2sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2sSensVal').innerText = this.value">
          </div>
          <!-- Gate 3 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Static Sensitivity</span>
              <span class="value" id="g3sSensVal">50</span>
            </div>
            <input type="range" name="g3sSens" id="g3sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3sSensVal').innerText = this.value">
          </div>
          <!-- Gate 4 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Static Sensitivity</span>
              <span class="value" id="g4sSensVal">50</span>
            </div>
            <input type="range" name="g4sSens" id="g4sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4sSensVal').innerText = this.value">
          </div>
          <!-- Gate 5 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Static Sensitivity</span>
              <span class="value" id="g5sSensVal">50</span>
            </div>
            <input type="range" name="g5sSens" id="g5sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5sSensVal').innerText = this.value">
          </div>
          <!-- Gate 6 Static -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Static Sensitivity</span>
              <span class="value" id="g6sSensVal">50</span>
            </div>
            <input type="range" name="g6sSens" id="g6sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g6sSensVal').innerText = this.value">
          </div>

          <div style="font-weight: bold; color: #38bdf8; margin: 15px 0 10px 0; border-top: 1px solid #334155; border-bottom: 1px solid #334155; padding: 10px 0 5px 0;">Moving Gate Sensitivities</div>

          <!-- Gate 0 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 0 Moving Sensitivity</span>
              <span class="value" id="g0mSensVal">100</span>
            </div>
            <input type="range" name="g0mSens" id="g0mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0mSensVal').innerText = this.value">
          </div>
          <!-- Gate 1 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Moving Sensitivity</span>
              <span class="value" id="g1mSensVal">100</span>
            </div>
            <input type="range" name="g1mSens" id="g1mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1mSensVal').innerText = this.value">
          </div>
          <!-- Gate 2 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Moving Sensitivity</span>
              <span class="value" id="g2mSensVal">100</span>
            </div>
            <input type="range" name="g2mSens" id="g2mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2mSensVal').innerText = this.value">
          </div>
          <!-- Gate 3 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Moving Sensitivity</span>
              <span class="value" id="g3mSensVal">100</span>
            </div>
            <input type="range" name="g3mSens" id="g3mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3mSensVal').innerText = this.value">
          </div>
          <!-- Gate 4 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Moving Sensitivity</span>
              <span class="value" id="g4mSensVal">80</span>
            </div>
            <input type="range" name="g4mSens" id="g4mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4mSensVal').innerText = this.value">
          </div>
          <!-- Gate 5 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Moving Sensitivity</span>
              <span class="value" id="g5mSensVal">100</span>
            </div>
            <input type="range" name="g5mSens" id="g5mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5mSensVal').innerText = this.value">
          </div>
          <!-- Gate 6 Moving -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Moving Sensitivity</span>
              <span class="value" id="g6mSensVal">100</span>
            </div>
            <input type="range" name="g6mSens" id="g6mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g6mSensVal').innerText = this.value">
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
      <input type="checkbox" id="showRawCheckbox" style="cursor:pointer;" onchange="drawChart()">
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

  <div class="card">
    <h1>Storage Status</h1>
    <div class="metric">
      <span class="label">Reads</span>
      <span class="value" id="fsReads">0</span>
    </div>
    <div class="metric">
      <span class="label">Writes</span>
      <span class="value" id="fsWrites">0</span>
    </div>
    <div class="metric">
      <span class="label">Estimated Lifespan</span>
      <span class="value" id="fsLifespan">Calculating...</span>
    </div>
    <div class="metric">
      <span class="label">Flash Health</span>
      <span class="value" id="fsHealth">100%</span>
    </div>
    <div style="width: 100%; background: #334155; height: 6px; border-radius: 3px; margin-top: 8px; overflow: hidden;">
      <div id="fsHealthBar" style="width: 100%; background: #10b981; height: 100%; transition: width 0.3s ease;"></div>
    </div>
  </div>

  <div class="card" style="text-align: center;">
    <h1>System Actions</h1>
    <div style="margin-bottom: 15px;">
      <a href="/file-manager" class="btn" style="background: #10b981; color: white; display: inline-flex; width: 100%; box-sizing: border-box; justify-content: center; align-items: center; gap: 6px; padding: 10px 12px; text-decoration: none;">
        <svg viewBox="0 0 24 24" style="width: 18px; height: 18px; fill: currentColor;">
          <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/>
        </svg>
        Open File Manager
      </a>
    </div>
    <div style="border-top: 1px solid #334155; padding-top: 15px;">
      <div style="display: flex; gap: 10px; justify-content: center;">
        <button class="btn" style="background: #eab308; color: #0f172a; flex: 1; font-size: 0.95rem; padding: 10px 12px;" onclick="resetStats()">Reset Daily Stats</button>
        <button class="btn" style="background: #ef4444; color: white; flex: 1; font-size: 0.95rem; padding: 10px 12px;" onclick="resetESP()">Reboot DeskBuddy</button>
      </div>
      <div style="margin-top: 10px;">
        <button class="btn" style="background: #dc2626; color: white; width: 100%; font-size: 0.95rem; padding: 10px 12px;" onclick="factoryReset()">Factory Reset</button>
      </div>
    </div>
    <div style="border-top: 1px solid #334155; padding-top: 15px; margin-top: 15px; text-align: left;">
      <span class="label" style="font-size: 0.85rem; display: block; margin-bottom: 8px;">Debug Trigger Event:</span>
      <div style="display: flex; gap: 8px;">
        <select id="debugEventSelect" style="flex: 1; background: #0f172a; border: 1px solid #334155; color: white; border-radius: 6px; padding: 8px; font-size: 0.9rem;">
          <option value="0">First Sit of Day (0)</option>
          <option value="1">Welcome Back (1)</option>
          <option value="2">Stretch Reminder (2)</option>
          <option value="3">Focus Session Congrats (3)</option>
          <option value="4">Slacker Roast (4)</option>
          <option value="5">Streak Beaten (5)</option>
          <option value="6">Lunch Reminder (6)</option>
        </select>
        <select id="debugMsgMode" style="width: 110px; background: #0f172a; border: 1px solid #334155; color: white; border-radius: 6px; padding: 8px; font-size: 0.9rem;">
          <option value="ai">AI Msg</option>
          <option value="fallback">Fallback</option>
        </select>
        <button class="btn" style="background: #a855f7; color: white; padding: 8px 12px; font-size: 0.9rem;" onclick="triggerDebugEvent()">Trigger</button>
      </div>
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
      const setVal = (id, val) => {
        let el = document.getElementById(id);
        if (el) el.value = val;
      };
      const setTxt = (id, txt) => {
        let el = document.getElementById(id);
        if (el) el.innerText = txt;
      };

      fetch('/radar-data')
        .then(response => response.json())
        .then(data => {
          history.detectionDist.push(data.detectionDist);
          history.rawDetectionDist.push(data.rawDetectionDist);
          
          Object.keys(history).forEach(key => {
            if (history[key].length > maxPoints) history[key].shift();
          });
          
          drawChart();
          
          // FS population (runs every 250ms)
          let reads = data.fsReadCount !== undefined ? data.fsReadCount : 0;
          let writes = data.fsWriteCount !== undefined ? data.fsWriteCount : 0;
          setTxt('fsReads', reads);
          setTxt('fsWrites', writes);
          
          let totalBytes = data.fsTotalBytes || 1048576;
          let uptimeSec = data.uptimeSeconds || 1;
          let historyDays = data.historyDays || 0;
          
          let writesPerDay = 10;
          if (historyDays > 0) {
            writesPerDay = writes / historyDays;
          } else if (uptimeSec > 60) {
            writesPerDay = (writes * 86400) / uptimeSec;
          }
          if (writesPerDay < 10) writesPerDay = 10;
          
          let totalSectors = totalBytes / 4096;
          let totalLifetimeWrites = totalSectors * 100000;
          let remainingWrites = totalLifetimeWrites - writes;
          if (remainingWrites < 0) remainingWrites = 0;
          
          let remainingDays = remainingWrites / writesPerDay;
          let years = remainingDays / 365.25;
          let lifespanText = "";
          if (years > 100) {
            lifespanText = "> 100 Years";
          } else {
            lifespanText = years.toFixed(1) + " Years";
          }
          setTxt('fsLifespan', lifespanText + ' (' + writesPerDay.toFixed(1) + ' writes/day)');
          
          let healthPercent = (remainingWrites / totalLifetimeWrites) * 100;
          setTxt('fsHealth', healthPercent.toFixed(2) + '%');
          let healthBar = document.getElementById('fsHealthBar');
          if (healthBar) {
            healthBar.style.width = healthPercent.toFixed(2) + '%';
            if (healthPercent > 80) {
              healthBar.style.background = '#10b981';
              let hEl = document.getElementById('fsHealth');
              if (hEl) hEl.style.color = '#10b981';
            } else if (healthPercent > 50) {
              healthBar.style.background = '#f59e0b';
              let hEl = document.getElementById('fsHealth');
              if (hEl) hEl.style.color = '#f59e0b';
            } else {
              healthBar.style.background = '#ef4444';
              let hEl = document.getElementById('fsHealth');
              if (hEl) hEl.style.color = '#ef4444';
            }
          }
          
          if (!window.settingsPopulated) {

            setVal('aiModeSelect', data.aiMode);
            setVal('aiPersonaSelect', data.aiPersona);
            setVal('clockFaceSelect', data.clockFace);
            setVal('targetHoursInput', data.targetHours);
            setVal('userNameInput', data.userName);
            setVal('hasMailSelect', data.hasMail ? "1" : "0");
            setVal('time24hSelect', data.time24h ? "1" : "0");
            
            setVal('focusDistLimSlider', data.focusDistLim);
            setTxt('focusDistLimVal', data.focusDistLim + ' cm');
            
            setVal('motionRatioLimSlider', data.motionRatioLim);
            setTxt('motionRatioLimVal', data.motionRatioLim + '%');
            
            setVal('distLimitSlider', data.distLimit);
            setTxt('distLimitVal', data.distLimit + ' cm');

            setVal('filterWindowSlider', data.filterWindow);
            setTxt('filterWindowVal', parseFloat(data.filterWindow).toFixed(1) + 's');
            
            setVal('g0mSensSlider', data.g0mSens);
            setTxt('g0mSensVal', data.g0mSens);
            setVal('g0sSensSlider', data.g0sSens);
            setTxt('g0sSensVal', data.g0sSens);
            
            setVal('g1mSensSlider', data.g1mSens);
            setTxt('g1mSensVal', data.g1mSens);
            setVal('g1sSensSlider', data.g1sSens);
            setTxt('g1sSensVal', data.g1sSens);
            
            setVal('g2mSensSlider', data.g2mSens);
            setTxt('g2mSensVal', data.g2mSens);
            setVal('g2sSensSlider', data.g2sSens);
            setTxt('g2sSensVal', data.g2sSens);

            setVal('g3mSensSlider', data.g3mSens);
            setTxt('g3mSensVal', data.g3mSens);
            setVal('g3sSensSlider', data.g3sSens);
            setTxt('g3sSensVal', data.g3sSens);

            setVal('g4mSensSlider', data.g4mSens);
            setTxt('g4mSensVal', data.g4mSens);
            setVal('g4sSensSlider', data.g4sSens);
            setTxt('g4sSensVal', data.g4sSens);

            setVal('g5mSensSlider', data.g5mSens);
            setTxt('g5mSensVal', data.g5mSens);
            setVal('g5sSensSlider', data.g5sSens);
            setTxt('g5sSensVal', data.g5sSens);

            setVal('g6mSensSlider', data.g6mSens);
            setTxt('g6mSensVal', data.g6mSens);
            setVal('g6sSensSlider', data.g6sSens);
            setTxt('g6sSensVal', data.g6sSens);
            
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

    function factoryReset() {
      if (confirm("WARNING: This will clear all settings, configurations, and historical daily stats. Are you sure you want to perform a factory reset?")) {
        fetch('/factory-reset')
          .then(response => {
            alert("Factory reset complete. DeskBuddy is rebooting... You will be redirected in 5 seconds.");
            setTimeout(() => { window.location.href = "/"; }, 5000);
          })
          .catch(err => alert("Failed to trigger factory reset."));
      }
    }

    function triggerDebugEvent() {
      let type = document.getElementById('debugEventSelect').value;
      let mode = document.getElementById('debugMsgMode').value;
      let detail = "";
      if (type === "0" || type === "5") detail = "45m";
      if (type === "1") detail = "15m";
      if (type === "3") detail = "25m";
      
      fetch('/trigger-event?type=' + type + '&detail=' + encodeURIComponent(detail) + '&mode=' + mode)
        .then(response => {
          console.log("Event " + type + " (" + mode + ") triggered on screen!");
        })
        .catch(err => console.error("Failed to trigger event.", err));
    }
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleRadarData() {
  DynamicJsonDocument doc(4096);
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
  doc["aiPersona"] = aiPersona;
  doc["dailyAiRequests"] = dailyAiRequestCount;
  doc["fsReadCount"] = fsReadCount;
  doc["fsWriteCount"] = fsWriteCount;
  doc["fsTotalBytes"] = (uint32_t)LittleFS.totalBytes();
  doc["uptimeSeconds"] = (uint32_t)(millis() / 1000);
  doc["clockFace"] = clockFace;
  doc["targetHours"] = targetHours;
  doc["hasMail"] = hasMail;
  doc["time24h"] = time24h;
  doc["userName"] = userName;
  doc["focusDistLim"] = focusDistanceLimit;
  doc["motionRatioLim"] = motionRatioLimit;
  doc["motionRatio"] = (sessionDeskTime > 0) ? std::min((int)((sessionMotionTime * 100) / sessionDeskTime), 100) : 0;
  doc["totalMotionTime"] = formatTime(totalMotionTime);
  doc["motionCount"] = motionCount;
  doc["distLimit"] = deskDistanceLimit;
  doc["filterWindow"] = filterWindow;
  
  // Learned occupancy metrics
  doc["historyDays"] = historyDaysCount;
  
  // Combine history with today's real-time accumulated presence
  uint8_t blendedHistory[24];
  for (int h = 0; h < 24; h++) {
    uint32_t todayMs = presenceMsCurrentDay[h];
    uint8_t todayPct = (uint8_t)constrain((todayMs * 100UL) / 3600000UL, 0UL, 100UL);
    // Blend the effective history (incorporating predefined routine) with today's real-time presence
    blendedHistory[h] = (uint8_t)((getEffectivePresence(h) * 4 + todayPct) / 5);
  }
  
  doc["lunchHour"] = getLearnedLunchHour(blendedHistory);
  doc["workdayStart"] = getLearnedWorkdayStart(blendedHistory);
  doc["workdayEnd"] = getLearnedWorkdayEnd(blendedHistory);
  
  JsonArray historyArray = doc.createNestedArray("occupancyHistory");
  for (int h = 0; h < 24; h++) {
    historyArray.add(blendedHistory[h]);
  }
  
  // Gate sensitivities (always report current synced variables)
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
    preferences.begin("deskbuddy", false);

    if (server.hasArg("aiMode")) {
      int val = server.arg("aiMode").toInt();
      if (val != aiMode) { aiMode = val; preferences.putInt("aiMode", aiMode); }
    }
    if (server.hasArg("aiPersona")) {
      int val = server.arg("aiPersona").toInt();
      if (val != aiPersona) { aiPersona = val; preferences.putInt("aiPersona", aiPersona); }
    }
    if (server.hasArg("clockFace")) {
      int val = server.arg("clockFace").toInt();
      if (val != clockFace) { clockFace = val; preferences.putInt("clockFace", clockFace); }
    }
    if (server.hasArg("targetHours")) {
      float val = server.arg("targetHours").toFloat();
      if (val < 0.1f) val = 8.0f;
      if (val != targetHours) { targetHours = val; preferences.putFloat("targetHours", targetHours); }
    }
    if (server.hasArg("hasMail")) {
      bool val = (server.arg("hasMail").toInt() == 1);
      if (val != hasMail) { hasMail = val; preferences.putBool("hasMail", hasMail); }
    }
    if (server.hasArg("time24h")) {
      bool val = (server.arg("time24h").toInt() == 1);
      if (val != time24h) { time24h = val; preferences.putBool("time24h", time24h); }
    }
    if (server.hasArg("userName")) {
      String val = server.arg("userName");
      if (val != userName) { userName = val; preferences.putString("userName", userName.c_str()); }
    }
    if (server.hasArg("focusDistLim")) {
      int val = server.arg("focusDistLim").toInt();
      if (val != focusDistanceLimit) { focusDistanceLimit = val; preferences.putInt("focusDistLim", focusDistanceLimit); }
    }
    if (server.hasArg("motionRatioLim")) {
      int val = server.arg("motionRatioLim").toInt();
      if (val != motionRatioLimit) { motionRatioLimit = val; preferences.putInt("motionRatioLim", motionRatioLimit); }
    }
    if (server.hasArg("distLimit")) {
      int val = server.arg("distLimit").toInt();
      if (val != deskDistanceLimit) { deskDistanceLimit = val; preferences.putInt("distLimit", deskDistanceLimit); }
    }
    if (server.hasArg("filterWindow")) {
      float val = server.arg("filterWindow").toFloat();
      if (val != filterWindow) { filterWindow = val; preferences.putFloat("filterWindow", filterWindow); }
    }

    if (server.hasArg("g0mSens")) {
      int val = server.arg("g0mSens").toInt();
      if (val != g0mSens) { g0mSens = val; preferences.putInt("g0mSens", g0mSens); }
    }
    if (server.hasArg("g0sSens")) {
      int val = server.arg("g0sSens").toInt();
      if (val != g0sSens) { g0sSens = val; preferences.putInt("g0sSens", g0sSens); }
    }
    if (server.hasArg("g1mSens")) {
      int val = server.arg("g1mSens").toInt();
      if (val != g1mSens) { g1mSens = val; preferences.putInt("g1mSens", g1mSens); }
    }
    if (server.hasArg("g1sSens")) {
      int val = server.arg("g1sSens").toInt();
      if (val != g1sSens) { g1sSens = val; preferences.putInt("g1sSens", g1sSens); }
    }
    if (server.hasArg("g2mSens")) {
      int val = server.arg("g2mSens").toInt();
      if (val != g2mSens) { g2mSens = val; preferences.putInt("g2mSens", g2mSens); }
    }
    if (server.hasArg("g2sSens")) {
      int val = server.arg("g2sSens").toInt();
      if (val != g2sSens) { g2sSens = val; preferences.putInt("g2sSens", g2sSens); }
    }
    if (server.hasArg("g3mSens")) {
      int val = server.arg("g3mSens").toInt();
      if (val != g3mSens) { g3mSens = val; preferences.putInt("g3mSens", g3mSens); }
    }
    if (server.hasArg("g3sSens")) {
      int val = server.arg("g3sSens").toInt();
      if (val != g3sSens) { g3sSens = val; preferences.putInt("g3sSens", g3sSens); }
    }
    if (server.hasArg("g4mSens")) {
      int val = server.arg("g4mSens").toInt();
      if (val != g4mSens) { g4mSens = val; preferences.putInt("g4mSens", g4mSens); }
    }
    if (server.hasArg("g4sSens")) {
      int val = server.arg("g4sSens").toInt();
      if (val != g4sSens) { g4sSens = val; preferences.putInt("g4sSens", g4sSens); }
    }
    if (server.hasArg("g5mSens")) {
      int val = server.arg("g5mSens").toInt();
      if (val != g5mSens) { g5mSens = val; preferences.putInt("g5mSens", g5mSens); }
    }
    if (server.hasArg("g5sSens")) {
      int val = server.arg("g5sSens").toInt();
      if (val != g5sSens) { g5sSens = val; preferences.putInt("g5sSens", g5sSens); }
    }
    if (server.hasArg("g6mSens")) {
      int val = server.arg("g6mSens").toInt();
      if (val != g6mSens) { g6mSens = val; preferences.putInt("g6mSens", g6mSens); }
    }
    if (server.hasArg("g6sSens")) {
      int val = server.arg("g6sSens").toInt();
      if (val != g6sSens) { g6sSens = val; preferences.putInt("g6sSens", g6sSens); }
    }

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
    
    // Redirect back to settings page
    server.sendHeader("Location", "/settings");
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

inline void handleMqttHistory() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("messages");
  
  if (mqttHistoryMutex != NULL) {
    xSemaphoreTake(mqttHistoryMutex, portMAX_DELAY);
    int idx = mqttHistoryHead;
    int count = mqttHistoryCount;

    // Return in chronological order (oldest to newest)
    for (int i = 0; i < count; i++) {
      int curIdx = (idx - count + i + MQTT_HISTORY_SIZE) % MQTT_HISTORY_SIZE;
      JsonObject obj = arr.createNestedObject();
      obj["topic"] = mqttHistory[curIdx].topic;
      obj["payload"] = mqttHistory[curIdx].payload;
      obj["timestamp"] = (double)mqttHistory[curIdx].timestamp;
    }
    xSemaphoreGive(mqttHistoryMutex);
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

inline void handleMqttPublish() {
  if (server.hasArg("payload")) {
    String topic = server.hasArg("topic") ? server.arg("topic") : "deskbuddy/message";
    String payload = server.arg("payload");
    
    if (mqttClient.connected()) {
      mqttClient.publish(topic.c_str(), payload.c_str());
      server.send(200, "text/plain", "Published");
    } else {
      server.send(503, "text/plain", "MQTT client disconnected");
    }
  } else {
    server.send(400, "text/plain", "Missing payload");
  }
}

inline void handleTriggerEvent() {
  if (server.hasArg("type")) {
    int eventType = server.arg("type").toInt();
    String detail = server.hasArg("detail") ? server.arg("detail") : "";
    
    int forceMode = 0;
    if (server.hasArg("mode")) {
      String mode = server.arg("mode");
      if (mode == "ai") {
        forceMode = 1;
      } else if (mode == "fallback") {
        forceMode = 2;
      }
    }

    // Trigger behavioral handler manually
    extern void triggerBehaviour(int event, String detail = "", int forceMode = 0);
    triggerBehaviour(eventType, detail, forceMode);
    
    server.send(200, "text/plain", "Event Triggered");
  } else {
    server.send(400, "text/plain", "Missing type");
  }
}

inline void handleFactoryReset() {
  preferences.begin("deskbuddy", false);
  preferences.clear();
  preferences.end();

  if (LittleFS.exists("/stats.json")) {
    fsWriteCount++;
    LittleFS.remove("/stats.json");
  }

  server.send(200, "text/plain", "Factory Reset Complete. Rebooting...");
  delay(1000);
  ESP.restart();
}

// Global/static file upload helper
static fs::File uploadFile;

inline void handleFilesList() {
  DynamicJsonDocument doc(2048);
  JsonArray files = doc.createNestedArray("files");
  
  fsReadCount++;
  fs::File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    fs::File file = root.openNextFile();
    while (file) {
      JsonObject f = files.createNestedObject();
      String name = String(file.name());
      if (!name.startsWith("/")) {
        name = "/" + name;
      }
      f["name"] = name;
      f["size"] = file.size();
      file = root.openNextFile();
    }
  }
  
  doc["totalBytes"] = (uint32_t)LittleFS.totalBytes();
  doc["usedBytes"] = (uint32_t)LittleFS.usedBytes();
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

inline void handleDownloadFile() {
  if (server.hasArg("path")) {
    String path = server.arg("path");
    if (!path.startsWith("/")) {
      path = "/" + path;
    }
    if (LittleFS.exists(path)) {
      fsReadCount++;
      fs::File file = LittleFS.open(path, "r");
      if (file) {
        // Extract filename for Content-Disposition header
        String filename = path;
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash != -1) {
          filename = filename.substring(lastSlash + 1);
        }
        
        server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
        server.streamFile(file, "application/octet-stream");
        file.close();
      } else {
        server.send(500, "text/plain", "Failed to open file");
      }
    } else {
      server.send(404, "text/plain", "File not found");
    }
  } else {
    server.send(400, "text/plain", "Missing path parameter");
  }
}

inline void handleDeleteFile() {
  if (server.hasArg("path")) {
    String path = server.arg("path");
    if (!path.startsWith("/")) {
      path = "/" + path;
    }
    // Protect stats.json
    if (path == "/stats.json") {
      server.send(403, "text/plain", "Cannot delete system file");
      return;
    }
    if (LittleFS.exists(path)) {
      fsWriteCount++;
      LittleFS.remove(path);
      server.send(200, "text/plain", "File deleted successfully");
    } else {
      server.send(404, "text/plain", "File not found");
    }
  } else {
    server.send(400, "text/plain", "Missing path parameter");
  }
}

inline void handleUploadResponse() {
  server.send(200, "text/plain", "Upload Success");
}

inline void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    // Open the file for writing in LittleFS
    fsWriteCount++;
    uploadFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
  }
}

inline void handleFileManager() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy File Manager</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .header { display: flex; align-items: center; width: 100%; max-width: 650px; margin-bottom: 15px; gap: 15px; padding: 0 10px; box-sizing: border-box; }
    .header h1 { margin: 0; font-size: 1.6rem; color: #38bdf8; font-weight: 800; }
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
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 650px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    h1 { font-size: 1.5rem; color: #38bdf8; text-align: center; margin-bottom: 20px; }
    
    .file-table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    .file-table th, .file-table td { padding: 12px; text-align: left; border-bottom: 1px solid #334155; }
    .file-table th { color: #94a3b8; font-weight: 600; font-size: 0.9rem; text-transform: uppercase; letter-spacing: 0.05em; }
    .file-table tr:hover { background: rgba(56, 189, 248, 0.03); }
    .file-name { font-weight: 600; color: #f1f5f9; display: flex; align-items: center; gap: 8px; }
    .file-size { color: #94a3b8; font-family: monospace; }
    
    .actions { display: flex; gap: 8px; align-items: center; }
    .btn {
      background: #38bdf8;
      color: #0f172a;
      font-weight: bold;
      border: none;
      padding: 6px 12px;
      border-radius: 6px;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.85rem;
      transition: all 0.2s;
      display: inline-flex;
      align-items: center;
      gap: 4px;
      text-decoration: none;
    }
    .btn:hover { opacity: 0.9; }
    .btn-danger { background: #ef4444; color: white; }
    .btn-danger:hover { background: #dc2626; }
    .btn-secondary { background: #475569; color: #f8fafc; }
    .btn-secondary:hover { background: #334155; }
    
    .upload-zone {
      border: 2px dashed #475569;
      border-radius: 8px;
      padding: 30px;
      text-align: center;
      cursor: pointer;
      transition: border-color 0.2s, background-color 0.2s;
      margin-bottom: 15px;
    }
    .upload-zone.dragover { border-color: #38bdf8; background: rgba(56, 189, 248, 0.05); }
    .upload-zone svg { width: 40px; height: 40px; fill: #64748b; margin-bottom: 10px; }
    .upload-zone p { margin: 0; color: #94a3b8; font-size: 0.95rem; }
    
    .progress-bar-container { display: none; background: #0f172a; border-radius: 6px; height: 12px; overflow: hidden; margin-top: 15px; border: 1px solid #334155; }
    .progress-bar { height: 100%; width: 0%; background: linear-gradient(90deg, #38bdf8, #06b6d4); transition: width 0.1s ease; }
    
    .status-msg { margin-top: 10px; font-size: 0.9rem; text-align: center; display: none; }
    .status-msg.success { color: #4ade80; display: block; }
    .status-msg.error { color: #f87171; display: block; }
  </style>
</head>
<body>
  <div class="header">
    <a href="/settings" class="back-btn" title="Back to Settings">
      <svg viewBox="0 0 24 24">
        <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/>
      </svg>
    </a>
    <h1>DeskBuddy File Manager</h1>
  </div>
  
  <div class="card">
    <h1>Upload Files</h1>
    <div class="upload-zone" id="dropZone" onclick="document.getElementById('fileInput').click()">
      <svg viewBox="0 0 24 24">
        <path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM14 13v4h-4v-4H7l5-5 5 5h-3z"/>
      </svg>
      <p>Drag and drop files here, or click to select files</p>
      <input type="file" id="fileInput" style="display: none;" multiple>
    </div>
    <div class="progress-bar-container" id="progressContainer">
      <div class="progress-bar" id="progressBar"></div>
    </div>
    <div class="status-msg" id="statusMsg"></div>
  </div>
  
  <div class="card">
    <h1>Files on Device</h1>
    
    <!-- Storage Usage Section -->
    <div style="margin-bottom: 20px; background: #0f172a; padding: 12px; border-radius: 8px; border: 1px solid #334155;">
      <div style="display: flex; justify-content: space-between; font-size: 0.9rem; margin-bottom: 6px;">
        <span style="color: #94a3b8;">Storage Usage</span>
        <span id="storageText" style="font-weight: bold; color: #38bdf8;">0 KB / 0 KB (0%)</span>
      </div>
      <div style="background: #1e293b; border-radius: 4px; height: 8px; overflow: hidden; border: 1px solid #334155;">
        <div id="storageBar" style="height: 100%; width: 0%; background: linear-gradient(90deg, #10b981, #38bdf8); transition: width 0.3s ease;"></div>
      </div>
      <div style="display: flex; justify-content: space-between; font-size: 0.8rem; margin-top: 6px; color: #64748b;">
        <span id="freeStorageText">Free: 0 KB</span>
        <span>LittleFS Partition</span>
      </div>
    </div>

    <table class="file-table">
      <thead>
        <tr>
          <th>Name</th>
          <th>Size</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody id="fileList">
        <tr>
          <td colspan="3" style="text-align: center; color: #64748b;">Loading files...</td>
        </tr>
      </tbody>
    </table>
  </div>

  <script>
    const dropZone = document.getElementById('dropZone');
    const fileInput = document.getElementById('fileInput');
    const progressContainer = document.getElementById('progressContainer');
    const progressBar = document.getElementById('progressBar');
    const statusMsg = document.getElementById('statusMsg');
    const fileList = document.getElementById('fileList');

    // Prevent defaults for drag events
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
      dropZone.addEventListener(eventName, preventDefaults, false);
      document.body.addEventListener(eventName, preventDefaults, false);
    });

    function preventDefaults(e) {
      e.preventDefault();
      e.stopPropagation();
    }

    // Highlight drop zone on drag over
    ['dragenter', 'dragover'].forEach(eventName => {
      dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'), false);
    });

    ['dragleave', 'drop'].forEach(eventName => {
      dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'), false);
    });

    // Handle dropped files
    dropZone.addEventListener('drop', handleDrop, false);
    fileInput.addEventListener('change', handleFilesSelect, false);

    function handleDrop(e) {
      const dt = e.dataTransfer;
      const files = dt.files;
      uploadFiles(files);
    }

    function handleFilesSelect(e) {
      const files = e.target.files;
      uploadFiles(files);
    }

    function showStatus(text, isSuccess) {
      statusMsg.innerText = text;
      statusMsg.className = 'status-msg ' + (isSuccess ? 'success' : 'error');
      statusMsg.style.display = 'block';
      setTimeout(() => {
        statusMsg.style.display = 'none';
      }, 5000);
    }

    function uploadFiles(files) {
      if (files.length === 0) return;
      
      progressContainer.style.display = 'block';
      progressBar.style.width = '0%';
      
      // Upload files sequentially
      let uploadIndex = 0;
      
      function uploadNext() {
        if (uploadIndex >= files.length) {
          showStatus('All files uploaded successfully!', true);
          progressContainer.style.display = 'none';
          loadFiles();
          return;
        }
        
        const file = files[uploadIndex];
        const formData = new FormData();
        formData.append('file', file, file.name);
        
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/upload', true);
        
        xhr.upload.addEventListener('progress', e => {
          if (e.lengthComputable) {
            const overallPercent = ((uploadIndex + (e.loaded / e.total)) / files.length) * 100;
            progressBar.style.width = overallPercent + '%';
          }
        });
        
        xhr.onload = function() {
          if (xhr.status === 200) {
            uploadIndex++;
            uploadNext();
          } else {
            showStatus('Failed to upload ' + file.name + ': ' + xhr.responseText, false);
            progressContainer.style.display = 'none';
          }
        };
        
        xhr.onerror = function() {
          showStatus('Network error during upload of ' + file.name, false);
          progressContainer.style.display = 'none';
        };
        
        xhr.send(formData);
      }
      
      uploadNext();
    }

    function formatBytes(bytes) {
      if (bytes === 0) return '0 Bytes';
      const k = 1024;
      const sizes = ['Bytes', 'KB', 'MB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    function loadFiles() {
      fetch('/files')
        .then(response => response.json())
        .then(data => {
          if (data.totalBytes !== undefined && data.usedBytes !== undefined) {
            const used = data.usedBytes;
            const total = data.totalBytes;
            const free = total - used;
            const pct = total > 0 ? ((used / total) * 100).toFixed(1) : 0;
            
            document.getElementById('storageText').innerText = `${formatBytes(used)} / ${formatBytes(total)} (${pct}%)`;
            document.getElementById('storageBar').style.width = `${pct}%`;
            document.getElementById('freeStorageText').innerText = `Free: ${formatBytes(free)}`;
          }
          
          fileList.innerHTML = '';
          if (data.files.length === 0) {
            fileList.innerHTML = '<tr><td colspan="3" style="text-align: center; color: #64748b;">No files on device</td></tr>';
            return;
          }
          data.files.forEach(file => {
            const tr = document.createElement('tr');
            
            const tdName = document.createElement('td');
            tdName.className = 'file-name';
            tdName.innerHTML = `
              <svg viewBox="0 0 24 24" style="width:16px; height:16px; fill:#38bdf8;">
                <path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/>
              </svg>
              <span>${file.name}</span>
            `;
            
            const tdSize = document.createElement('td');
            tdSize.className = 'file-size';
            tdSize.innerText = formatBytes(file.size);
            
            const tdActions = document.createElement('td');
            tdActions.className = 'actions';
            
            const btnDownload = document.createElement('a');
            btnDownload.className = 'btn btn-secondary';
            btnDownload.href = '/download?path=' + encodeURIComponent(file.name);
            btnDownload.innerHTML = `
              <svg viewBox="0 0 24 24" style="width:14px; height:14px; fill:currentColor;">
                <path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM17 13l-5 5-5-5h3V9h4v4h3z"/>
              </svg> Download
            `;
            
            const btnDelete = document.createElement('button');
            btnDelete.className = 'btn btn-danger';
            btnDelete.innerHTML = `
              <svg viewBox="0 0 24 24" style="width:14px; height:14px; fill:currentColor;">
                <path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/>
              </svg> Delete
            `;
            btnDelete.onclick = () => deleteFile(file.name);
            
            tdActions.appendChild(btnDownload);
            // Protect stats.json from accidental delete
            if (file.name !== '/stats.json') {
              tdActions.appendChild(btnDelete);
            } else {
              const disabledDelete = document.createElement('span');
              disabledDelete.style.color = '#64748b';
              disabledDelete.style.fontSize = '0.8rem';
              disabledDelete.style.fontStyle = 'italic';
              disabledDelete.innerText = 'System File';
              tdActions.appendChild(disabledDelete);
            }
            
            tr.appendChild(tdName);
            tr.appendChild(tdSize);
            tr.appendChild(tdActions);
            fileList.appendChild(tr);
          });
        })
        .catch(err => {
          fileList.innerHTML = '<tr><td colspan="3" style="text-align: center; color: #f87171;">Failed to load files</td></tr>';
        });
    }

    function deleteFile(name) {
      if (confirm('Are you sure you want to delete ' + name + '?')) {
        fetch('/delete-file?path=' + encodeURIComponent(name))
          .then(response => {
            if (response.ok) {
              showStatus('Deleted ' + name, true);
              loadFiles();
            } else {
              response.text().then(text => showStatus('Failed to delete ' + name + ': ' + text, false));
            }
          })
          .catch(err => showStatus('Error deleting file: ' + err, false));
      }
    }

    loadFiles();
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}



inline void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/radar-data", handleRadarData);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/reset-stats", handleResetStats);
  server.on("/factory-reset", handleFactoryReset);
  server.on("/trigger-event", handleTriggerEvent);
  server.on("/mqtt-history", handleMqttHistory);
  server.on("/mqtt-publish", handleMqttPublish);
  server.on("/reset-esp", []() {
    server.send(200, "text/plain", "Rebooting");
    delay(500);
    ESP.restart();
  });
  server.on("/files", HTTP_GET, handleFilesList);
  server.on("/file-manager", HTTP_GET, handleFileManager);
  server.on("/download", HTTP_GET, handleDownloadFile);
  server.on("/delete-file", HTTP_GET, handleDeleteFile);
  server.on("/upload", HTTP_POST, handleUploadResponse, handleFileUpload);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
}

#endif // WEB_H
