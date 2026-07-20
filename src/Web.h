#ifndef WEB_H
#define WEB_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ld2410.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "MqttService.h"

#include "State.h"

// Extern references for global state in main.cpp
extern PubSubClient mqttClient;
extern WebServer server;
extern Preferences preferences;
extern ld2410 radar;
#include <NTPClient.h>
extern NTPClient timeClient;
extern uint8_t getEffectivePresence(int dayIndex, int h);
extern int getLearnedWorkdayStart(int dayIndex);
extern int getLearnedWorkdayStart(const uint8_t* history);
extern int getLearnedWorkdayEnd(int dayIndex);
extern int getLearnedWorkdayEnd(const uint8_t* history);
extern int getLearnedLunchHour(int dayIndex);
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
  <link rel="icon" href="/favicon.ico">
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
    .ai-carousel {
      position: relative;
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 2.5rem;
      padding: 0 30px;
    }
    .ai-message {
      font-size: 1.2rem;
      font-style: italic;
      color: #38bdf8;
      text-align: center;
      line-height: 1.5;
      min-height: 2.5rem;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .ai-arrow {
      position: absolute;
      background: none;
      border: 1px solid #334155;
      color: #94a3b8;
      border-radius: 50%;
      width: 28px;
      height: 28px;
      font-size: 1rem;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: color 0.2s, border-color 0.2s;
      padding: 0;
    }
    .ai-arrow:hover { color: #38bdf8; border-color: #38bdf8; }
    .ai-arrow.hidden { display: none; }
    .ai-arrow-up { left: 0; top: 50%; transform: translateY(-50%); }
    .ai-arrow-down { right: 0; top: 50%; transform: translateY(-50%); }
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
    .settings-input {
      background: #0f172a;
      color: #f8fafc;
      border: 1px solid #334155;
      padding: 8px 12px;
      border-radius: 8px;
      outline: none;
      font-family: inherit;
      font-size: 0.95rem;
      transition: border-color 0.2s, box-shadow 0.2s;
    }
    .settings-input:focus {
      border-color: #38bdf8;
      box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.2);
    }
    .btn {
      background: #38bdf8;
      color: #0f172a;
      font-weight: bold;
      border: none;
      padding: 8px 16px;
      border-radius: 8px;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.95rem;
      transition: opacity 0.2s, transform 0.1s, background-color 0.2s;
      display: inline-flex;
      align-items: center;
      justify-content: center;
    }
    .btn:hover {
      opacity: 0.95;
    }
    .btn:active {
      transform: scale(0.98);
    }
    .btn-secondary {
      background: #334155;
      color: #94a3b8;
    }
    .btn-secondary:hover {
      background: #475569;
      color: #f8fafc;
    }
    .btn-purple {
      background: #7c3aed;
      color: #f5f3ff;
    }
    .btn-purple:hover {
      background: #8b5cf6;
    }
    .panel-header-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    .mqtt-status {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 0.8rem;
      color: #94a3b8;
      background: #0f172a;
      padding: 4px 10px;
      border-radius: 20px;
      border: 1px solid #334155;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      display: inline-block;
      box-shadow: 0 0 8px currentColor;
    }
    .status-connected {
      background: #10b981;
      color: #10b981;
    }
    .status-disconnected {
      background: #ef4444;
      color: #ef4444;
    }
    #mqttConsole::-webkit-scrollbar {
      width: 6px;
    }
    #mqttConsole::-webkit-scrollbar-track {
      background: #0f172a;
    }
    #mqttConsole::-webkit-scrollbar-thumb {
      background: #334155;
      border-radius: 3px;
    }
    #mqttConsole::-webkit-scrollbar-thumb:hover {
      background: #38bdf8;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>DeskBuddy Dashboard</h1>
    <div style="display: flex; gap: 15px; align-items: center;">
      <a href="/todo" style="color: #94a3b8; text-decoration: none; font-weight: 600; font-size: 0.95rem; display: flex; align-items: center; gap: 4px; transition: color 0.2s;" onmouseover="this.style.color='#38bdf8'" onmouseout="this.style.color='#94a3b8'" title="Task List">
        <svg style="width: 18px; height: 18px; fill: currentColor;" viewBox="0 0 24 24">
          <path d="M19 3h-4.18C14.4 1.84 13.3 1 12 1s-2.4.84-2.82 2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-7 0c.55 0 1 .45 1 1s-.45 1-1 1-1-.45-1-1 .45-1 1-1zm2 14H7v-2h7v2zm3-4H7v-2h10v2zm0-4H7V7h10v2z"/>
        </svg>
        TODO
      </a>
      <a href="/settings" class="cog-btn" title="Settings & Calibration">
        <svg viewBox="0 0 24 24">
          <path d="M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"/>
        </svg>
      </a>
    </div>
  </div>

  <div class="card ai-card">
    <div class="ai-badge" id="aiBadge">AI GENERATED</div>
    <div class="ai-carousel">
      <button class="ai-arrow ai-arrow-up hidden" id="aiArrowUp" onclick="aiNav(-1)">&#9664;</button>
      <div class="ai-message" id="aiMsg">Loading latest update...</div>
      <button class="ai-arrow ai-arrow-down hidden" id="aiArrowDown" onclick="aiNav(1)">&#9654;</button>
    </div>
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
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px;">
      <h1 style="margin: 0; text-align: left;">Learned Occupancy Pattern</h1>
      <select id="daySelect" class="settings-select" style="padding: 4px 8px; font-size: 0.85rem; height: 30px; background: #0f172a; border-radius: 6px; border: 1px solid #334155; color: #f8fafc;" onchange="updateHistoryDay()">
        <option value="-1">Today (Live)</option>
        <option value="0">Sunday</option>
        <option value="1">Monday</option>
        <option value="2">Tuesday</option>
        <option value="3">Wednesday</option>
        <option value="4">Thursday</option>
        <option value="5">Friday</option>
        <option value="6">Saturday</option>
      </select>
    </div>
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
    <h1 style="margin: 0 0 15px 0; text-align: left;">Average Sitting Time</h1>
    <div style="display: flex; align-items: flex-end; justify-content: space-between; height: 120px; padding: 10px 5px; background: #0f172a; border-radius: 8px; border: 1px solid #334155; margin-bottom: 5px;">
      <div id="sittingTimeChart" style="display: flex; align-items: flex-end; justify-content: space-around; width: 100%; height: 100%; gap: 4px;">
      </div>
    </div>
    <div style="display: flex; justify-content: space-around; font-size: 0.7rem; color: #64748b; padding: 4px 5px 0;">
      <span style="flex: 1; text-align: center;">Mon</span>
      <span style="flex: 1; text-align: center;">Tue</span>
      <span style="flex: 1; text-align: center;">Wed</span>
      <span style="flex: 1; text-align: center;">Thu</span>
      <span style="flex: 1; text-align: center;">Fri</span>
      <span style="flex: 1; text-align: center;">Sat</span>
      <span style="flex: 1; text-align: center;">Sun</span>
    </div>
  </div>

  <div class="card">
    <div class="panel-header-row">
      <h1 style="margin: 0; font-size: 1.5rem; color: #38bdf8;">MQTT Terminal</h1>
      <div class="mqtt-status" id="mqttStatus">
        <span class="status-dot status-disconnected"></span>
        <span id="mqttStatusText">Offline</span>
      </div>
    </div>
    <div style="display: flex; gap: 8px; margin-bottom: 12px; align-items: center; justify-content: space-between; flex-wrap: wrap;">
      <select id="mqttTopic" class="settings-input" style="flex: 1; min-width: 140px; text-align: left; box-sizing: border-box; height: 38px; background: #1e293b; color: #f8fafc; border: 1px solid #334155; border-radius: 6px; padding: 0 8px;">
        <option value="deskbuddy/debug/cmd">deskbuddy/debug/cmd</option>
        <option value="deskbuddy/message">deskbuddy/message</option>
      </select>
      <input type="text" id="mqttPayload" placeholder="Type command..." class="settings-input" style="flex: 2; min-width: 180px; text-align: left; box-sizing: border-box;" onkeydown="if(event.key === 'Enter') sendMqttMessage()">
      <button class="btn" onclick="sendMqttMessage()" style="padding: 6px 15px; height: 38px;">Send</button>
    </div>
    <div id="mqttConsole" style="background: rgba(15, 23, 42, 0.6); border-radius: 8px; border: 1px solid #334155; height: 180px; overflow-y: auto; padding: 10px; font-family: 'Fira Code', monospace; font-size: 0.85rem; color: #38bdf8; display: flex; flex-direction: column; gap: 6px; box-sizing: border-box; text-align: left;">
      <div style="color: #64748b; font-style: italic;">Console initialized. Awaiting MQTT updates...</div>
    </div>
    <div style="display: flex; gap: 8px; margin-top: 8px;">
      <button class="btn btn-secondary" onclick="clearMqttHistory()" style="padding: 5px 12px; font-size: 0.85rem;">Clear History</button>
    </div>
  </div>

  <script>
    let selectedDay = -1;
    let aiMessages = [];
    let aiIndex = 0;
    let lastAiMessage = "";
    const AI_MAX_HISTORY = 10;

    function aiNav(dir) {
      aiIndex = Math.max(0, Math.min(aiIndex + dir, aiMessages.length - 1));
      renderAiMessage();
    }

    function renderAiMessage() {
      let msg = aiMessages[aiIndex] || "No events recorded yet.";
      document.getElementById('aiMsg').innerText = msg;
      document.getElementById('aiArrowUp').classList.toggle('hidden', aiIndex === 0);
      document.getElementById('aiArrowDown').classList.toggle('hidden', aiIndex >= aiMessages.length - 1);
    }

    function updateHistoryDay() {
      selectedDay = parseInt(document.getElementById('daySelect').value);
      updateMetrics();
    }

    function updateMetrics() {
      let url = '/radar-data';
      if (selectedDay !== -1) {
        url += '?day=' + selectedDay;
      }
      fetch(url)
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
            let children = chartContainer.children;
            if (children.length === 24) {
              data.occupancyHistory.forEach((val, idx) => {
                let bar = children[idx];
                bar.style.height = val + '%';
                bar.title = 'Hour ' + idx + ': ' + val + '%';
              });
            } else {
              chartContainer.innerHTML = '';
              data.occupancyHistory.forEach((val, idx) => {
                let bar = document.createElement('div');
                bar.style.flex = '1';
                bar.style.margin = '0 1px';
                bar.style.height = val + '%';
                bar.style.background = 'linear-gradient(to top, #38bdf8, #f472b6)';
                bar.style.borderRadius = '2px 2px 0 0';
                bar.style.transition = 'height 0.3s ease';
                bar.title = 'Hour ' + idx + ': ' + val + '%';
                chartContainer.appendChild(bar);
              });
            }
          }

          // Update Average Sitting Time chart
          let sittingChart = document.getElementById('sittingTimeChart');
          if (sittingChart && data.avgSittingTimeWeekly) {
            let dayLabels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
            let maxMin = Math.max(...data.avgSittingTimeWeekly, 1);
            let existingBars = sittingChart.children;
            if (existingBars.length === 7) {
              data.avgSittingTimeWeekly.forEach((val, idx) => {
                let pct = Math.round((val / maxMin) * 100);
                let h = Math.floor(val / 60);
                let m = val % 60;
                let label = h > 0 ? h + 'h ' + (m > 0 ? m + 'm' : '') : m + 'm';
                let bar = existingBars[idx];
                bar.style.height = pct + '%';
                bar.querySelector('.sitLabel').innerText = label.trim();
                bar.title = dayLabels[idx] + ': ' + label.trim();
              });
            } else {
              sittingChart.innerHTML = '';
              data.avgSittingTimeWeekly.forEach((val, idx) => {
                let pct = Math.round((val / maxMin) * 100);
                let h = Math.floor(val / 60);
                let m = val % 60;
                let label = h > 0 ? h + 'h ' + (m > 0 ? m + 'm' : '') : m + 'm';
                let wrapper = document.createElement('div');
                wrapper.style.flex = '1';
                wrapper.style.display = 'flex';
                wrapper.style.flexDirection = 'column';
                wrapper.style.alignItems = 'center';
                wrapper.style.justifyContent = 'flex-end';
                wrapper.style.height = '100%';
                wrapper.title = dayLabels[idx] + ': ' + label.trim();
                let lbl = document.createElement('div');
                lbl.className = 'sitLabel';
                lbl.style.fontSize = '0.65rem';
                lbl.style.color = '#f8fafc';
                lbl.style.marginBottom = '3px';
                lbl.style.whiteSpace = 'nowrap';
                lbl.innerText = label.trim();
                let bar = document.createElement('div');
                bar.style.width = '70%';
                bar.style.height = pct + '%';
                bar.style.background = 'linear-gradient(to top, #38bdf8, #f472b6)';
                bar.style.borderRadius = '2px 2px 0 0';
                bar.style.transition = 'height 0.3s ease';
                wrapper.appendChild(lbl);
                wrapper.appendChild(bar);
                sittingChart.appendChild(wrapper);
              });
            }
          }
          
          // AI message history (carousel)
          let aiMsg = document.getElementById('aiMsg');
          let loadingContainer = document.getElementById('aiLoading');
          let arrowUp = document.getElementById('aiArrowUp');
          let arrowDown = document.getElementById('aiArrowDown');
          if (data.aiLoading) {
            aiMsg.style.display = "none";
            arrowUp.classList.add('hidden');
            arrowDown.classList.add('hidden');
            loadingContainer.style.display = "flex";
          } else {
            loadingContainer.style.display = "none";
            aiMsg.style.display = "flex";
            if (data.aiMessage && data.aiMessage !== lastAiMessage) {
              lastAiMessage = data.aiMessage;
              if (aiMessages.indexOf(data.aiMessage) === -1) {
                aiMessages.unshift(data.aiMessage);
                if (aiMessages.length > AI_MAX_HISTORY) aiMessages.pop();
              }
              aiIndex = 0;
              renderAiMessage();
            } else {
              renderAiMessage();
            }
          }
          
          let isAi = data.isAiGenerated;
          document.getElementById('aiBadge').style.display = isAi ? "block" : "none";
          
          // Update MQTT Status
          let statusDot = document.querySelector('#mqttStatus .status-dot');
          let statusText = document.getElementById('mqttStatusText');
          if (data.mqttConnected) {
            statusDot.className = "status-dot status-connected";
            statusText.innerText = data.mqttBroker ? `Broker: ${data.mqttBroker}` : 'Connected';
          } else {
            statusDot.className = "status-dot status-disconnected";
            statusText.innerText = 'Disconnected';
          }
          
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

    function clearMqttHistory() {
      fetch('/mqtt-clear', { method: 'POST' })
        .then(() => {
          lastMqttCount = -1;
        })
        .catch(err => console.error('Failed to clear MQTT history:', err));
    }
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleTodo() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <link rel="icon" href="/favicon.ico">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy TODO</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .header { display: flex; justify-content: space-between; align-items: center; width: 100%; max-width: 600px; margin-bottom: 20px; padding: 0 10px; box-sizing: border-box; }
    .header h1 { margin: 0; font-size: 1.6rem; color: #38bdf8; font-weight: 800; letter-spacing: -0.025em; }
    .back-btn {
      color: #94a3b8;
      text-decoration: none;
      font-weight: 600;
      font-size: 0.95rem;
      display: flex;
      align-items: center;
      gap: 6px;
      transition: color 0.2s;
    }
    .back-btn:hover { color: #38bdf8; }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px 0; width: 100%; max-width: 600px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    h2 { font-size: 1.25rem; color: #38bdf8; margin-top: 0; margin-bottom: 15px; border-bottom: 1px solid #334155; padding-bottom: 8px; }
    .input-group { display: flex; gap: 8px; margin-bottom: 15px; }
    input[type="text"] { flex: 1; background: #0f172a; border: 1px solid #334155; border-radius: 6px; padding: 8px 12px; color: #f8fafc; font-size: 0.95rem; outline: none; }
    input[type="text"]:focus { border-color: #38bdf8; }
    button { background: #38bdf8; color: #0f172a; border: none; border-radius: 6px; padding: 8px 16px; font-weight: bold; cursor: pointer; transition: opacity 0.2s; }
    button:hover { opacity: 0.9; }
    .task-list { display: flex; flex-direction: column; gap: 4px; height: 364px; overflow-y: auto; padding: 8px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; box-sizing: border-box; margin-bottom: 15px; }
    .task-list::-webkit-scrollbar { width: 6px; }
    .task-list::-webkit-scrollbar-track { background: #0f172a; }
    .task-list::-webkit-scrollbar-thumb { background: #334155; border-radius: 3px; }
    .task-list::-webkit-scrollbar-thumb:hover { background: #38bdf8; }
    .task-item { height: 40px; box-sizing: border-box; display: flex; align-items: center; justify-content: space-between; background: none; border-bottom: 1px solid #1e293b; padding: 0 8px; gap: 8px; flex-shrink: 0; }
    .task-item:last-child { border-bottom: none; }
    .task-item.completed { opacity: 0.5; }
    .task-item.completed span { text-decoration: line-through; }
    .task-left { display: flex; align-items: center; gap: 10px; flex: 1; min-width: 0; }
    .task-left input[type="checkbox"] { width: 18px; height: 18px; cursor: pointer; accent-color: #38bdf8; flex-shrink: 0; }
    .task-text { font-size: 0.95rem; color: #e2e8f0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .delete-btn { background: none; border: none; color: #9ca3af; cursor: pointer; padding: 4px; display: flex; align-items: center; transition: color 0.2s; flex-shrink: 0; }
    .delete-btn:hover { color: #6b7280; }
    .delete-btn svg { width: 18px; height: 18px; fill: currentColor; }
    .empty-state { display: flex; align-items: center; justify-content: center; height: 100%; color: #64748b; font-size: 0.9rem; }
  </style>
</head>
<body>
  <div class="header">
    <a href="/" class="back-btn">
      <svg style="width: 18px; height: 18px; fill: currentColor;" viewBox="0 0 24 24">
        <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/>
      </svg>
      Dashboard
    </a>
    <h1>TODO Tasks</h1>
  </div>

  <div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 8px; margin-bottom: 15px;">
      <h2 style="margin: 0; border: none; padding: 0;">Daily Tasks</h2>
      <select id="dayHistorySelect" onchange="renderLists()" style="background: #0f172a; border: 1px solid #334155; color: #38bdf8; border-radius: 6px; padding: 4px 8px; font-size: 0.85rem; font-weight: bold; outline: none; cursor: pointer;"></select>
    </div>
    <div class="task-list" id="dailyList"></div>
    <div class="input-group" style="flex-wrap: wrap; gap: 8px;">
      <input type="text" id="dailyInput" placeholder="Add daily task..." style="flex: 1; min-width: 150px;">
      <select id="dailyHour" style="background: #0f172a; border: 1px solid #334155; color: #f8fafc; border-radius: 6px; padding: 8px; font-size: 0.85rem; outline: none;" title="Due Hour"></select>
      <select id="dailyMinute" style="background: #0f172a; border: 1px solid #334155; color: #f8fafc; border-radius: 6px; padding: 8px; font-size: 0.85rem; outline: none;" title="Due Minute"></select>
      <label style="display: flex; align-items: center; gap: 6px; font-size: 0.85rem; color: #cbd5e1; cursor: pointer; user-select: none;">
        <input type="checkbox" id="dailyRecurrent" style="accent-color: #38bdf8; width: 16px; height: 16px; cursor: pointer;">
        Recurrent
      </label>
      <button onclick="addTask('daily')">Add</button>
    </div>
  </div>

  <div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 8px; margin-bottom: 15px;">
      <h2 style="margin: 0; border: none; padding: 0;">Monthly Tasks</h2>
      <select id="monthHistorySelect" onchange="renderLists()" style="background: #0f172a; border: 1px solid #334155; color: #38bdf8; border-radius: 6px; padding: 4px 8px; font-size: 0.85rem; font-weight: bold; outline: none; cursor: pointer;"></select>
    </div>
    <div class="task-list" id="monthlyList"></div>
    <div class="input-group" style="flex-wrap: wrap; gap: 8px;">
      <input type="text" id="monthlyInput" placeholder="Add monthly task..." style="flex: 1; min-width: 150px;">
      <select id="monthlyDay" style="background: #0f172a; border: 1px solid #334155; color: #f8fafc; border-radius: 6px; padding: 8px; font-size: 0.85rem; outline: none;" title="Day of Month"></select>
      <select id="monthlyMonth" style="background: #0f172a; border: 1px solid #334155; color: #f8fafc; border-radius: 6px; padding: 8px; font-size: 0.85rem; outline: none;" title="Target Month"></select>
      <label style="display: flex; align-items: center; gap: 6px; font-size: 0.85rem; color: #cbd5e1; cursor: pointer; user-select: none;">
        <input type="checkbox" id="monthlyRecurrent" style="accent-color: #38bdf8; width: 16px; height: 16px; cursor: pointer;">
        Recurrent
      </label>
      <button onclick="addTask('monthly')">Add</button>
    </div>
  </div>

  <script>
    let tasks = { daily: [], monthly: [] };
    let time24h = true;

    function formatHour(h) {
      if (time24h) {
        return String(h).padStart(2, '0');
      } else {
        const ampm = h >= 12 ? 'PM' : 'AM';
        const displayH = h % 12 === 0 ? 12 : h % 12;
        return `${displayH} ${ampm}`;
      }
    }

    function formatTimeDeadline(h, m) {
      const minStr = String(m).padStart(2, '0');
      if (time24h) {
        return `${String(h).padStart(2, '0')}:${minStr}`;
      } else {
        const ampm = h >= 12 ? 'PM' : 'AM';
        const displayH = h % 12 === 0 ? 12 : h % 12;
        return `${displayH}:${minStr} ${ampm}`;
      }
    }

    function populateHourDropdown() {
      const dailyHourSel = document.getElementById('dailyHour');
      if (!dailyHourSel) return;
      dailyHourSel.innerHTML = '';
      for (let h = 0; h < 24; h++) {
        let opt = document.createElement('option');
        opt.value = h;
        opt.innerText = formatHour(h);
        dailyHourSel.appendChild(opt);
      }
      dailyHourSel.value = 12; // default to midday
    }

    async function loadTasks() {
      try {
        const statsRes = await fetch('/radar-data');
        if (statsRes.ok) {
          const stats = await statsRes.json();
          time24h = stats.time24h !== undefined ? stats.time24h : true;
        }
      } catch (err) {
        console.error('Error loading stats:', err);
      }
      populateHourDropdown();

      try {
        const response = await fetch('/api/tasks');
        if (response.ok) {
          tasks = await response.json();
          renderLists();
        }
      } catch (err) {
        console.error('Error loading tasks:', err);
      }
    }

    async function saveTasks() {
      try {
        await fetch('/api/tasks/save', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(tasks)
        });
      } catch (err) {
        console.error('Error saving tasks:', err);
      }
    }

    function renderLists() {
      renderList('daily', 'dailyList');
      renderList('monthly', 'monthlyList');
    }

    function renderList(type, elementId) {
      const container = document.getElementById(elementId);
      container.innerHTML = '';
      
      const list = tasks[type] || [];
      
      if (type === 'daily') {
        const selectedDateStr = document.getElementById('dayHistorySelect').value;
        
        let activeTasks = [];
        list.forEach((task, index) => {
          let isActive = false;
          let isCompleted = false;
          
          if (task.recurrent) {
            const startD = task.startDate || "";
            const endD = task.endDate || "";
            isActive = (selectedDateStr >= startD) && (!endD || selectedDateStr < endD);
            isCompleted = task.completedDates && task.completedDates.includes(selectedDateStr);
          } else {
            isActive = (!task.targetDate || task.targetDate === selectedDateStr);
            isCompleted = task.completed;
          }
          
          if (isActive) {
            activeTasks.push({ task: task, originalIndex: index, isCompleted: isCompleted });
          }
        });
        
        // Sort active tasks: uncompleted first, then completed. Secondary sort by hour and minute.
        activeTasks.sort((a, b) => {
          if (a.isCompleted !== b.isCompleted) {
            return a.isCompleted ? 1 : -1;
          }
          if (a.task.hour !== b.task.hour) {
            return a.task.hour - b.task.hour;
          }
          return (a.task.minute || 0) - (b.task.minute || 0);
        });

        if (activeTasks.length === 0) {
          container.innerHTML = '<div class="empty-state">No tasks scheduled for this day.</div>';
          return;
        }

        activeTasks.forEach(itemInfo => {
          const task = itemInfo.task;
          const index = itemInfo.originalIndex;
          const isCompleted = itemInfo.isCompleted;
          const tMin = task.minute || 0;
          
          const now = new Date();
          const curDateStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
          const currentHour = now.getHours();
          const currentMinute = now.getMinutes();
          
          let isOverdue = false;
          if (!isCompleted) {
            if (selectedDateStr < curDateStr) {
              isOverdue = true;
            } else if (selectedDateStr === curDateStr) {
              if (task.hour < currentHour || (task.hour === currentHour && tMin < currentMinute)) {
                isOverdue = true;
              }
            }
          }
          
          const badgeColor = isOverdue ? '#f43f5e' : '#9ca3af';
          const recurrentIndicator = task.recurrent ? `<span style="color: #3b82f6; font-weight: bold; font-size: 0.85rem; margin-right: 8px;" title="Recurrent">R</span>` : '';
          
          const item = document.createElement('div');
          item.className = `task-item ${isCompleted ? 'completed' : ''}`;
          
          let deadlineHtml = `<span class="task-deadline" style="font-size: 0.75rem; color: ${badgeColor}; background: #27272a; padding: 2px 6px; border-radius: 4px; font-weight: 600; margin-left: 8px;">Due: ${formatTimeDeadline(task.hour, tMin)}</span>`;

          item.innerHTML = `
            <div class="task-left">
              <input type="checkbox" ${isCompleted ? 'checked' : ''} onchange="toggleTask('${type}', ${index})">
              <span class="task-text">${escapeHtml(task.text)}</span>
              ${deadlineHtml}
            </div>
            <div style="display: flex; align-items: center;">
              ${recurrentIndicator}
              <button class="delete-btn" onclick="deleteTask('${type}', ${index})" title="Delete task">
                <svg viewBox="0 0 24 24">
                  <path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/>
                </svg>
              </button>
            </div>
          `;
          container.appendChild(item);
        });
      } else {
        const selectedMonthStr = document.getElementById('monthHistorySelect').value;
        const [selYear, selMonth] = selectedMonthStr.split('-').map(Number);
        
        let activeTasks = [];
        list.forEach((task, index) => {
          let isActive = false;
          let isCompleted = false;
          
          if (task.recurrent) {
            const startM = task.startMonth || "";
            const endM = task.endMonth || "";
            isActive = (selectedMonthStr >= startM) && (!endM || selectedMonthStr < endM);
            isCompleted = task.completedMonths && task.completedMonths.includes(selectedMonthStr);
          } else {
            isActive = (!task.year || (task.year === selYear && task.month === selMonth));
            isCompleted = task.completed;
          }
          
          if (isActive) {
            activeTasks.push({ task: task, originalIndex: index, isCompleted: isCompleted });
          }
        });
        
        // Sort active tasks: uncompleted first, then completed. Secondary sort by day.
        activeTasks.sort((a, b) => {
          if (a.isCompleted !== b.isCompleted) {
            return a.isCompleted ? 1 : -1;
          }
          return a.task.day - b.task.day;
        });

        if (activeTasks.length === 0) {
          container.innerHTML = '<div class="empty-state">No tasks scheduled for this month.</div>';
          return;
        }
        
        activeTasks.forEach(itemInfo => {
          const task = itemInfo.task;
          const index = itemInfo.originalIndex;
          const isCompleted = itemInfo.isCompleted;
          
          const now = new Date();
          const curMonthStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}`;
          const currentDay = now.getDate();
          
          let isOverdue = false;
          if (!isCompleted) {
            if (selectedMonthStr < curMonthStr) {
              isOverdue = true;
            } else if (selectedMonthStr === curMonthStr) {
              if (task.day < currentDay) {
                isOverdue = true;
              }
            }
          }
          
          const badgeColor = isOverdue ? '#f43f5e' : '#9ca3af';
          const recurrentIndicator = task.recurrent ? `<span style="color: #3b82f6; font-weight: bold; font-size: 0.85rem; margin-right: 8px;" title="Recurrent">R</span>` : '';
          
          const item = document.createElement('div');
          item.className = `task-item ${isCompleted ? 'completed' : ''}`;
          
          let deadlineHtml = `<span class="task-deadline" style="font-size: 0.75rem; color: ${badgeColor}; background: #27272a; padding: 2px 6px; border-radius: 4px; font-weight: 600; margin-left: 8px;">Due: Day ${task.day}</span>`;

          item.innerHTML = `
            <div class="task-left">
              <input type="checkbox" ${isCompleted ? 'checked' : ''} onchange="toggleTask('${type}', ${index})">
              <span class="task-text">${escapeHtml(task.text)}</span>
              ${deadlineHtml}
            </div>
            <div style="display: flex; align-items: center;">
              ${recurrentIndicator}
              <button class="delete-btn" onclick="deleteTask('${type}', ${index})" title="Delete task">
                <svg viewBox="0 0 24 24">
                  <path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/>
                </svg>
              </button>
            </div>
          `;
          container.appendChild(item);
        });
      }
    }

    function addTask(type) {
      const input = document.getElementById(`${type}Input`);
      const text = input.value.trim();
      if (!text) return;
      
      if (!tasks[type]) tasks[type] = [];
      
      if (type === 'daily') {
        const hour = parseInt(document.getElementById('dailyHour').value);
        const minute = parseInt(document.getElementById('dailyMinute').value);
        const isRecurrent = document.getElementById('dailyRecurrent').checked;
        const selectedDateStr = document.getElementById('dayHistorySelect').value;
        const now = new Date();
        const curDateStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
        
        if (isRecurrent) {
          tasks[type].push({
            text: text,
            hour: hour,
            minute: minute,
            recurrent: true,
            startDate: selectedDateStr,
            completedDates: []
          });
        } else {
          tasks[type].push({
            text: text,
            hour: hour,
            minute: minute,
            recurrent: false,
            targetDate: selectedDateStr,
            completed: false
          });
        }
        document.getElementById('dailyRecurrent').checked = false;
      } else if (type === 'monthly') {
        const day = parseInt(document.getElementById('monthlyDay').value);
        const selectedMonth = parseInt(document.getElementById('monthlyMonth').value);
        const isRecurrent = document.getElementById('monthlyRecurrent').checked;
        
        if (isRecurrent) {
          const now = new Date();
          const currentYear = now.getFullYear();
          const currentMonth = now.getMonth() + 1; // 1-12
          const year = (selectedMonth < currentMonth) ? currentYear + 1 : currentYear;
          const startMonthStr = `${year}-${String(selectedMonth).padStart(2, '0')}`;
          
          tasks[type].push({
            text: text,
            day: day,
            recurrent: true,
            startMonth: startMonthStr,
            completedMonths: []
          });
        } else {
          const now = new Date();
          const currentYear = now.getFullYear();
          const currentMonth = now.getMonth() + 1; // 1-12
          const year = (selectedMonth < currentMonth) ? currentYear + 1 : currentYear;
          
          tasks[type].push({
            text: text,
            day: day,
            month: selectedMonth,
            year: year,
            recurrent: false,
            completed: false
          });
        }
        document.getElementById('monthlyRecurrent').checked = false;
      }
      
      input.value = '';
      
      renderLists();
      saveTasks();
    }

    function toggleTask(type, index) {
      if (type === 'daily') {
        const task = tasks[type][index];
        const selectedDateStr = document.getElementById('dayHistorySelect').value;
        if (task.recurrent) {
          if (!task.completedDates) task.completedDates = [];
          const idx = task.completedDates.indexOf(selectedDateStr);
          if (idx > -1) {
            task.completedDates.splice(idx, 1);
          } else {
            task.completedDates.push(selectedDateStr);
          }
        } else {
          task.completed = !task.completed;
        }
      } else {
        const task = tasks[type][index];
        const selectedMonthStr = document.getElementById('monthHistorySelect').value;
        if (task.recurrent) {
          if (!task.completedMonths) task.completedMonths = [];
          const idx = task.completedMonths.indexOf(selectedMonthStr);
          if (idx > -1) {
            task.completedMonths.splice(idx, 1);
          } else {
            task.completedMonths.push(selectedMonthStr);
          }
        } else {
          task.completed = !task.completed;
        }
      }
      renderLists();
      saveTasks();
    }

    function deleteTask(type, index) {
      if (type === 'daily') {
        const task = tasks[type][index];
        const selectedDateStr = document.getElementById('dayHistorySelect').value;
        if (task.recurrent) {
          if (!task.completedDates || task.completedDates.length === 0 || task.startDate === selectedDateStr) {
            tasks[type].splice(index, 1);
          } else {
            task.endDate = selectedDateStr;
          }
        } else {
          tasks[type].splice(index, 1);
        }
      } else if (type === 'monthly') {
        const task = tasks[type][index];
        const selectedMonthStr = document.getElementById('monthHistorySelect').value;
        if (task.recurrent) {
          if (!task.completedMonths || task.completedMonths.length === 0 || task.startMonth === selectedMonthStr) {
            tasks[type].splice(index, 1);
          } else {
            task.endMonth = selectedMonthStr;
          }
        } else {
          tasks[type].splice(index, 1);
        }
      }
      renderLists();
      saveTasks();
    }

    function escapeHtml(text) {
      return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
    }

    function populateMonthSelect() {
      const select = document.getElementById('monthHistorySelect');
      select.innerHTML = '';
      const monthNames = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
      const now = new Date();
      let year = now.getFullYear();
      let month = now.getMonth(); // 0-11
      
      for (let i = 0; i < 12; i++) {
        let mStr = `${year}-${String(month + 1).padStart(2, '0')}`;
        let label = `${monthNames[month]} ${year}`;
        let opt = document.createElement('option');
        opt.value = mStr;
        opt.innerText = label;
        select.appendChild(opt);
        
        month--;
        if (month < 0) {
          month = 11;
          year--;
        }
      }
    }

    function populateDaySelect() {
      const select = document.getElementById('dayHistorySelect');
      select.innerHTML = '';
      const daysOfWeek = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
      const now = new Date();
      
      // Sliding 15-day window: 7 future days, today, and 7 past days
      for (let i = -7; i <= 7; i++) {
        let tempDate = new Date();
        tempDate.setDate(now.getDate() - i);
        let y = tempDate.getFullYear();
        let m = String(tempDate.getMonth() + 1).padStart(2, '0');
        let d = String(tempDate.getDate()).padStart(2, '0');
        let dStr = `${y}-${m}-${d}`;
        
        let label = "";
        if (i === 0) label = "Today";
        else if (i === 1) label = "Yesterday";
        else if (i === -1) label = "Tomorrow";
        else {
          label = `${daysOfWeek[tempDate.getDay()]} (${tempDate.getDate()}/${tempDate.getMonth() + 1})`;
          if (i < 0) {
            label += " [Future]";
          }
        }
        
        let opt = document.createElement('option');
        opt.value = dStr;
        opt.innerText = label;
        select.appendChild(opt);
      }
      
      // Set default value to Today
      const todayStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
      select.value = todayStr;
    }

    function initUi() {
      const dailyMinSel = document.getElementById('dailyMinute');
      dailyMinSel.innerHTML = '';
      for (let m = 0; m < 60; m += 5) {
        let opt = document.createElement('option');
        opt.value = m;
        opt.innerText = String(m).padStart(2, '0');
        dailyMinSel.appendChild(opt);
      }
      
      const daySel = document.getElementById('monthlyDay');
      daySel.innerHTML = '';
      for (let i = 1; i <= 31; i++) {
        let opt = document.createElement('option');
        opt.value = i;
        opt.innerText = `Day ${i}`;
        daySel.appendChild(opt);
      }
      
      const monthSel = document.getElementById('monthlyMonth');
      monthSel.innerHTML = '';
      const monthNames = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
      monthNames.forEach((name, idx) => {
        let opt = document.createElement('option');
        opt.value = idx + 1; // 1-12
        opt.innerText = name;
        monthSel.appendChild(opt);
      });
      
      const now = new Date();
      dailyMinSel.value = 0;
      daySel.value = now.getDate();
      monthSel.value = now.getMonth() + 1;
      
      populateMonthSelect();
      populateDaySelect();
    }

    window.onload = function() {
      initUi();
      loadTasks();
    };
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleGetTasks() {
  if (LittleFS.exists("/todo.json")) {
    fs::File file = LittleFS.open("/todo.json", "r");
    server.streamFile(file, "application/json");
    file.close();
  } else {
    server.send(200, "application/json", "{\"daily\":[],\"monthly\":[]}");
  }
}

inline void handleSaveTasks() {
  if (server.hasArg("plain")) {
    String payload = server.arg("plain");
    fs::File file = LittleFS.open("/todo.json", "w");
    if (file) {
      file.print(payload);
      file.close();
      server.send(200, "application/json", "{\"status\":\"success\"}");
    } else {
      server.send(500, "text/plain", "Failed to open todo.json for writing");
    }
  } else {
    server.send(400, "text/plain", "Bad Request: No payload");
  }
}

inline void handleSettings() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <link rel="icon" href="/favicon.ico">
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
          <option value="2">Sweet</option>
          <option value="3">Friend</option>
        </select>
      </div>
      <div class="metric">
        <span class="label">Clock Face Style</span>
        <select name="clockFace" id="clockFaceSelect" class="settings-select">
          <option value="0">Default Digital</option>
          <option value="1">Minimalist</option>
          <option value="2">HiTech</option>
          <option value="3">DEV Mode</option>
          <option value="4">Aviator</option>
          <option value="5">Deskbuddy</option>
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
      <span class="label">Total Writes</span>
      <span class="value" id="fsWrites">0</span>
    </div>
    <div class="metric">
      <span class="label">Writes Today</span>
      <span class="value" id="fsWritesToday">0</span>
    </div>
    <div class="metric">
      <span class="label">Storage Used</span>
      <span class="value" id="fsStorage">—</span>
    </div>
    <div style="width: 100%; background: #334155; height: 6px; border-radius: 3px; margin-top: 4px; overflow: hidden;">
      <div id="fsStorageBar" style="width: 0%; background: #38bdf8; height: 100%; transition: width 0.3s ease;"></div>
    </div>
    <div class="metric" style="margin-top: 8px;">
      <span class="label">Free Space</span>
      <span class="value" id="fsFree">—</span>
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
    <div style="margin-bottom: 15px;">
      <a href="/credentials" class="btn" style="background: #6366f1; color: white; display: inline-flex; width: 100%; box-sizing: border-box; justify-content: center; align-items: center; gap: 6px; padding: 10px 12px; text-decoration: none;">
        <svg viewBox="0 0 24 24" style="width: 18px; height: 18px; fill: currentColor;">
          <path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/>
        </svg>
        Network & Credentials
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
          <option value="8">Excessive Breaks (8)</option>
          <option value="9">Goal Completed (9)</option>
          <option value="10">Journal Task Overview (10)</option>
          <option value="11">Nagging Overdue Alert (11)</option>
          <option value="12">Task Due Reminder (12)</option>
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
          setTxt('fsWritesToday', data.fsWritesToday || 0);

          let totalBytes = data.fsTotalBytes || 1048576;
          let usedBytes = data.fsUsedBytes || 0;
          let freeBytes = totalBytes - usedBytes;

          function fmtFS(b) { return b < 1024 ? b + ' B' : (b / 1024).toFixed(1) + ' KB'; }
          setTxt('fsStorage', fmtFS(usedBytes) + ' / ' + fmtFS(totalBytes));
          setTxt('fsFree', fmtFS(freeBytes) + ' free');
          let storagePct = totalBytes > 0 ? (usedBytes / totalBytes * 100) : 0;
          let sBar = document.getElementById('fsStorageBar');
          if (sBar) sBar.style.width = storagePct + '%';

          let writesToday = data.fsWritesToday || 0;
          let minutesElapsed = data.todayMinutesElapsed || 0;

          let writesPerDay = 10;
          if (minutesElapsed > 0) {
            writesPerDay = writesToday * 1440 / minutesElapsed;
          }
          if (writesPerDay < 10) writesPerDay = 10;

          let physicalWritesPerDay = writesPerDay * 2;
          let totalPhysicalWrites = writes * 2;

          let freeBlocks = Math.max(1, Math.floor(freeBytes / 4096));
          let totalLifetimeWrites = freeBlocks * 100000;
          let remainingWrites = totalLifetimeWrites - totalPhysicalWrites;
          if (remainingWrites < 0) remainingWrites = 0;

          let remainingDays = remainingWrites / physicalWritesPerDay;
          let years = remainingDays / 365.25;
          let lifespanText = "";
          if (years > 100) {
            lifespanText = "> 100 Years";
          } else {
            lifespanText = years.toFixed(1) + " Years";
          }
          setTxt('fsLifespan', lifespanText + ' (' + physicalWritesPerDay.toFixed(1) + ' phys. writes/day)');

          let healthPercent = totalLifetimeWrites > 0 ? (remainingWrites / totalLifetimeWrites) * 100 : 100;
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
            location.reload();
          })
          .catch(err => alert("Failed to reset daily stats."));
      }
    }

    function resetESP() {
      if (confirm("Are you sure you want to reboot DeskBuddy?")) {
        fetch('/reset-esp')
          .then(response => {
            setTimeout(() => { window.location.href = "/"; }, 5000);
          })
          .catch(err => alert("Failed to trigger reboot."));
      }
    }

    function factoryReset() {
      if (confirm("WARNING: This will clear all settings, configurations, and historical daily stats. Are you sure you want to perform a factory reset?")) {
        fetch('/factory-reset')
          .then(response => {
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
      if (type === "1" || type === "8") detail = "15m";
      if (type === "3") detail = "25m";
      if (type === "12") detail = "Drink Water";
      
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

inline void handleCredentials() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <link rel="icon" href="/favicon.ico">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Credentials</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .settings-header { display: flex; align-items: center; width: 100%; max-width: 650px; margin-bottom: 15px; gap: 15px; padding: 0 10px; box-sizing: border-box; }
    .settings-header h1 { margin: 0; font-size: 1.6rem; color: #38bdf8; font-weight: 800; }
    .back-btn { color: #94a3b8; cursor: pointer; transition: color 0.2s, transform 0.2s; display: flex; align-items: center; justify-content: center; }
    .back-btn:hover { color: #38bdf8; transform: translateX(-3px); }
    .back-btn svg { width: 24px; height: 24px; fill: currentColor; }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 650px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    h1 { font-size: 1.5rem; color: #38bdf8; text-align: center; margin-bottom: 20px; }
    .metric { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #334155; align-items: center; }
    .metric:last-child { border: none; }
    .label { color: #94a3b8; }
    .settings-input { background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 6px 10px; border-radius: 6px; width: 100%; text-align: left; font-family: inherit; font-size: 0.95rem; box-sizing: border-box; }
    .settings-input-short { background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 6px 10px; border-radius: 6px; width: 120px; text-align: right; font-family: inherit; font-size: 0.95rem; }
    .btn { background: #38bdf8; color: #0f172a; font-weight: bold; border: none; padding: 10px 20px; border-radius: 6px; cursor: pointer; font-family: inherit; font-size: 0.95rem; transition: opacity 0.2s; }
    .btn:hover { opacity: 0.9; }
    .btn-scan { background: #6366f1; color: white; padding: 6px 14px; border-radius: 6px; border: none; cursor: pointer; font-family: inherit; font-size: 0.85rem; font-weight: bold; white-space: nowrap; transition: opacity 0.2s; }
    .btn-scan:hover { opacity: 0.85; }
    .btn-scan:disabled { opacity: 0.5; cursor: wait; }
    .field-group { padding: 10px 0; border-bottom: 1px solid #334155; }
    .field-group:last-child { border: none; }
    .field-label { color: #94a3b8; font-size: 0.9rem; margin-bottom: 4px; display: block; }
    .field-help { color: #64748b; font-size: 0.75rem; margin-top: 3px; display: block; }
    .notice { background: #1e3a5f; border: 1px solid #38bdf8; border-radius: 8px; padding: 10px 15px; margin: 10px; width: 100%; max-width: 650px; box-sizing: border-box; font-size: 0.85rem; color: #38bdf8; text-align: center; }
    .ssid-row { display: flex; gap: 8px; align-items: flex-end; }
    .ssid-row input { flex: 1; }
    #apList { max-height: 0; overflow: hidden; transition: max-height 0.3s ease; margin-top: 0; }
    #apList.open { max-height: 260px; overflow-y: auto; margin-top: 8px; border: 1px solid #334155; border-radius: 6px; background: #0f172a; }
    .ap-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 12px; cursor: pointer; border-bottom: 1px solid #1e293b; font-size: 0.9rem; transition: background 0.15s; }
    .ap-item:last-child { border: none; }
    .ap-item:hover { background: #1e293b; }
    .ap-item .ap-name { color: #f8fafc; }
    .ap-item .ap-rssi { color: #64748b; font-size: 0.8rem; }
    .ap-item .ap-locked { color: #f59e0b; font-size: 0.75rem; margin-left: 6px; }
  </style>
</head>
<body>
  <div class="settings-header">
    <a href="/settings" class="back-btn" title="Back to Settings">
      <svg viewBox="0 0 24 24">
        <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/>
      </svg>
    </a>
    <h1>Network & Credentials</h1>
  </div>

  <div class="notice">
    WiFi &amp; MQTT changes require a reboot to take effect.
  </div>

  <form action="/save-credentials" method="POST">
    <div class="card">
      <h1>WiFi</h1>
      <div class="field-group">
        <label class="field-label">Network Name (SSID)</label>
        <div class="ssid-row">
          <input type="text" name="wifiSsid" id="wifiSsid" class="settings-input" placeholder="WiFi SSID" autocomplete="off">
          <button type="button" class="btn-scan" id="scanBtn" onclick="scanWifi()">Scan</button>
        </div>
        <div id="apList"></div>
      </div>
      <div class="field-group">
        <label class="field-label">Password</label>
        <input type="password" name="wifiPass" id="wifiPass" class="settings-input" placeholder="WiFi Password">
      </div>
      <div class="field-group">
        <div class="metric" style="border: none; padding: 4px 0;">
          <span class="label">Use Static IP</span>
          <select name="wifiStatic" id="wifiStaticSelect" class="settings-input-short" style="width: 80px;" onchange="toggleStaticFields()">
            <option value="1">Yes</option>
            <option value="0">No</option>
          </select>
        </div>
        <div id="staticFields">
          <div style="display: flex; gap: 8px; margin-top: 8px;">
            <div style="flex: 1;">
              <label class="field-label">IP Address</label>
              <input type="text" name="wifiIp" id="wifiIp" class="settings-input" placeholder="192.168.15.160">
            </div>
            <div style="flex: 1;">
              <label class="field-label">Gateway</label>
              <input type="text" name="wifiGw" id="wifiGw" class="settings-input" placeholder="192.168.15.1">
            </div>
          </div>
          <div style="display: flex; gap: 8px; margin-top: 8px;">
            <div style="flex: 1;">
              <label class="field-label">Subnet</label>
              <input type="text" name="wifiSubnet" id="wifiSubnet" class="settings-input" placeholder="255.255.255.0">
            </div>
            <div style="flex: 1;">
              <label class="field-label">Primary DNS</label>
              <input type="text" name="wifiDns1" id="wifiDns1" class="settings-input" placeholder="1.1.1.1">
            </div>
          </div>
          <div style="margin-top: 8px;">
            <label class="field-label">Secondary DNS</label>
            <input type="text" name="wifiDns2" id="wifiDns2" class="settings-input" placeholder="8.8.8.8">
          </div>
        </div>
      </div>
    </div>

    <div class="card">
      <h1>MQTT Broker</h1>
      <div style="display: flex; gap: 8px;">
        <div style="flex: 2;">
          <label class="field-label">Broker IP</label>
          <input type="text" name="mqttBroker" id="mqttBroker" class="settings-input" placeholder="192.168.15.18">
        </div>
        <div style="flex: 1;">
          <label class="field-label">Port</label>
          <input type="number" name="mqttPort" id="mqttPort" class="settings-input" placeholder="1883">
        </div>
      </div>
    </div>

    <div class="card">
      <h1>AI API Keys</h1>
      <div class="field-group">
        <label class="field-label">Groq API Key <span style="color: #10b981; font-size: 0.75rem;">(Free tier available)</span></label>
        <input type="password" name="groqKey" id="groqKey" class="settings-input" placeholder="gsk_...">
        <span class="field-help">Used for AI messages. Get yours at <a href="https://console.groq.com" target="_blank" style="color: #38bdf8;">console.groq.com</a></span>
      </div>
      <div class="field-group">
        <label class="field-label">Gemini API Key</label>
        <input type="password" name="geminiKey" id="geminiKey" class="settings-input" placeholder="AIza...">
      </div>
      <div class="field-group">
        <label class="field-label">DeepSeek API Key</label>
        <input type="password" name="deepseekKey" id="deepseekKey" class="settings-input" placeholder="sk-...">
      </div>
    </div>

    <div class="card">
      <h1>OpenWeather</h1>
      <div class="field-group">
        <label class="field-label">API Key</label>
        <input type="password" name="owKey" id="owKey" class="settings-input" placeholder="Your OpenWeather API key">
        <span class="field-help">Free tier available at <a href="https://openweathermap.org/api" target="_blank" style="color: #38bdf8;">openweathermap.org</a></span>
      </div>
      <div style="display: flex; gap: 8px;">
        <div style="flex: 1;">
          <label class="field-label">Latitude</label>
          <input type="text" name="owLat" id="owLat" class="settings-input" placeholder="-23.11">
        </div>
        <div style="flex: 1;">
          <label class="field-label">Longitude</label>
          <input type="text" name="owLon" id="owLon" class="settings-input" placeholder="-46.53">
        </div>
      </div>
    </div>

    <div class="card" style="text-align: center;">
      <button type="submit" class="btn" style="width: 100%; padding: 12px;">Save &amp; Reboot</button>
    </div>
  </form>

  <script>
    function toggleStaticFields() {
      var sel = document.getElementById('wifiStaticSelect');
      var sf = document.getElementById('staticFields');
      sf.style.display = (sel.value === '1') ? 'block' : 'none';
    }

    function rssiToPercent(rssi) {
      if (rssi <= -100) return 0;
      if (rssi >= -50) return 100;
      return 2 * (rssi + 100);
    }

    function signalBars(pct) {
      if (pct > 75) return '&#9650;&#9650;&#9650;';
      if (pct > 50) return '&#9650;&#9650;';
      if (pct > 25) return '&#9650;';
      return '';
    }

    function scanWifi() {
      var btn = document.getElementById('scanBtn');
      var list = document.getElementById('apList');
      btn.disabled = true;
      btn.textContent = 'Scanning...';
      list.innerHTML = '';
      list.classList.add('open');

      fetch('/wifi-scan')
        .then(function(r) { return r.json(); })
        .then(function(aps) {
          btn.disabled = false;
          btn.textContent = 'Scan';
          if (aps.length === 0) {
            list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#64748b;">No networks found</span></div>';
            return;
          }
          var html = '';
          for (var i = 0; i < aps.length; i++) {
            var pct = rssiToPercent(aps[i].rssi);
            var bars = signalBars(pct);
            var lock = aps[i].secure ? '<span class="ap-locked">&#128274;</span>' : '';
            html += '<div class="ap-item" onclick="selectAP(this)" data-ssid="' + aps[i].ssid.replace(/"/g, '&quot;') + '" data-secure="' + aps[i].secure + '">';
            html += '<span class="ap-name">' + aps[i].ssid + lock + '</span>';
            html += '<span class="ap-rssi">' + bars + ' ' + pct + '%</span>';
            html += '</div>';
          }
          list.innerHTML = html;
        })
        .catch(function() {
          btn.disabled = false;
          btn.textContent = 'Scan';
          list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#ef4444;">Scan failed</span></div>';
        });
    }

    function selectAP(el) {
      document.getElementById('wifiSsid').value = el.dataset.ssid;
      var list = document.getElementById('apList');
      list.classList.remove('open');
      if (el.dataset.secure === '0') {
        document.getElementById('wifiPass').value = '';
        document.getElementById('wifiPass').placeholder = 'Open network - no password needed';
      } else {
        document.getElementById('wifiPass').placeholder = 'WiFi Password';
      }
    }
  </script>

  <script>
    fetch('/radar-data')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        document.getElementById('wifiSsid').value = data.wifiSsid || '';
        document.getElementById('wifiPass').value = data.wifiPass || '';
        document.getElementById('wifiStaticSelect').value = data.wifiStatic ? '1' : '0';
        document.getElementById('wifiIp').value = data.wifiIp || '';
        document.getElementById('wifiGw').value = data.wifiGw || '';
        document.getElementById('wifiSubnet').value = data.wifiSubnet || '';
        document.getElementById('wifiDns1').value = data.wifiDns1 || '';
        document.getElementById('wifiDns2').value = data.wifiDns2 || '';
        toggleStaticFields();
        document.getElementById('mqttBroker').value = data.mqttBroker || '';
        document.getElementById('mqttPort').value = data.mqttPort || 1883;
        if (data.hasGroqKey) document.getElementById('groqKey').placeholder = '*** configured ***';
        if (data.hasGeminiKey) document.getElementById('geminiKey').placeholder = '*** configured ***';
        if (data.hasDeepseekKey) document.getElementById('deepseekKey').placeholder = '*** configured ***';
        if (data.hasOwKey) document.getElementById('owKey').placeholder = '*** configured ***';
        document.getElementById('owLat').value = data.owLat || '';
        document.getElementById('owLon').value = data.owLon || '';
      })
      .catch(function(err) { console.error("Error loading credentials:", err); });

    document.querySelector('form').addEventListener('submit', function(e) {
      if (!confirm('Save credentials and reboot DeskBuddy?')) {
        e.preventDefault();
      }
    });
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1);
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

// Captive portal: redirect all OS detection probes to /setup
inline void handleCaptiveRedirect() {
  server.sendHeader("Location", "/setup", true);
  server.send(302, "text/plain", "");
}

// Captive portal: simplified WiFi provisioning page
inline void handleSetup() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <link rel="icon" href="/favicon.ico">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Setup</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 420px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; box-sizing: border-box; }
    h1 { font-size: 1.4rem; color: #38bdf8; text-align: center; margin: 0 0 6px 0; }
    .subtitle { color: #94a3b8; text-align: center; font-size: 0.9rem; margin-bottom: 20px; }
    .field-label { color: #94a3b8; font-size: 0.9rem; margin-bottom: 4px; display: block; }
    .settings-input { background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 8px 10px; border-radius: 6px; width: 100%; text-align: left; font-family: inherit; font-size: 0.95rem; box-sizing: border-box; }
    .btn { background: #38bdf8; color: #0f172a; font-weight: bold; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-family: inherit; font-size: 1rem; transition: opacity 0.2s; width: 100%; }
    .btn:hover { opacity: 0.9; }
    .btn-scan { background: #6366f1; color: white; padding: 8px 14px; border-radius: 6px; border: none; cursor: pointer; font-family: inherit; font-size: 0.85rem; font-weight: bold; white-space: nowrap; transition: opacity 0.2s; }
    .btn-scan:hover { opacity: 0.85; }
    .btn-scan:disabled { opacity: 0.5; cursor: wait; }
    .ssid-row { display: flex; gap: 8px; align-items: flex-end; margin-bottom: 12px; }
    .ssid-row input { flex: 1; }
    .field-group { margin-bottom: 12px; }
    #apList { max-height: 0; overflow: hidden; transition: max-height 0.3s ease; margin-top: 0; }
    #apList.open { max-height: 260px; overflow-y: auto; margin-top: 8px; border: 1px solid #334155; border-radius: 6px; background: #0f172a; }
    .ap-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 12px; cursor: pointer; border-bottom: 1px solid #1e293b; font-size: 0.9rem; transition: background 0.15s; }
    .ap-item:last-child { border: none; }
    .ap-item:hover { background: #1e293b; }
    .ap-item .ap-name { color: #f8fafc; }
    .ap-item .ap-rssi { color: #64748b; font-size: 0.8rem; }
    .ap-item .ap-locked { color: #f59e0b; font-size: 0.75rem; margin-left: 6px; }
    .help { color: #64748b; font-size: 0.8rem; text-align: center; margin-top: 12px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>DeskBuddy Setup</h1>
    <p class="subtitle">Connect your device to a WiFi network</p>

    <form action="/save-credentials" method="POST">
      <div class="field-group">
        <label class="field-label">Network Name (SSID)</label>
        <div class="ssid-row">
          <input type="text" name="wifiSsid" id="wifiSsid" class="settings-input" placeholder="WiFi SSID" autocomplete="off">
          <button type="button" class="btn-scan" id="scanBtn" onclick="scanWifi()">Scan</button>
        </div>
        <div id="apList"></div>
      </div>
      <div class="field-group">
        <label class="field-label">Password</label>
        <input type="password" name="wifiPass" id="wifiPass" class="settings-input" placeholder="WiFi Password">
      </div>
      <button type="submit" class="btn">Save &amp; Connect</button>
    </form>

    <p class="help">After connecting, visit your device's IP address for full settings.</p>
  </div>

  <script>
    function rssiToPercent(rssi) {
      if (rssi <= -100) return 0;
      if (rssi >= -50) return 100;
      return 2 * (rssi + 100);
    }
    function signalBars(pct) {
      if (pct > 75) return '&#9650;&#9650;&#9650;';
      if (pct > 50) return '&#9650;&#9650;';
      if (pct > 25) return '&#9650;';
      return '';
    }
    function scanWifi() {
      var btn = document.getElementById('scanBtn');
      var list = document.getElementById('apList');
      btn.disabled = true;
      btn.textContent = 'Scanning...';
      list.innerHTML = '';
      list.classList.add('open');
      fetch('/wifi-scan')
        .then(function(r) { return r.json(); })
        .then(function(aps) {
          btn.disabled = false;
          btn.textContent = 'Scan';
          if (aps.length === 0) {
            list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#64748b;">No networks found</span></div>';
            return;
          }
          var html = '';
          for (var i = 0; i < aps.length; i++) {
            var pct = rssiToPercent(aps[i].rssi);
            var bars = signalBars(pct);
            var lock = aps[i].secure ? '<span class="ap-locked">&#128274;</span>' : '';
            html += '<div class="ap-item" onclick="selectAP(this)" data-ssid="' + aps[i].ssid.replace(/"/g, '&quot;') + '" data-secure="' + aps[i].secure + '">';
            html += '<span class="ap-name">' + aps[i].ssid + lock + '</span>';
            html += '<span class="ap-rssi">' + bars + ' ' + pct + '%</span>';
            html += '</div>';
          }
          list.innerHTML = html;
        })
        .catch(function() {
          btn.disabled = false;
          btn.textContent = 'Scan';
          list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#ef4444;">Scan failed</span></div>';
        });
    }
    function selectAP(el) {
      document.getElementById('wifiSsid').value = el.dataset.ssid;
      document.getElementById('apList').classList.remove('open');
      if (el.dataset.secure === '0') {
        document.getElementById('wifiPass').value = '';
        document.getElementById('wifiPass').placeholder = 'Open network - no password needed';
      } else {
        document.getElementById('wifiPass').placeholder = 'WiFi Password';
      }
    }
  </script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

inline void handleSaveCredentials() {
  preferences.begin("deskbuddy", false);

  if (server.hasArg("wifiSsid")) { appConfig.wifiSsid = server.arg("wifiSsid"); preferences.putString("wifiSsid", appConfig.wifiSsid.c_str()); }
  if (server.hasArg("wifiPass")) { appConfig.wifiPass = server.arg("wifiPass"); preferences.putString("wifiPass", appConfig.wifiPass.c_str()); }
  if (server.hasArg("wifiStatic")) { appConfig.wifiStaticEnabled = (server.arg("wifiStatic").toInt() == 1); preferences.putBool("wifiStatic", appConfig.wifiStaticEnabled); }
  if (server.hasArg("wifiIp")) { appConfig.wifiIp = server.arg("wifiIp"); preferences.putString("wifiIp", appConfig.wifiIp.c_str()); }
  if (server.hasArg("wifiGw")) { appConfig.wifiGw = server.arg("wifiGw"); preferences.putString("wifiGw", appConfig.wifiGw.c_str()); }
  if (server.hasArg("wifiSubnet")) { appConfig.wifiSubnet = server.arg("wifiSubnet"); preferences.putString("wifiSubnet", appConfig.wifiSubnet.c_str()); }
  if (server.hasArg("wifiDns1")) { appConfig.wifiDns1 = server.arg("wifiDns1"); preferences.putString("wifiDns1", appConfig.wifiDns1.c_str()); }
  if (server.hasArg("wifiDns2")) { appConfig.wifiDns2 = server.arg("wifiDns2"); preferences.putString("wifiDns2", appConfig.wifiDns2.c_str()); }

  if (server.hasArg("mqttBroker")) { appConfig.mqttBroker = server.arg("mqttBroker"); preferences.putString("mqttBroker", appConfig.mqttBroker.c_str()); }
  if (server.hasArg("mqttPort")) { appConfig.mqttPort = server.arg("mqttPort").toInt(); preferences.putInt("mqttPort", appConfig.mqttPort); }

  if (server.hasArg("groqKey")) {
    String val = server.arg("groqKey");
    if (val.length() > 0) { appConfig.groqApiKey = val; preferences.putString("groqKey", appConfig.groqApiKey.c_str()); }
  }
  if (server.hasArg("geminiKey")) {
    String val = server.arg("geminiKey");
    if (val.length() > 0) { appConfig.geminiApiKey = val; preferences.putString("geminiKey", appConfig.geminiApiKey.c_str()); }
  }
  if (server.hasArg("deepseekKey")) {
    String val = server.arg("deepseekKey");
    if (val.length() > 0) { appConfig.deepseekApiKey = val; preferences.putString("deepseekKey", appConfig.deepseekApiKey.c_str()); }
  }

  if (server.hasArg("owKey")) {
    String val = server.arg("owKey");
    if (val.length() > 0) { appConfig.openWeatherKey = val; preferences.putString("owKey", appConfig.openWeatherKey.c_str()); }
  }
  if (server.hasArg("owLat")) { appConfig.openWeatherLat = server.arg("owLat").toFloat(); preferences.putFloat("owLat", appConfig.openWeatherLat); }
  if (server.hasArg("owLon")) { appConfig.openWeatherLon = server.arg("owLon").toFloat(); preferences.putFloat("owLon", appConfig.openWeatherLon); }

  preferences.end();

  // Reboot after save
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Credentials Saved, Rebooting...");
  delay(500);
  LittleFS.end();
  ESP.restart();
}

inline void handleRadarData() {
  DynamicJsonDocument doc(4096);
  doc["presence"] = (appState.currentPresenceState != STATE_AWAY);
  doc["state"] = getPresenceStateName(appState.currentPresenceState);
  doc["presenceDetected"] = appState.sensorPresenceDetected;
  doc["movingTargetDetected"] = appState.sensorMovingTargetDetected;
  doc["mqttConnected"] = mqttClient.connected();
  doc["mqttBroker"] = appConfig.mqttBroker;
  doc["mqttPort"] = appConfig.mqttPort;
  
  // Distance metrics (always return current stored values to avoid single-frame connection dropouts)
  doc["detectionDist"] = (int)appState.filteredDetectionDist;
  doc["rawDetectionDist"] = appState.rawDetectionDist;
  doc["sessionDistAvg"] = (int)appState.sessionDistanceAverage;
  
  doc["deskTime"] = formatTime(appStats.totalDeskTime);
  doc["focusTime"] = formatTime(appStats.totalFocusTime);
  doc["breakTime"] = formatTime(appStats.totalBreakTime);
  doc["overnightBreak"] = formatTime(appStats.overnightBreakDuration * 1000);
  doc["breaks"] = appStats.breakCount;
  doc["latestBreak"] = formatTime(appStats.latestBreakDuration);
  doc["longestStreak"] = formatTime(appStats.longestSittingStreak);
  doc["firstSitTime"] = formatEpochTime(appStats.firstSitEpoch);
  doc["firstSitEpoch"] = (uint32_t)appStats.firstSitEpoch;
  doc["score"] = appStats.productivityScore;
  doc["aiMode"] = appConfig.aiMode;
  doc["aiPersona"] = appConfig.aiPersona;
  doc["dailyAiRequests"] = appStats.dailyAiRequestCount;
  doc["fsReadCount"] = appStats.fsReadCount;
  doc["fsWriteCount"] = appStats.fsWriteCount;
  doc["fsTotalBytes"] = (uint32_t)LittleFS.totalBytes();
  doc["fsUsedBytes"] = (uint32_t)LittleFS.usedBytes();
  doc["fsWritesToday"] = appStats.fsWritesToday;
  doc["todayMinutesElapsed"] = timeClient.isTimeSet() ? (timeClient.getHours() * 60 + timeClient.getMinutes()) : 0;
  doc["uptimeSeconds"] = (uint32_t)(millis() / 1000);
  doc["clockFace"] = appConfig.clockFace;
  doc["targetHours"] = appConfig.targetHours;
  doc["hasMail"] = appConfig.hasMail;
  doc["time24h"] = appConfig.time24h;
  doc["userName"] = appConfig.userName;
  doc["focusDistLim"] = appConfig.focusDistanceLimit;
  doc["motionRatioLim"] = appConfig.motionRatioLimit;
  doc["motionRatio"] = (appState.sessionDeskTime > 0) ? std::min((int)((appState.sessionMotionTime * 100) / appState.sessionDeskTime), 100) : 0;
  doc["totalMotionTime"] = formatTime(appStats.totalMotionTime);
  doc["motionCount"] = appStats.motionCount;
  doc["distLimit"] = appConfig.deskDistanceLimit;
  doc["filterWindow"] = appConfig.filterWindow;
  
  // Learned occupancy metrics
  int currentDay = timeClient.isTimeSet() ? timeClient.getDay() : 1;
  int targetDay = currentDay;
  bool showRawToday = true;
  if (server.hasArg("day")) {
    int dayVal = server.arg("day").toInt();
    if (dayVal == -1) {
      showRawToday = true;
    } else if (dayVal >= 0 && dayVal < 7) {
      showRawToday = false;
      targetDay = dayVal;
    }
  }
  doc["historyDays"] = showRawToday ? 1 : appStats.historyDaysCountWeekly[targetDay];
  
  // Credential fields for /credentials page
  doc["wifiSsid"] = appConfig.wifiSsid;
  doc["wifiPass"] = appConfig.wifiPass;
  doc["wifiStatic"] = appConfig.wifiStaticEnabled;
  doc["wifiIp"] = appConfig.wifiIp;
  doc["wifiGw"] = appConfig.wifiGw;
  doc["wifiSubnet"] = appConfig.wifiSubnet;
  doc["wifiDns1"] = appConfig.wifiDns1;
  doc["wifiDns2"] = appConfig.wifiDns2;
  doc["mqttPort"] = appConfig.mqttPort;
  doc["hasGroqKey"] = (appConfig.groqApiKey.length() > 0);
  doc["hasGeminiKey"] = (appConfig.geminiApiKey.length() > 0);
  doc["hasDeepseekKey"] = (appConfig.deepseekApiKey.length() > 0);
  doc["hasOwKey"] = (appConfig.openWeatherKey.length() > 0);
  doc["owLat"] = appConfig.openWeatherLat;
  doc["owLon"] = appConfig.openWeatherLon;
  
  // Combine history with today's real-time accumulated presence if targetDay matches today
  uint8_t blendedHistory[24];
  for (int h = 0; h < 24; h++) {
    if (showRawToday) {
      uint32_t todayMs = appStats.presenceMsCurrentDay[h];
      blendedHistory[h] = (uint8_t)constrain((todayMs * 100UL) / 3600000UL, 0UL, 100UL);
    } else {
      blendedHistory[h] = getEffectivePresence(targetDay, h);
    }
  }
  
  doc["lunchHour"] = getLearnedLunchHour(blendedHistory);
  doc["workdayStart"] = getLearnedWorkdayStart(blendedHistory);
  doc["workdayEnd"] = getLearnedWorkdayEnd(blendedHistory);
  
  JsonArray historyArray = doc.createNestedArray("occupancyHistory");
  for (int h = 0; h < 24; h++) {
    historyArray.add(blendedHistory[h]);
  }

  // Average sitting time per day-of-week (Mon-Sun order, computed from hourly presence averages)
  int dayOrder[] = {1, 2, 3, 4, 5, 6, 0}; // Mon-Sun
  JsonArray sittingTimeArray = doc.createNestedArray("avgSittingTimeWeekly");
  for (int i = 0; i < 7; i++) {
    int d = dayOrder[i];
    int totalPct = 0;
    for (int h = 0; h < 24; h++) {
      totalPct += getEffectivePresence(d, h);
    }
    sittingTimeArray.add((totalPct * 60) / 100); // convert to minutes
  }
  
  // Gate sensitivities (always report current synced variables)
  doc["g0mSens"] = appConfig.g0mSens;
  doc["g0sSens"] = appConfig.g0sSens;
  doc["g1mSens"] = appConfig.g1mSens;
  doc["g1sSens"] = appConfig.g1sSens;
  doc["g2mSens"] = appConfig.g2mSens;
  doc["g2sSens"] = appConfig.g2sSens;
  doc["g3mSens"] = appConfig.g3mSens;
  doc["g3sSens"] = appConfig.g3sSens;
  doc["g4mSens"] = appConfig.g4mSens;
  doc["g4sSens"] = appConfig.g4sSens;
  doc["g5mSens"] = appConfig.g5mSens;
  doc["g5sSens"] = appConfig.g5sSens;
  doc["g6mSens"] = appConfig.g6mSens;
  doc["g6sSens"] = appConfig.g6sSens;
  
  // Add AI response thread-safely
  xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
  doc["aiMessage"] = appState.aiResponse;
  doc["isAiGenerated"] = appState.lastResponseIsAi;
  xSemaphoreGive(appState.geminiMutex);
  doc["aiLoading"] = appState.isAILoading;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

inline void handleSaveSettings() {
  if (server.hasArg("aiMode") && server.hasArg("targetHours")) {
    preferences.begin("deskbuddy", false);

    if (server.hasArg("aiMode")) {
      int val = server.arg("aiMode").toInt();
      if (val != appConfig.aiMode) { appConfig.aiMode = val; preferences.putInt("aiMode", appConfig.aiMode); }
    }
    if (server.hasArg("aiPersona")) {
      int val = server.arg("aiPersona").toInt();
      if (val != appConfig.aiPersona) { appConfig.aiPersona = val; preferences.putInt("aiPersona", appConfig.aiPersona); }
    }
    if (server.hasArg("clockFace")) {
      int val = server.arg("clockFace").toInt();
      if (val != appConfig.clockFace) { appConfig.clockFace = val; preferences.putInt("clockFace", appConfig.clockFace); }
    }
    if (server.hasArg("targetHours")) {
      float val = server.arg("targetHours").toFloat();
      if (val < 0.1f) val = 8.0f;
      if (val != appConfig.targetHours) { appConfig.targetHours = val; preferences.putFloat("targetHours", appConfig.targetHours); }
    }
    if (server.hasArg("hasMail")) {
      bool val = (server.arg("hasMail").toInt() == 1);
      if (val != appConfig.hasMail) { appConfig.hasMail = val; preferences.putBool("hasMail", appConfig.hasMail); }
    }
    if (server.hasArg("time24h")) {
      bool val = (server.arg("time24h").toInt() == 1);
      if (val != appConfig.time24h) { appConfig.time24h = val; preferences.putBool("time24h", appConfig.time24h); }
    }
    if (server.hasArg("userName")) {
      String val = server.arg("userName");
      if (val != appConfig.userName) { appConfig.userName = val; preferences.putString("userName", appConfig.userName.c_str()); }
    }
    if (server.hasArg("focusDistLim")) {
      int val = server.arg("focusDistLim").toInt();
      if (val != appConfig.focusDistanceLimit) { appConfig.focusDistanceLimit = val; preferences.putInt("focusDistLim", appConfig.focusDistanceLimit); }
    }
    if (server.hasArg("motionRatioLim")) {
      int val = server.arg("motionRatioLim").toInt();
      if (val != appConfig.motionRatioLimit) { appConfig.motionRatioLimit = val; preferences.putInt("motionRatioLim", appConfig.motionRatioLimit); }
    }
    if (server.hasArg("distLimit")) {
      int val = server.arg("distLimit").toInt();
      if (val != appConfig.deskDistanceLimit) { appConfig.deskDistanceLimit = val; preferences.putInt("distLimit", appConfig.deskDistanceLimit); }
    }
    if (server.hasArg("filterWindow")) {
      float val = server.arg("filterWindow").toFloat();
      if (val != appConfig.filterWindow) { appConfig.filterWindow = val; preferences.putFloat("filterWindow", appConfig.filterWindow); }
    }

    if (server.hasArg("g0mSens")) {
      int val = server.arg("g0mSens").toInt();
      if (val != appConfig.g0mSens) { appConfig.g0mSens = val; preferences.putInt("g0mSens", appConfig.g0mSens); }
    }
    if (server.hasArg("g0sSens")) {
      int val = server.arg("g0sSens").toInt();
      if (val != appConfig.g0sSens) { appConfig.g0sSens = val; preferences.putInt("g0sSens", appConfig.g0sSens); }
    }
    if (server.hasArg("g1mSens")) {
      int val = server.arg("g1mSens").toInt();
      if (val != appConfig.g1mSens) { appConfig.g1mSens = val; preferences.putInt("g1mSens", appConfig.g1mSens); }
    }
    if (server.hasArg("g1sSens")) {
      int val = server.arg("g1sSens").toInt();
      if (val != appConfig.g1sSens) { appConfig.g1sSens = val; preferences.putInt("g1sSens", appConfig.g1sSens); }
    }
    if (server.hasArg("g2mSens")) {
      int val = server.arg("g2mSens").toInt();
      if (val != appConfig.g2mSens) { appConfig.g2mSens = val; preferences.putInt("g2mSens", appConfig.g2mSens); }
    }
    if (server.hasArg("g2sSens")) {
      int val = server.arg("g2sSens").toInt();
      if (val != appConfig.g2sSens) { appConfig.g2sSens = val; preferences.putInt("g2sSens", appConfig.g2sSens); }
    }
    if (server.hasArg("g3mSens")) {
      int val = server.arg("g3mSens").toInt();
      if (val != appConfig.g3mSens) { appConfig.g3mSens = val; preferences.putInt("g3mSens", appConfig.g3mSens); }
    }
    if (server.hasArg("g3sSens")) {
      int val = server.arg("g3sSens").toInt();
      if (val != appConfig.g3sSens) { appConfig.g3sSens = val; preferences.putInt("g3sSens", appConfig.g3sSens); }
    }
    if (server.hasArg("g4mSens")) {
      int val = server.arg("g4mSens").toInt();
      if (val != appConfig.g4mSens) { appConfig.g4mSens = val; preferences.putInt("g4mSens", appConfig.g4mSens); }
    }
    if (server.hasArg("g4sSens")) {
      int val = server.arg("g4sSens").toInt();
      if (val != appConfig.g4sSens) { appConfig.g4sSens = val; preferences.putInt("g4sSens", appConfig.g4sSens); }
    }
    if (server.hasArg("g5mSens")) {
      int val = server.arg("g5mSens").toInt();
      if (val != appConfig.g5mSens) { appConfig.g5mSens = val; preferences.putInt("g5mSens", appConfig.g5mSens); }
    }
    if (server.hasArg("g5sSens")) {
      int val = server.arg("g5sSens").toInt();
      if (val != appConfig.g5sSens) { appConfig.g5sSens = val; preferences.putInt("g5sSens", appConfig.g5sSens); }
    }
    if (server.hasArg("g6mSens")) {
      int val = server.arg("g6mSens").toInt();
      if (val != appConfig.g6mSens) { appConfig.g6mSens = val; preferences.putInt("g6mSens", appConfig.g6mSens); }
    }
    if (server.hasArg("g6sSens")) {
      int val = server.arg("g6sSens").toInt();
      if (val != appConfig.g6sSens) { appConfig.g6sSens = val; preferences.putInt("g6sSens", appConfig.g6sSens); }
    }

    preferences.end();
    
    saveDailyStats();
    
    // Dynamically adjust physical radar gates according to new range limit
    if (radar.isConnected()) {
      radar.setGateSensitivityThreshold(0, appConfig.g0mSens, appConfig.g0sSens);
      radar.setGateSensitivityThreshold(1, appConfig.g1mSens, appConfig.g1sSens);
      radar.setGateSensitivityThreshold(2, appConfig.g2mSens, appConfig.g2sSens);
      radar.setGateSensitivityThreshold(3, appConfig.g3mSens, appConfig.g3sSens);
      radar.setGateSensitivityThreshold(4, appConfig.g4mSens, appConfig.g4sSens);
      radar.setGateSensitivityThreshold(5, appConfig.g5mSens, appConfig.g5sSens);
      radar.setGateSensitivityThreshold(6, appConfig.g6mSens, appConfig.g6sSens);
      int requiredGates = (appConfig.deskDistanceLimit + 19) / 20;
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
  appStats.firstSitToday = true;
  appStats.firstSitEpoch = 0;
  appStats.breakCount = 0;
  appStats.totalDeskTime = 0;
  appStats.totalFocusTime = 0;
  appStats.totalBreakTime = 0;
  appStats.overnightBreakDuration = 0;
  appStats.lastAwayEpoch = 0;
  appStats.dailyAiRequestCount = 0;
  appStats.longestSittingStreak = 0;
  appStats.latestBreakDuration = 0;
  appStats.totalMotionTime = 0;
  appStats.motionCount = 0;
  appState.sessionDeskTime = 0;
  appState.sessionMotionTime = 0;
  appState.sessionDistanceSum = 0;
  appState.sessionDistanceCount = 0;
  appState.sessionDistanceAverage = 0.0;

  saveDailyStats();

  server.send(200, "text/plain", "Daily Stats Reset");
}

inline void handleMqttHistory() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("messages");
  
  if (appState.mqttHistoryMutex != NULL) {
    xSemaphoreTake(appState.mqttHistoryMutex, portMAX_DELAY);
    int idx = appState.mqttHistoryHead;
    int count = appState.mqttHistoryCount;

    // Return in chronological order (oldest to newest)
    for (int i = 0; i < count; i++) {
      int curIdx = (idx - count + i + MQTT_HISTORY_SIZE) % MQTT_HISTORY_SIZE;
      JsonObject obj = arr.createNestedObject();
      obj["topic"] = appState.mqttHistory[curIdx].topic;
      obj["payload"] = appState.mqttHistory[curIdx].payload;
      obj["timestamp"] = (double)appState.mqttHistory[curIdx].timestamp;
    }
    xSemaphoreGive(appState.mqttHistoryMutex);
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

inline void handleMqttClear() {
  if (appState.mqttHistoryMutex != NULL) {
    xSemaphoreTake(appState.mqttHistoryMutex, portMAX_DELAY);
    appState.mqttHistoryHead = 0;
    appState.mqttHistoryCount = 0;
    xSemaphoreGive(appState.mqttHistoryMutex);
  }
  server.send(200, "text/plain", "Cleared");
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
    appStats.fsWriteCount++;
    appStats.fsWritesToday++;
    LittleFS.remove("/stats.json");
  }

  server.send(200, "text/plain", "Factory Reset Complete. Rebooting...");
  delay(1000);
  LittleFS.end();
  ESP.restart();
}

// Global/static file upload helper
static fs::File uploadFile;

inline void handleFilesList() {
  DynamicJsonDocument doc(2048);
  JsonArray files = doc.createNestedArray("files");
  
  appStats.fsReadCount++;
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
      appStats.fsReadCount++;
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
      appStats.fsWriteCount++;
      appStats.fsWritesToday++;
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
    appStats.fsWriteCount++;
    appStats.fsWritesToday++;
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
  <link rel="icon" href="/favicon.ico">
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
  // Captive portal detection routes (redirect to /setup)
  server.on("/generate_204", handleCaptiveRedirect);
  server.on("/hotspot-detect.html", handleCaptiveRedirect);
  server.on("/connecttest.txt", handleCaptiveRedirect);
  server.on("/ncsi.txt", handleCaptiveRedirect);
  server.on("/success.txt", handleCaptiveRedirect);
  server.on("/redirect", handleCaptiveRedirect);
  server.on("/canonical.html", handleCaptiveRedirect);

  // Captive portal setup page (always registered; only used in AP mode)
  server.on("/setup", HTTP_GET, handleSetup);

  server.on("/", handleRoot);
  server.on("/todo", handleTodo);
  server.on("/api/tasks", HTTP_GET, handleGetTasks);
  server.on("/api/tasks/save", HTTP_POST, handleSaveTasks);
  server.on("/settings", handleSettings);
  server.on("/radar-data", handleRadarData);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/credentials", HTTP_GET, handleCredentials);
  server.on("/save-credentials", HTTP_POST, handleSaveCredentials);
  server.on("/wifi-scan", HTTP_GET, handleWifiScan);
  server.on("/reset-stats", handleResetStats);
  server.on("/factory-reset", handleFactoryReset);
  server.on("/trigger-event", handleTriggerEvent);
  server.on("/mqtt-history", handleMqttHistory);
  server.on("/mqtt-publish", handleMqttPublish);
  server.on("/mqtt-clear", HTTP_POST, handleMqttClear);
  server.on("/reset-esp", []() {
    server.send(200, "text/plain", "Rebooting");
    delay(500);
    LittleFS.end();
    ESP.restart();
  });
  server.on("/files", HTTP_GET, handleFilesList);
  server.on("/file-manager", HTTP_GET, handleFileManager);
  server.on("/download", HTTP_GET, handleDownloadFile);
  server.on("/delete-file", HTTP_GET, handleDeleteFile);
  server.on("/upload", HTTP_POST, handleUploadResponse, handleFileUpload);

  server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");

  server.onNotFound([]() {
    if (appState.captivePortalMode) {
      handleCaptiveRedirect();
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

#endif // WEB_H
