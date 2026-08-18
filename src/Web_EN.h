#ifndef WEB_EN_H
#define WEB_EN_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ld2410.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "MqttService.h"

#include "State.h"
#include "Points.h"
#include "Timer.h"

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
static const char ROOT_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1">
 <title>DeskBuddy Radar Dashboard</title>
 <link rel="stylesheet" href="/web/style.css">
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
 <a href="#" onclick="triggerJournalDisplay()" style="color: #94a3b8; text-decoration: none; font-weight: 600; font-size: 0.95rem; display: flex; align-items: center; gap: 4px; transition: color 0.2s;" onmouseover="this.style.color='#38bdf8'" onmouseout="this.style.color='#94a3b8'" title="Journal Overview">
 <svg style="width: 18px; height: 18px; fill: currentColor;" viewBox="0 0 24 24">
 <path d="M4 6H2v14c0 1.1.9 2 2 2h14v-2H4V6zm16-4H8c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm0 14H8V4h12v12z"/>
 </svg>
 JOURNAL
 </a>
 <a href="/settings" class="cog-btn" title="Settings & Calibration">
 <svg viewBox="0 0 24 24">
 <path d="M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"/>
 </svg>
 </a>
 </div>
 </div>

 <div class="card ai-card" id="tftMessageCard">
 <div style="position: relative; display: flex; justify-content: center; align-items: center; margin-bottom: 12px; width: 100%;">
 <h1 style="margin:0; text-align: center;">Recent Updates</h1>
 <button class="expand-toggle" id="expandToggle" onclick="toggleTftLog()" title="Expand / Collapse" style="position: absolute; right: 0;">
 <svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>
 </button>
 </div>
 <div id="tftLog" style="max-height:100px; overflow:hidden; display:flex; flex-direction:column; gap:8px; transition: max-height 0.35s ease; box-sizing: border-box; padding: 2px;">
 <div style="color:#64748b; text-align:center; padding:20px 0; font-size:0.9rem;">Loading...</div>
 </div>
 <div class="ai-loading-container" id="aiLoading" style="display:none; margin-top:8px;">
 <div class="ai-spinner"></div>
 <span>AI is generating response...</span>
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

 <!-- Trip Odometer Card -->
 <div class="card" id="tripOdometerCard">
 <div style="position: relative; display: flex; justify-content: center; align-items: center; margin-bottom: 14px;">
 <h1 style="margin: 0; text-align: center;">Activity Trip Odometers</h1>
 <button onclick="openOdometerModal()" title="Edit Labels" style="position: absolute; right: 0; background: none; border: none; color: #94a3b8; cursor: pointer; padding: 4px;" onmouseover="this.style.color='#38bdf8';" onmouseout="this.style.color='#94a3b8';">
 <svg style="width: 18px; height: 18px; fill: currentColor;" viewBox="0 0 24 24">
 <path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/>
 </svg>
 </button>
 </div>
 <div style="display: flex; gap: 8px; justify-content: space-between; align-items: center; width: 100%; box-sizing: border-box;">
 <div class="odo-btn" id="odoRow0" onclick="selectOdometer(0)" style="flex: 1; min-width: 0; padding: 8px 4px; border-radius: 8px; border: 1px solid #334155; cursor: pointer; text-align: center; transition: all 0.2s;">
 <div class="odo-label" id="odoName0" style="color: #38bdf8; font-size: 0.7rem; font-weight: 700; text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">Work</div>
 <div class="odo-time" id="odoVal0" style="font-family: monospace; font-size: 0.85rem; font-weight: 700; color: #f8fafc; margin-top: 2px;">00:00:00</div>
 </div>
 <div class="odo-btn" id="odoRow1" onclick="selectOdometer(1)" style="flex: 1; min-width: 0; padding: 8px 4px; border-radius: 8px; border: 1px solid #334155; cursor: pointer; text-align: center; transition: all 0.2s;">
 <div class="odo-label" id="odoName1" style="color: #34d399; font-size: 0.7rem; font-weight: 700; text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">Study</div>
 <div class="odo-time" id="odoVal1" style="font-family: monospace; font-size: 0.85rem; font-weight: 700; color: #f8fafc; margin-top: 2px;">00:00:00</div>
 </div>
 <div class="odo-btn" id="odoRow2" onclick="selectOdometer(2)" style="flex: 1; min-width: 0; padding: 8px 4px; border-radius: 8px; border: 1px solid #334155; cursor: pointer; text-align: center; transition: all 0.2s;">
 <div class="odo-label" id="odoName2" style="color: #fbbf24; font-size: 0.7rem; font-weight: 700; text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">Meeting</div>
 <div class="odo-time" id="odoVal2" style="font-family: monospace; font-size: 0.85rem; font-weight: 700; color: #f8fafc; margin-top: 2px;">00:00:00</div>
 </div>
 <div class="odo-btn" id="odoRow3" onclick="selectOdometer(3)" style="flex: 1; min-width: 0; padding: 8px 4px; border-radius: 8px; border: 1px solid #334155; cursor: pointer; text-align: center; transition: all 0.2s;">
 <div class="odo-label" id="odoName3" style="color: #fb923c; font-size: 0.7rem; font-weight: 700; text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">Other</div>
 <div class="odo-time" id="odoVal3" style="font-family: monospace; font-size: 0.85rem; font-weight: 700; color: #f8fafc; margin-top: 2px;">00:00:00</div>
 </div>
 </div>
 </div>

 <!-- Odometer Label Edit Modal -->
 <div id="odoModal" style="display: none; position: fixed; inset: 0; background: rgba(15, 23, 42, 0.85); backdrop-filter: blur(6px); z-index: 9999; justify-content: center; align-items: center; padding: 20px;">
 <div style="background: #1e293b; border: 1px solid #334155; border-radius: 12px; width: 100%; max-width: 420px; padding: 24px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5);">
 <h2 style="margin: 0 0 16px 0; color: #38bdf8; font-size: 1.15rem; text-align: center;">Edit Odometer Category Labels</h2>
 <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 20px;">
 <div>
 <label class="field-label" style="color: #38bdf8; font-weight: 700;">Odometer 1 (Cyan)</label>
 <input type="text" id="odoInput0" class="settings-input" style="width: 100%; box-sizing: border-box;" maxlength="16">
 </div>
 <div>
 <label class="field-label" style="color: #34d399; font-weight: 700;">Odometer 2 (Emerald)</label>
 <input type="text" id="odoInput1" class="settings-input" style="width: 100%; box-sizing: border-box;" maxlength="16">
 </div>
 <div>
 <label class="field-label" style="color: #fbbf24; font-weight: 700;">Odometer 3 (Amber)</label>
 <input type="text" id="odoInput2" class="settings-input" style="width: 100%; box-sizing: border-box;" maxlength="16">
 </div>
 <div>
 <label class="field-label" style="color: #fb923c; font-weight: 700;">Odometer 4 (Orange)</label>
 <input type="text" id="odoInput3" class="settings-input" style="width: 100%; box-sizing: border-box;" maxlength="16">
 </div>
 </div>
 <div style="display: flex; justify-content: flex-end; gap: 10px;">
 <button onclick="closeOdometerModal()" class="btn btn-secondary" style="padding: 8px 16px;">Cancel</button>
 <button onclick="saveOdometerLabels()" class="btn" style="padding: 8px 16px;">Save Labels</button>
 </div>
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
 <h1 style="margin: 0 0 15px 0; text-align: left;">Timer</h1>
 <div style="display: flex; gap: 20px; flex-wrap: wrap;">
 <div style="flex: 1; min-width: 240px;">
 <div style="font-size: 0.8rem; color: #94a3b8; font-weight: 600; text-transform: uppercase; letter-spacing: 0.04em; margin-bottom: 6px;">Stopwatch</div>
 <div class="timer-display" id="swDisplay">00:00:00</div>
 <div style="display: flex; gap: 8px; margin-top: 8px;">
 <button class="timer-btn" onclick="swToggle()" id="swBtn">Start</button>
 <button class="timer-btn ghost" onclick="swReset()">Reset</button>
 </div>
 </div>
 <div style="flex: 1; min-width: 240px;">
 <div style="font-size: 0.8rem; color: #94a3b8; font-weight: 600; text-transform: uppercase; letter-spacing: 0.04em; margin-bottom: 6px;">Countdown</div>
 <div class="timer-display" id="cdDisplay">05:00</div>
 <div style="display: flex; gap: 8px; margin-top: 8px; flex-wrap: wrap; align-items: center;">
 <input type="number" id="cdMinutes" min="0" max="120" value="5" style="width: 60px; background: #0f172a; border: 1px solid #334155; border-radius: 6px; padding: 6px 8px; color: #f8fafc; font-size: 0.95rem;" title="Minutes">
 <span style="color: #94a3b8; font-size: 0.8rem;">m</span>
 <input type="number" id="cdSeconds" min="0" max="59" value="0" style="width: 56px; background: #0f172a; border: 1px solid #334155; border-radius: 6px; padding: 6px 8px; color: #f8fafc; font-size: 0.95rem;" title="Seconds">
 <span style="color: #94a3b8; font-size: 0.8rem;">s</span>
 <button class="timer-btn" onclick="cdStart()" id="cdBtn">Start</button>
 <button class="timer-btn ghost" onclick="cdReset()">Reset</button>
 </div>
 <div style="display: flex; gap: 6px; margin-top: 8px; flex-wrap: wrap;">
 <button class="timer-btn" onclick="cdPreset(5)">5m</button>
 <button class="timer-btn" onclick="cdPreset(15)">15m</button>
 <button class="timer-btn" onclick="cdPreset(25)">25m</button>
 <button class="timer-btn" onclick="cdPreset(45)">45m</button>
 <button class="timer-btn" onclick="cdPreset(60)">60m</button>
 </div>
 </div>
 </div>
 </div>

 <script>
 let selectedDay = -1;

 function triggerJournalDisplay() {
 fetch('/trigger-event?type=10&detail=&mode=0')
 .then(response => {
 if (!response.ok) alert("Failed to trigger Journal.");
 })
 .catch(err => console.error("Failed to trigger Journal.", err));
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
 
 // AI loading indicator
 document.getElementById('aiLoading').style.display = data.aiLoading ? "flex" : "none";

 if (data.activeOdometer !== undefined) {
 updateOdometerActiveUI(data.activeOdometer);
 }
 if (data.odometerFmt && Array.isArray(data.odometerFmt)) {
 for (let i = 0; i < 4; i++) {
 const valEl = document.getElementById('odoVal' + i);
 if (valEl && data.odometerFmt[i]) valEl.textContent = data.odometerFmt[i];
 }
 }
 if (data.odometerLabels && Array.isArray(data.odometerLabels)) {
 for (let i = 0; i < 4; i++) {
 const nameEl = document.getElementById('odoName' + i);
 if (nameEl && data.odometerLabels[i]) nameEl.textContent = data.odometerLabels[i];
 }
 }

 // TFT message history log
 fetch('/api/tft-messages')
 .then(r => r.json())
 .then(msgData => {
 let container = document.getElementById('tftLog');
 container.innerHTML = '';
 let msgs = msgData.messages || [];
 msgs.forEach((m, idx) => {
 let t = '--:--:--';
 if (m.epoch > 1000000000) {
 let d = new Date(m.epoch * 1000);
 let hours = String(d.getUTCHours()).padStart(2, '0');
 let minutes = String(d.getUTCMinutes()).padStart(2, '0');
 let seconds = String(d.getUTCSeconds()).padStart(2, '0');
 t = hours + ':' + minutes + ':' + seconds;
 }
 let div = document.createElement('div');
 div.className = 'tft-display-entry' + (m.isAi ? ' ai-display-entry' : '');
 if (!tftExpanded && idx > 0) div.style.display = 'none';
 div.innerHTML = '<span class="tft-display-time">' + t + '</span><span class="tft-display-text">' + m.text + '</span>';
 container.appendChild(div);
 });
 if (msgs.length === 0) {
 container.innerHTML = '<div style="color:#64748b; text-align:center; padding:20px 0; font-size:0.9rem;">No messages yet.</div>';
 }

 })
 .catch(() => {
 // Silently ignore fetch errors for secondary endpoint
 });
 
 setTimeout(updateMetrics, 500);
 })
 .catch(err => {
 console.error("Error fetching radar data:", err);
 setTimeout(updateMetrics, 500);
 });
 }
 updateMetrics();

 const ODO_ROW_BG = [
 'rgba(56, 189, 248, 0.2)',
 'rgba(52, 211, 153, 0.2)',
 'rgba(251, 191, 36, 0.2)',
 'rgba(251, 146, 60, 0.2)'
 ];
 const ODO_BORDER_COLOR = ['#38bdf8', '#34d399', '#fbbf24', '#fb923c'];

 function selectOdometer(idx) {
 fetch('/api/odometer?active=' + idx, { method: 'POST' })
 .then(r => r.json())
 .then(d => {
 if (d.status === 'success') {
 updateOdometerActiveUI(idx);
 }
 }).catch(e => console.error(e));
 }

 function updateOdometerActiveUI(activeIdx) {
 for (let i = 0; i < 4; i++) {
 const row = document.getElementById('odoRow' + i);
 if (row) {
 if (i === activeIdx) {
 row.style.background = ODO_ROW_BG[i];
 row.style.borderColor = ODO_BORDER_COLOR[i];
 row.style.boxShadow = '0 0 10px ' + ODO_ROW_BG[i];
 } else {
 row.style.background = 'transparent';
 row.style.borderColor = '#334155';
 row.style.boxShadow = 'none';
 }
 }
 }
 }

 function openOdometerModal() {
 for (let i = 0; i < 4; i++) {
 const nameEl = document.getElementById('odoName' + i);
 const inputEl = document.getElementById('odoInput' + i);
 if (nameEl && inputEl) {
 inputEl.value = nameEl.textContent;
 }
 }
 document.getElementById('odoModal').style.display = 'flex';
 }

 function closeOdometerModal() {
 document.getElementById('odoModal').style.display = 'none';
 }

 function saveOdometerLabels() {
 const payload = {
 labels: [
 document.getElementById('odoInput0').value,
 document.getElementById('odoInput1').value,
 document.getElementById('odoInput2').value,
 document.getElementById('odoInput3').value
 ]
 };
 fetch('/api/odometer_labels', {
 method: 'POST',
 headers: { 'Content-Type': 'application/json' },
 body: JSON.stringify(payload)
 }).then(r => r.json()).then(d => {
 if (d.status === 'success') {
 for (let i = 0; i < 4; i++) {
 const nameEl = document.getElementById('odoName' + i);
 if (nameEl && payload.labels[i]) {
 nameEl.textContent = payload.labels[i];
 }
 }
 closeOdometerModal();
 }
 }).catch(e => console.error(e));
 }

 let tftExpanded = false;
 function toggleTftLog() {
 tftExpanded = !tftExpanded;
 let log = document.getElementById('tftLog');
 let btn = document.getElementById('expandToggle');
 let entries = log.querySelectorAll('#tftLog > .tft-display-entry');
 if (tftExpanded) {
 log.style.maxHeight = '320px';
 log.style.overflowY = 'auto';
 btn.classList.add('expanded');
 entries.forEach(e => e.style.display = 'flex');
 } else {
 log.style.maxHeight = '100px';
 log.style.overflow = 'hidden';
 btn.classList.remove('expanded');
 entries.forEach((e, i) => e.style.display = (i > 0) ? 'none' : 'flex');
 }
 }

 let audioCtx = null;
 function ensureAudio() {
 if (!audioCtx) {
 try { audioCtx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) {}
 }
 if (audioCtx && audioCtx.state === 'suspended') audioCtx.resume();
 }

 let swRunning = false, swStartEpoch = 0, swAccum = 0, swTimer = null;
 function swTick() {
 if (!swRunning) return;
 const el = Date.now() - swStartEpoch + swAccum;
 const h = Math.floor(el / 3600000);
 const m = Math.floor(el / 60000) % 60;
 const s = Math.floor(el / 1000) % 60;
 document.getElementById('swDisplay').textContent = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
 swTimer = setTimeout(swTick, 200);
 }
 function swToggle() {
 ensureAudio();
 const btn = document.getElementById('swBtn');
 if (!swRunning) {
 swRunning = true;
 swStartEpoch = Date.now();
 btn.textContent = 'Stop';
 btn.style.background = '#f43f5e';
 btn.style.borderColor = '#f43f5e';
 swTick();
 timerCmd({ action: 'start', mode: 'sw' });
 } else {
 swAccum += Date.now() - swStartEpoch;
 swRunning = false;
 clearTimeout(swTimer);
 btn.textContent = 'Start';
 btn.style.background = '';
 btn.style.borderColor = '';
 timerCmd({ action: 'pause' });
 }
 }
 function timerCmd(payload) {
 try {
 fetch('/api/timer', {
 method: 'POST',
 headers: { 'Content-Type': 'application/json' },
 body: JSON.stringify(payload)
 }).catch(() => console.log('Timer cmd skipped (device unreachable)'));
 } catch (e) {}
 }

 function swReset() {
 swRunning = false;
 swAccum = 0;
 clearTimeout(swTimer);
 const btn = document.getElementById('swBtn');
 btn.textContent = 'Start';
 btn.style.background = '';
 btn.style.borderColor = '';
 document.getElementById('swDisplay').textContent = '00:00:00';
 timerCmd({ action: 'reset' });
 }

 let cdRunning = false, cdWasRunning = false, cdLeftMs = 0, cdEndEpoch = 0, cdTimer = null;
 function cdReadInputs() {
 let mins = parseInt(document.getElementById('cdMinutes').value) || 0;
 let secs = parseInt(document.getElementById('cdSeconds').value) || 0;
 if (mins < 0) mins = 0;
 if (mins > 120) mins = 120;
 if (secs < 0) secs = 0;
 if (secs > 59) secs = 59;
 let ms = mins * 60000 + secs * 1000;
 if (ms < 1000) { mins = 5; secs = 0; ms = 300000; }
 document.getElementById('cdMinutes').value = mins;
 document.getElementById('cdSeconds').value = secs;
 return ms;
 }
 function cdSetDisplay(ms) {
 const m = Math.floor(ms / 60000);
 const s = Math.floor(ms / 1000) % 60;
 document.getElementById('cdDisplay').textContent = `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
 }
 function cdTick() {
 if (!cdRunning) return;
 cdLeftMs = cdEndEpoch - Date.now();
 if (cdLeftMs <= 0) { cdFinish(); return; }
 cdSetDisplay(cdLeftMs);
 cdTimer = setTimeout(cdTick, 200);
 }
 function cdStart() {
 ensureAudio();
 const btn = document.getElementById('cdBtn');
 if (cdRunning) {
 cdRunning = false;
 clearTimeout(cdTimer);
 btn.textContent = 'Resume';
 timerCmd({ action: 'pause' });
 return;
 }
 const isFresh = !cdWasRunning || cdLeftMs <= 0;
 if (isFresh) {
 cdLeftMs = cdReadInputs();
 }
 cdWasRunning = true;
 cdRunning = true;
 cdEndEpoch = Date.now() + cdLeftMs;
 btn.textContent = 'Pause';
 btn.style.background = '#f59e0b';
 btn.style.borderColor = '#f59e0b';
 cdTick();
 timerCmd(isFresh ? { action: 'start', mode: 'cd', totalMs: cdLeftMs } : { action: 'resume' });
 }
 function cdReset() {
 cdRunning = false;
 cdWasRunning = false;
 clearTimeout(cdTimer);
 cdLeftMs = cdReadInputs();
 const btn = document.getElementById('cdBtn');
 btn.textContent = 'Start';
 btn.style.background = '';
 btn.style.borderColor = '';
 const disp = document.getElementById('cdDisplay');
 disp.style.color = '#38bdf8';
 cdSetDisplay(cdLeftMs);
 timerCmd({ action: 'reset' });
 }
 function cdPreset(m) {
 document.getElementById('cdMinutes').value = m;
 document.getElementById('cdSeconds').value = 0;
 cdReset();
 }
 function cdFinish() {
 cdRunning = false;
 cdWasRunning = false;
 clearTimeout(cdTimer);
 const btn = document.getElementById('cdBtn');
 btn.textContent = 'Start';
 btn.style.background = '';
 btn.style.borderColor = '';
 const disp = document.getElementById('cdDisplay');
 disp.textContent = 'DONE';
 disp.style.color = '#34d399';
 setTimeout(() => { if (!cdRunning) disp.style.color = '#38bdf8'; }, 5000);
 if (audioCtx) {
 [0, 0.4, 0.8].forEach(t => {
 const o = audioCtx.createOscillator();
 const g = audioCtx.createGain();
 o.type = 'sine';
 o.frequency.value = 880;
 o.connect(g);
 g.connect(audioCtx.destination);
 g.gain.setValueAtTime(0.001, audioCtx.currentTime + t);
 g.gain.exponentialRampToValueAtTime(0.3, audioCtx.currentTime + t + 0.02);
 g.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + t + 0.3);
 o.start(audioCtx.currentTime + t);
 o.stop(audioCtx.currentTime + t + 0.32);
 });
 }
 }

 cdLeftMs = cdReadInputs();
 cdSetDisplay(cdLeftMs);
 </script>
</body>
</html>
 )rawhtml";

inline void handleRoot() {
  appState.lastWebActivityTime = millis();
  // In captive-portal mode there is no uplink yet: send the provisioning page.
  if (appState.captivePortalMode) {
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send_P(200, "text/html; charset=utf-8", ROOT_HTML);
}

static const char TODO_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1">
 <title>DeskBuddy TODO</title>
 <link rel="stylesheet" href="/web/style.css">
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

 <div class="points-card">
 <div>
 <div style="font-size: 0.8rem; color: #94a3b8; font-weight: 600; text-transform: uppercase; letter-spacing: 0.04em;">Points · <span id="pointsMonth"></span></div>
 <div style="display: flex; align-items: baseline; gap: 10px; margin-top: 4px;">
 <span class="points-total" id="pointsTotal">0</span>
 <span class="points-badge good" id="pointsBadge">Good</span>
 </div>
 </div>
 <div class="points-months" id="pointsMonths"></div>
 </div>

 <div class="card">
 <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 8px; margin-bottom: 12px; flex-wrap: wrap; gap: 8px;">
 <h2 style="margin: 0; border: none; padding: 0;">Month Calendar</h2>
 <div style="display: flex; gap: 6px; align-items: center;">
 <button class="cal-btn" onclick="calMove(-1)" title="Previous month">&#9664;</button>
 <span id="calLabel" style="color: #38bdf8; font-weight: 700; min-width: 110px; text-align: center;"></span>
 <button class="cal-btn" onclick="calMove(1)" title="Next month">&#9654;</button>
 <button class="cal-btn" onclick="calToday()">Today</button>
 </div>
 </div>
 <div class="cal-grid" id="calGrid"></div>
 <div style="display: flex; gap: 14px; margin-top: 10px; font-size: 0.75rem; color: #94a3b8; flex-wrap: wrap;">
 <span><span class="cal-dot" style="background: #34d399;"></span> Done</span>
 <span><span class="cal-dot" style="background: #fbbf24;"></span> Due</span>
 <span><span class="cal-dot" style="background: #f43f5e;"></span> Missed</span>
 <span><span class="cal-dot" style="background: #475569;"></span> No tasks</span>
 </div>
 </div>

 <div class="card">
 <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 8px; margin-bottom: 15px;">
 <div style="display: flex; align-items: center; gap: 10px;">
 <h2 style="margin: 0; border: none; padding: 0;">Daily Tasks</h2>
 <span id="dailyTally" style="font-size: 0.8rem; color: #94a3b8; background: #1e293b; padding: 2px 8px; border-radius: 10px;"></span>
 </div>
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
 <div style="display: flex; align-items: center; gap: 10px;">
 <h2 style="margin: 0; border: none; padding: 0;">Monthly Tasks</h2>
 <span id="monthlyTally" style="font-size: 0.8rem; color: #94a3b8; background: #1e293b; padding: 2px 8px; border-radius: 10px;"></span>
 </div>
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
 renderCal();
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
 loadPoints();
 renderCal();
 } catch (err) {
 console.error('Error saving tasks:', err);
 }
 }

 async function loadPoints() {
 try {
 const res = await fetch('/api/points');
 if (!res.ok) return;
 const p = await res.json();
 const monthEl = document.getElementById('pointsMonth');
 const totalEl = document.getElementById('pointsTotal');
 const badgeEl = document.getElementById('pointsBadge');
 const monthsEl = document.getElementById('pointsMonths');
 if (!monthEl || !totalEl || !badgeEl || !monthsEl) return;
 const cur = p.currentMonth || '';
 monthEl.textContent = fmtMonthLabel(cur);
 const fmt = n => (n > 0 ? '+' + n : String(n));
 totalEl.textContent = fmt(p.running !== undefined ? p.running : 0);
 totalEl.style.color = p.running > 0 ? '#34d399' : p.running < 0 ? '#fb7185' : '#e2e8f0';
 badgeEl.textContent = p.category || 'good';
 badgeEl.className = 'points-badge ' + (p.category || 'good');
 monthsEl.innerHTML = '';
 const months = p.months || {};
 Object.keys(months).sort().forEach(k => {
 const chip = document.createElement('span');
 chip.className = 'points-chip';
 chip.innerHTML = fmtMonthLabel(k) + ' <span class="pt">' + fmt(months[k]) + '</span>';
 chip.title = 'Baked total for ' + fmtMonthLabel(k);
 monthsEl.appendChild(chip);
 });
 if (cur) {
 const chip = document.createElement('span');
 chip.className = 'points-chip current';
 chip.innerHTML = 'Now <span class="pt">' + fmt(p.running || 0) + '</span>';
 monthsEl.appendChild(chip);
 }
 } catch (err) {
 console.error('Error loading points:', err);
 }
 }

 let calMonth = null;

 function calTodayStr() {
 const now = new Date();
 return `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
 }

 function calDayInfo(dateStr, dayNum, monthKey) {
 const todayStr = calTodayStr();
 const todayMonth = todayStr.substring(0, 7);
 const dots = [];
 const daily = tasks.daily || [];
 const monthly = tasks.monthly || [];

 daily.forEach(t => {
 if (t.recurrent) {
 const s = t.startDate || '';
 const e = t.endDate || '';
 if ((!s || dateStr >= s) && (!e || dateStr < e)) {
 const done = (t.completedDates || []).includes(dateStr);
 dots.push(done ? 'done' : (dateStr < todayStr ? 'missed' : 'due'));
 }
 } else {
 if ((t.targetDate || '') === dateStr) {
 const done = !!t.completed;
 dots.push(done ? 'done' : (dateStr < todayStr ? 'missed' : 'due'));
 }
 }
 });

 monthly.forEach(t => {
 if ((t.day || 0) !== dayNum) return;
 let due = false, done = false;
 if (t.recurrent) {
 const s = t.startMonth || '';
 const e = t.endMonth || '';
 if ((!s || monthKey >= s) && (!e || monthKey < e)) due = true;
 done = (t.completedMonths || []).includes(monthKey);
 } else {
 if ((t.year || 0) === parseInt(monthKey.substring(0, 4), 10) &&
 (t.month || 0) === parseInt(monthKey.substring(5, 7), 10)) due = true;
 done = !!t.completed;
 }
 if (due) {
 dots.push(done ? 'done' : ((monthKey < todayMonth || (monthKey === todayMonth && dateStr < todayStr)) ? 'missed' : 'due'));
 }
 });

 return dots;
 }

 function renderCal() {
 const now = new Date();
 const curMonth = calMonth || `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}`;
 calMonth = curMonth;
 const cy = parseInt(curMonth.substring(0, 4), 10);
 const cm0 = parseInt(curMonth.substring(5, 7), 10) - 1;

 document.getElementById('calLabel').textContent = fmtMonthLabel(curMonth);

 const grid = document.getElementById('calGrid');
 grid.innerHTML = '';
 ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'].forEach(d => {
 const h = document.createElement('div');
 h.className = 'cal-head';
 h.textContent = d;
 grid.appendChild(h);
 });

 const todayStr = calTodayStr();
 const daysInMonth = new Date(cy, cm0 + 1, 0).getDate();
 const firstDow = new Date(cy, cm0, 1).getDay();

 for (let i = 0; i < firstDow; i++) {
 const b = document.createElement('div');
 b.className = 'cal-cell out';
 grid.appendChild(b);
 }

 for (let d = 1; d <= daysInMonth; d++) {
 const dateStr = `${curMonth}-${String(d).padStart(2, '0')}`;
 const cell = document.createElement('div');
 cell.className = 'cal-cell';
 if (dateStr === todayStr) cell.classList.add('today');

 const num = document.createElement('div');
 num.className = 'cal-daynum';
 num.textContent = d;
 cell.appendChild(num);

 const dots = calDayInfo(dateStr, d, curMonth);
 if (dots.length > 0) {
 const row = document.createElement('div');
 row.className = 'cal-dots';
 dots.slice(0, 3).forEach(k => {
 const dot = document.createElement('span');
 dot.className = 'cal-dot';
 dot.style.background = k === 'done' ? '#34d399' : k === 'due' ? '#fbbf24' : '#f43f5e';
 dot.title = k;
 row.appendChild(dot);
 });
 if (dots.length > 3) {
 const c = document.createElement('span');
 c.className = 'cal-count';
 c.textContent = '+' + (dots.length - 3);
 row.appendChild(c);
 }
 cell.appendChild(row);
 }

 cell.onclick = () => calPick(dateStr);
 grid.appendChild(cell);
 }
 }

 function calPick(dateStr) {
 const daySel = document.getElementById('dayHistorySelect');
 if (Array.from(daySel.options).find(o => o.value === dateStr)) daySel.value = dateStr;
 const mSel = document.getElementById('monthHistorySelect');
 const mKey = dateStr.substring(0, 7);
 if (Array.from(mSel.options).find(o => o.value === mKey)) mSel.value = mKey;
 renderLists();
 }

 function calMove(delta) {
 const [y, m] = calMonth.split('-').map(Number);
 let ny = y, nm = m + delta;
 while (nm < 1) { nm += 12; ny--; }
 while (nm > 12) { nm -= 12; ny++; }
 calMonth = `${ny}-${String(nm).padStart(2, '0')}`;
 renderCal();
 }

 function calToday() {
 const now = new Date();
 calMonth = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}`;
 const todayStr = calTodayStr();
 const daySel = document.getElementById('dayHistorySelect');
 if (Array.from(daySel.options).find(o => o.value === todayStr)) daySel.value = todayStr;
 renderCal();
 renderLists();
 }

 function renderLists() {
 renderList('daily', 'dailyList');
 renderList('monthly', 'monthlyList');
 }

 function fmtMonthLabel(key) {
 if (!key || key.length < 7) return '';
 const y = parseInt(key.substring(0, 4), 10);
 const m = parseInt(key.substring(5, 7), 10) - 1;
 const abbr = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
 return `${abbr[m]} ${y}`;
 }

 function updateTally(elId, done, total) {
 const net = 2 * done - total;
 const color = net > 0 ? '#34d399' : net < 0 ? '#f43f5e' : '#94a3b8';
 const sign = net > 0 ? '+' + net : String(net);
 document.getElementById(elId).innerHTML = `${done} / ${total} completed <span style="color:${color}; font-weight:700;">${sign}</span>`;
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
 if (task.completed) {
 isActive = (task.targetDate === selectedDateStr);
 } else {
 isActive = !task.targetDate || selectedDateStr >= task.targetDate;
 }
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

 const dailyDone = activeTasks.filter(i => i.isCompleted).length;
 updateTally('dailyTally', dailyDone, activeTasks.length);

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
 if (!task.recurrent && task.targetDate && task.targetDate < curDateStr) {
 isOverdue = true;
 } else if (selectedDateStr < curDateStr) {
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
 
 let deadlineHtml;
 if (!task.recurrent && task.targetDate && task.targetDate < curDateStr) {
 const odM = task.targetDate.substring(5, 7);
 const odD = task.targetDate.substring(8, 10);
 deadlineHtml = `<span class="task-deadline" style="font-size: 0.75rem; color: #f43f5e; background: #27272a; padding: 2px 6px; border-radius: 4px; font-weight: 600; margin-left: 8px;">Overdue from ${odD}/${odM}</span>`;
 } else {
 deadlineHtml = `<span class="task-deadline" style="font-size: 0.75rem; color: ${badgeColor}; background: #27272a; padding: 2px 6px; border-radius: 4px; font-weight: 600; margin-left: 8px;">Due: ${formatTimeDeadline(task.hour, tMin)}</span>`;
 }

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
 
 let activeTasks = [];
 list.forEach((task, index) => {
 if (task.recurrent) {
 const startM = task.startMonth || "";
 const endM = task.endMonth || "";
 if (endM && selectedMonthStr >= endM) return;
 const completed = task.completedMonths || [];
 let [y, m] = (startM || selectedMonthStr).split('-').map(Number);
 while (true) {
 const key = `${String(y).padStart(4, '0')}-${String(m).padStart(2, '0')}`;
 if (key > selectedMonthStr) break;
 const done = completed.includes(key);
 if (key === selectedMonthStr || !done) {
 activeTasks.push({ task: task, originalIndex: index, occMonth: key, isCompleted: done });
 }
 m++;
 if (m > 12) { m = 1; y++; }
 }
 } else {
 let occMonth = null;
 let isActive = false;
 if (task.year && task.month) {
 const targetKey = `${String(task.year).padStart(4, '0')}-${String(task.month).padStart(2, '0')}`;
 occMonth = targetKey;
 isActive = task.completed ? (selectedMonthStr === targetKey) : (selectedMonthStr >= targetKey);
 } else {
 isActive = true;
 }
 if (isActive) {
 activeTasks.push({ task: task, originalIndex: index, occMonth: occMonth, isCompleted: task.completed });
 }
 }
 });
 
 // Sort active tasks: uncompleted first, then completed. Secondary sort by day.
 activeTasks.sort((a, b) => {
 if (a.isCompleted !== b.isCompleted) {
 return a.isCompleted ? 1 : -1;
 }
 return a.task.day - b.task.day;
 });

 const monthlyDone = activeTasks.filter(i => i.isCompleted).length;
 updateTally('monthlyTally', monthlyDone, activeTasks.length);

 if (activeTasks.length === 0) {
 container.innerHTML = '<div class="empty-state">No tasks scheduled for this month.</div>';
 return;
 }
 
 activeTasks.forEach(itemInfo => {
 const task = itemInfo.task;
 const index = itemInfo.originalIndex;
 const isCompleted = itemInfo.isCompleted;
 const shownMonth = itemInfo.occMonth || selectedMonthStr;
 
 const now = new Date();
 const curMonthStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}`;
 const currentDay = now.getDate();
 
 let isOverdue = false;
 if (!isCompleted) {
 if (shownMonth < curMonthStr || (shownMonth === curMonthStr && task.day < currentDay)) {
 isOverdue = true;
 }
 }
 
 const badgeColor = isOverdue ? '#f43f5e' : '#9ca3af';
 const recurrentIndicator = task.recurrent ? `<span style="color: #3b82f6; font-weight: bold; font-size: 0.85rem; margin-right: 8px;" title="Recurrent">R</span>` : '';

 const item = document.createElement('div');
 item.className = `task-item ${isCompleted ? 'completed' : ''}`;
 const occArg = task.recurrent ? `, '${shownMonth}'` : '';
 item.innerHTML = `
 <div class="task-left">
 <input type="checkbox" ${isCompleted ? 'checked' : ''} onchange="toggleTask('${type}', ${index}${occArg})">
 <span class="task-text">${escapeHtml(task.text)}</span>
 <span class="task-deadline" style="font-size: 0.75rem; color: ${badgeColor}; background: #27272a; padding: 2px 6px; border-radius: 4px; font-weight: 600; margin-left: 8px; white-space: nowrap;">${isOverdue ? 'Overdue' : 'Due'}: Day ${task.day} · ${fmtMonthLabel(shownMonth)}</span>
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

 function toggleTask(type, index, occMonth) {
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
 const monthToToggle = occMonth || selectedMonthStr;
 const idx = task.completedMonths.indexOf(monthToToggle);
 if (idx > -1) {
 task.completedMonths.splice(idx, 1);
 } else {
 task.completedMonths.push(monthToToggle);
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
 loadPoints();
 renderCal();
 };
 </script>
</body>
</html>
 )rawhtml";

inline void handleTodo() {
  server.send_P(200, "text/html; charset=utf-8", TODO_HTML);
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
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request: No payload");
    return;
  }
  String payload = server.arg("plain");

  DynamicJsonDocument newDoc(8192);
  DeserializationError err = deserializeJson(newDoc, payload);
  if (err || !newDoc.containsKey("daily") || !newDoc.containsKey("monthly")) {
    server.send(400, "text/plain", "Bad Request: Invalid JSON payload");
    return;
  }

  DynamicJsonDocument oldDoc(8192);
  if (LittleFS.exists("/todo.json")) {
    fs::File f = LittleFS.open("/todo.json", "r");
    if (f) {
      deserializeJson(oldDoc, f);
      f.close();
    }
  }

  // Points diff+award only when the clock knows the current month.
  String curMonth = "";
  if (timeClient.isTimeSet()) {
    time_t e = timeClient.getEpochTime();
    struct tm* ptm = localtime(&e);
    if (ptm != nullptr) {
      char b[8];
      snprintf(b, sizeof(b), "%04d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1);
      curMonth = String(b);
    }
  }

  if (curMonth.length() == 7) {
    pointsEnsure(oldDoc.as<JsonObject>(), curMonth);
    String ptsMonth = oldDoc["points"]["currentMonth"] | "";
    if (ptsMonth.length() == 7 && ptsMonth != curMonth) {
      pointsBake(oldDoc.as<JsonObject>(), curMonth);
      ptsMonth = curMonth;
    }
    int delta = pointsApplyDeltas(oldDoc.as<JsonObject>(), newDoc.as<JsonObject>(), ptsMonth);
    long running = pointsAdd(oldDoc["points"]["running"] | 0L, delta);
    newDoc["points"] = oldDoc["points"].as<JsonVariant>();
    newDoc["points"]["running"] = running;
    newDoc["points"]["currentMonth"] = ptsMonth;
  }

  if (pointsSaveDoc(newDoc)) {
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(500, "text/plain", "Failed to open todo.json for writing");
  }
}

inline void handleGetPoints() {
  long running = 0;
  String curMonth = "";
  DynamicJsonDocument out(2048);
  if (LittleFS.exists("/todo.json")) {
    fs::File file = LittleFS.open("/todo.json", "r");
    if (file) {
      DynamicJsonDocument doc(8192);
      if (deserializeJson(doc, file) == DeserializationError::Ok && doc.containsKey("points")) {
        JsonObject p = doc["points"];
        running = p["running"] | 0L;
        curMonth = p["currentMonth"] | "";
        out["currentMonth"] = curMonth;
        out["running"] = running;
        if (p.containsKey("months")) out["months"] = p["months"];
      }
      file.close();
    }
  }
  if (curMonth.length() == 0) {
    if (timeClient.isTimeSet()) {
      time_t e = timeClient.getEpochTime();
      struct tm* ptm = localtime(&e);
      if (ptm != nullptr) {
        char b[8];
        snprintf(b, sizeof(b), "%04d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1);
        curMonth = String(b);
      }
    }
  }
  out["currentMonth"] = curMonth;
  out["running"] = running;
  out["category"] = pointsCategory(running, appConfig.pointsPoorMax, appConfig.pointsExcellentMin);
  out["poorMax"] = appConfig.pointsPoorMax;
  out["excellentMin"] = appConfig.pointsExcellentMin;
  String resp;
  serializeJson(out, resp);
  server.send(200, "application/json", resp);
}

inline void handleTimerApi() {
  if (server.method() == HTTP_GET) {
    unsigned long now = millis();
    DynamicJsonDocument out(256);
    out["mode"] = timerState.mode;
    out["running"] = timerState.running;
    out["ms"] = timerCurrentMs(now);
    out["hold"] = (now < timerState.holdUntil);
    String resp;
    serializeJson(out, resp);
    server.send(200, "application/json", resp);
    return;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  String action = doc["action"] | "";
  action.toUpperCase();
  String mode = doc["mode"] | "sw";
  unsigned long totalMs = doc["totalMs"] | 0UL;

  if (action != "START" && action != "PAUSE" && action != "RESUME" && action != "RESET") {
    server.send(400, "application/json", "{\"error\":\"invalid action\"}");
    return;
  }
  timerCommand(action, mode, totalMs, millis());
  Logger::log("WEB", "Timer cmd: %s mode=%s totalMs=%lu", action.c_str(), mode.c_str(), totalMs);
  server.send(200, "application/json", "{\"ok\":true}");
}

inline void handleSetOdometer() {
  if (server.hasArg("active")) {
    int val = server.arg("active").toInt();
    if (val >= 0 && val < 4) {
      appStats.activeOdometer = val;
      preferences.putInt("activeOdo", val);
      saveDailyStats();
      Logger::log("WEB", "Active odometer set to %d", val);
      server.send(200, "application/json", "{\"status\":\"success\",\"active\":" + String(val) + "}");
      return;
    }
  } else if (server.hasArg("plain")) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
      int val = doc["active"] | -1;
      if (val >= 0 && val < 4) {
        appStats.activeOdometer = val;
        preferences.putInt("activeOdo", val);
        saveDailyStats();
        Logger::log("WEB", "Active odometer set to %d via JSON", val);
        server.send(200, "application/json", "{\"status\":\"success\",\"active\":" + String(val) + "}");
        return;
      }
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"invalid active param\"}");
}

inline void handleSetOdometerLabels() {
  bool updated = false;
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
      if (doc.containsKey("labels")) {
        JsonArray arr = doc["labels"];
        for (int i = 0; i < 4 && i < (int)arr.size(); i++) {
          String lbl = arr[i].as<String>();
          lbl.trim();
          if (lbl.length() > 0 && lbl.length() <= 16) {
            appConfig.odometerLabels[i] = lbl;
            char key[12];
            snprintf(key, sizeof(key), "odoLbl%d", i);
            preferences.putString(key, lbl);
            updated = true;
          }
        }
      }
    }
  } else {
    for (int i = 0; i < 4; i++) {
      char param[12];
      snprintf(param, sizeof(param), "label%d", i);
      if (server.hasArg(param)) {
        String lbl = server.arg(param);
        lbl.trim();
        if (lbl.length() > 0 && lbl.length() <= 16) {
          appConfig.odometerLabels[i] = lbl;
          char key[12];
          snprintf(key, sizeof(key), "odoLbl%d", i);
          preferences.putString(key, lbl);
          updated = true;
        }
      }
    }
  }
  if (updated) {
    saveDailyStats();
    Logger::log("WEB", "Odometer labels updated");
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"no valid labels provided\"}");
  }
}

static const char SETTINGS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1">
 <title>DeskBuddy Settings</title>
 <link rel="stylesheet" href="/web/style.css">
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
 <span class="label">Alert Frequency</span>
 <select name="aiMode" id="aiModeSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">Off</option>
 <option value="1">Normal</option>
 <option value="2">Chatty</option>
 </select>
 </div>
 <div class="metric">
 <span class="label">AI Persona</span>
 <select name="aiPersona" id="aiPersonaSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">Coach</option>
 <option value="1">Critic</option>
 <option value="2">Sweet</option>
 <option value="3">Friend</option>
 </select>
 </div>
 <div class="metric">
 <span class="label">Clock Face Style</span>
 <select name="clockFace" id="clockFaceSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">Default Digital</option>
 <option value="1">Minimalist</option>
 <option value="2">HiTech</option>
 <option value="3">DEV Mode</option>
 <option value="4">Aviator</option>
 <option value="5">Deskbuddy</option>
 <option value="6">DeskAura</option>
 <option value="7">DeskCat</option>
 <option value="8">DeskWho</option>
 <option value="9">DeskBit</option>
 </select>
 </div>
 <div class="metric">
 <span class="label">Time Format</span>
 <select name="time24h" id="time24hSelect" class="settings-select" onchange="this.form.submit()">
 <option value="1">24-Hour</option>
 <option value="0">12-Hour</option>
 </select>
 </div>
 <div class="metric">
 <span class="label">Temperature Unit</span>
 <select name="tempUnitF" id="tempUnitSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">Celsius (&deg;C)</option>
 <option value="1">Fahrenheit (&deg;F)</option>
 </select>
 </div>
 <div class="metric">
 <span class="label">User Name</span>
 <input type="text" name="userName" id="userNameInput" class="settings-input" maxlength="15" style="width: 125px;">
 </div>

 <div class="metric">
 <span class="label">Daily Target Hours</span>
 <input type="number" step="0.1" min="0.1" max="24.0" name="targetHours" id="targetHoursInput" class="settings-input">
 </div>
 <div class="metric">
 <span class="label">Mail Alert Active</span>
 <select name="hasMail" id="hasMailSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">No Mail</option>
 <option value="1">Mail Active</option>
 </select>
 </div>
 <div class="metric" style="border-bottom: none;">
 <span class="label">Anonymous Telemetry</span>
 <select name="telemEn" id="telemEnSelect" class="settings-select" onchange="this.form.submit()">
 <option value="0">Off</option>
 <option value="1">On (Opt-in)</option>
 </select>
 </div>
 <div style="font-size: 0.72rem; color: #64748b; margin: 8px 0 0 0; padding: 0 0 12px 0; line-height: 1.35; border-bottom: 1px solid #334155;">
 Sends anonymous usage stats (clock face, AI mode, desk hours, tasks, firmware version) to help improve DeskBuddy. No personal data, WiFi credentials, or API keys are ever sent.
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
 <div style="text-align: center; margin-top: 15px;">
 <button type="submit" class="btn">Save Configuration</button>
 </div>
 </form>
 </div>
 )rawhtml"
#if DESKBUDDY_DEBUG
  R"rawhtml(

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
 )rawhtml"
#endif
  R"rawhtml(
 )rawhtml"
#if DESKBUDDY_DEBUG
  R"rawhtml(
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
 )rawhtml"
#endif
  R"rawhtml(

 <div class="card" style="text-align: center;">
 <h1>System Actions</h1>
 )rawhtml"
#if DESKBUDDY_DEBUG
  R"rawhtml(
 <div style="margin-bottom: 15px;">
 <a href="/file-manager" class="btn" style="background: #10b981; color: white; display: inline-flex; width: 100%; box-sizing: border-box; justify-content: center; align-items: center; gap: 6px; padding: 10px 12px; text-decoration: none;">
 <svg viewBox="0 0 24 24" style="width: 18px; height: 18px; fill: currentColor;">
 <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/>
 </svg>
 Open File Manager
 </a>
 </div>
 )rawhtml"
#endif
  R"rawhtml(
 <div style="margin-bottom: 15px;">
 <div style="display: flex; gap: 10px;">
 <a href="/download?path=/stats.json" class="btn" style="background: #334155; color: #e2e8f0; display: inline-flex; flex: 1; box-sizing: border-box; justify-content: center; align-items: center; gap: 6px; padding: 10px 12px; text-decoration: none;">
 <svg viewBox="0 0 24 24" style="width: 18px; height: 18px; fill: currentColor;">
 <path d="M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z"/>
 </svg>
 Stats
 </a>
 <a href="/download?path=/todo.json" class="btn" style="background: #334155; color: #e2e8f0; display: inline-flex; flex: 1; box-sizing: border-box; justify-content: center; align-items: center; gap: 6px; padding: 10px 12px; text-decoration: none;">
 <svg viewBox="0 0 24 24" style="width: 18px; height: 18px; fill: currentColor;">
 <path d="M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z"/>
 </svg>
 Tasks
 </a>
 </div>
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
 )rawhtml"
#if DESKBUDDY_DEBUG
  R"rawhtml(
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
 <option value="13">Page Follow-up (13)</option>
 <option value="14">Late Hours Sit (14)</option>
 <option value="15">Points Check-in (15)</option>
 <option value="16">Curation Nudge (16)</option>
 </select>
 <select id="debugMsgMode" style="width: 110px; background: #0f172a; border: 1px solid #334155; color: white; border-radius: 6px; padding: 8px; font-size: 0.9rem;">
 <option value="ai">AI Msg</option>
 <option value="fallback">Fallback</option>
 </select>
 <button class="btn" style="background: #a855f7; color: white; padding: 8px 12px; font-size: 0.9rem;" onclick="triggerDebugEvent()">Trigger</button>
 </div>
 </div>
 )rawhtml"
#endif
  R"rawhtml(
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
 setVal('telemEnSelect', data.telemetryEnabled ? "1" : "0");
 setVal('time24hSelect', data.time24h ? "1" : "0");
 setVal('tempUnitSelect', data.tempUnitF ? "1" : "0");
 
 setVal('focusDistLimSlider', data.focusDistLim);
 setTxt('focusDistLimVal', data.focusDistLim + ' cm');
 
 setVal('motionRatioLimSlider', data.motionRatioLim);
 setTxt('motionRatioLimVal', data.motionRatioLim + '%');
 
 setVal('distLimitSlider', data.distLimit);
 setTxt('distLimitVal', data.distLimit + ' cm');

 setVal('filterWindowSlider', data.filterWindow);
 setTxt('filterWindowVal', parseFloat(data.filterWindow).toFixed(1) + 's');
 
 window.settingsPopulated = true;
 }

 setTimeout(updateRadarChartAndSettings, 500);
 })
 .catch(err => {
 console.error("Error fetching radar data:", err);
 setTimeout(updateRadarChartAndSettings, 500);
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

inline void handleSettings() {
  server.send_P(200, "text/html; charset=utf-8", SETTINGS_HTML);
}

static const char CREDENTIALS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1">
 <title>DeskBuddy Credentials</title>
 <link rel="stylesheet" href="/web/style.css">
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
 <option value="0">No</option>
 <option value="1">Yes</option>
</select>
 </div>
 <div id="staticFields">
 <div style="display: flex; gap: 8px; margin-top: 8px;">
 <div style="flex: 1;">
 <label class="field-label">IP Address</label>
 <input type="text" name="wifiIp" id="wifiIp" class="settings-input" placeholder="192.168.1.100">
 </div>
 <div style="flex: 1;">
 <label class="field-label">Gateway</label>
 <input type="text" name="wifiGw" id="wifiGw" class="settings-input" placeholder="192.168.1.1">
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
 <input type="text" name="mqttBroker" id="mqttBroker" class="settings-input" placeholder="broker host/IP">
 </div>
 <div style="flex: 1;">
 <label class="field-label">Port</label>
 <input type="number" name="mqttPort" id="mqttPort" class="settings-input" placeholder="1883">
 </div>
 </div>
 </div>

 <div class="card">
 <h1>Telemetry</h1>
 <div class="field-group">
 <label class="field-label">Endpoint URL <span style="color: #64748b; font-size: 0.75rem;">(Cloudflare Worker)</span></label>
 <input type="text" name="telemUrl" id="telemUrl" class="settings-input" placeholder="https://your-worker.workers.dev">
 <span class="field-help">Where anonymous usage data is sent. Enable telemetry on the Settings page.</span>
 </div>
 <div class="field-group" style="margin-top: 12px;">
 <div class="metric" style="border: none; padding: 4px 0;">
 <span class="label">Current Firmware</span>
 <span class="value" id="currentFwVer" style="color: #38bdf8; font-family: monospace;">--</span>
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
 // wifiPass is intentionally NOT prefilled; leave blank to keep the current one.
 document.getElementById('wifiStaticSelect').value = data.wifiStatic ? '1' : '0';
 document.getElementById('wifiIp').value = data.wifiIp || '';
 document.getElementById('wifiGw').value = data.wifiGw || '';
 document.getElementById('wifiSubnet').value = data.wifiSubnet || '';
 document.getElementById('wifiDns1').value = data.wifiDns1 || '';
 document.getElementById('wifiDns2').value = data.wifiDns2 || '';
 toggleStaticFields();
 document.getElementById('mqttBroker').value = data.mqttBroker || '';
 document.getElementById('mqttPort').value = data.mqttPort || 1883;
 document.getElementById('telemUrl').value = data.telemetryEndpoint || '';
 document.getElementById('currentFwVer').textContent = data.fwVersion || 'unknown';
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

inline void handleCredentials() {
  server.send_P(200, "text/html; charset=utf-8", CREDENTIALS_HTML);
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

// Captive portal: focused, mobile-friendly provisioning page (WiFi + static IP
// only). Full MQTT/API-key config stays on /credentials after connecting.
static const char SETUP_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
 <meta name="theme-color" content="#0f172a">
 <title>DeskBuddy Setup</title>
 <link rel="stylesheet" href="/web/style.css">
 <style>
  html,body{padding:0;margin:0}
  body{padding:14px;display:block}
  .card{margin:10px auto;max-width:480px;box-sizing:border-box}
  .setup-title{text-align:center;margin:6px 10px 10px}
  .setup-title h1{margin:0;font-size:1.5rem}
  .setup-title p{color:#94a3b8;margin:6px 0 0;font-size:.95rem}
  .step{display:flex;align-items:center;gap:10px;margin-bottom:4px}
  .step h2{margin:0;border:none;padding:0;font-size:1.05rem}
  .step-num{width:24px;height:24px;border-radius:50%;background:#38bdf8;color:#0f172a;font-weight:800;display:flex;align-items:center;justify-content:center;font-size:.8rem;flex-shrink:0}
  input[type="text"],input[type="password"],select{font-size:16px;min-height:48px;padding:12px}
  input[type="text"],input[type="password"]{width:100%}
  .settings-input{box-sizing:border-box}
  .ssid-row{display:flex;gap:8px;align-items:center}
  .ssid-row input{flex:1;min-width:0}
  .btn-scan{min-height:48px;padding:0 16px}
  .btn{min-height:52px;width:100%;font-size:1.05rem;padding:14px}
  .field-row{display:flex;gap:8px;flex-wrap:wrap}
  .field-row>div{flex:1;min-width:140px}
  #apList{max-width:480px;margin-left:auto;margin-right:auto}
 </style>
</head>
<body>
 <div class="setup-title">
  <h1>DeskBuddy</h1>
  <p>Connect to your WiFi to get started</p>
 </div>

 <form action="/save-credentials" method="POST">
 <div class="card">
  <div class="step"><span class="step-num">1</span><h2>WiFi Network</h2></div>
  <div class="field-group">
   <label class="field-label">Network Name (SSID)</label>
   <div class="ssid-row">
    <input type="text" name="wifiSsid" id="wifiSsid" class="settings-input" placeholder="e.g. Marina_2.4G" autocomplete="off" autocapitalize="none" spellcheck="false">
    <button type="button" class="btn-scan" id="scanBtn" onclick="scanWifi()">Scan</button>
   </div>
   <div id="apList"></div>
  </div>
  <div class="field-group">
   <label class="field-label">Password</label>
   <input type="password" name="wifiPass" id="wifiPass" class="settings-input" placeholder="WiFi password" autocomplete="off">
   <span class="field-help">Leave blank to keep the current password if one is already set.</span>
  </div>
 </div>

 <div class="card">
  <div class="step"><span class="step-num">2</span><h2>IP Settings</h2></div>
  <div class="field-group">
   <div style="display:flex;justify-content:space-between;align-items:center">
    <label class="field-label" style="margin:0">Use static IP</label>
    <select name="wifiStatic" id="wifiStaticSelect" class="settings-input" style="width:auto;min-width:110px" onchange="toggleStaticFields()">
 <option value="0">No (DHCP)</option>
 <option value="1">Yes</option>
</select>
 </div>
 </div>
 <div id="staticFields">
   <div class="field-row">
 <div><label class="field-label">IP address</label><input type="text" name="wifiIp" id="wifiIp" class="settings-input" placeholder="192.168.1.100" autocomplete="off" autocapitalize="none" spellcheck="false"></div>
 <div><label class="field-label">Gateway</label><input type="text" name="wifiGw" id="wifiGw" class="settings-input" placeholder="192.168.1.1" autocomplete="off" autocapitalize="none" spellcheck="false"></div>
   </div>
   <div class="field-row">
    <div><label class="field-label">Subnet</label><input type="text" name="wifiSubnet" id="wifiSubnet" class="settings-input" placeholder="255.255.255.0" autocomplete="off" autocapitalize="none" spellcheck="false"></div>
    <div><label class="field-label">Primary DNS</label><input type="text" name="wifiDns1" id="wifiDns1" class="settings-input" placeholder="1.1.1.1" autocomplete="off" autocapitalize="none" spellcheck="false"></div>
   </div>
   <div class="field-group" style="border:none;padding:0;margin-top:12px">
    <label class="field-label">Secondary DNS</label>
    <input type="text" name="wifiDns2" id="wifiDns2" class="settings-input" placeholder="8.8.8.8" autocomplete="off" autocapitalize="none" spellcheck="false">
   </div>
  </div>
 </div>

 <div class="card" style="text-align:center">
  <button type="submit" class="btn">Save &amp; Connect</button>
  <p class="help">After connecting, open the device IP to set MQTT, API keys and more.</p>
 </div>
 </form>

 <script>
 function toggleStaticFields() {
  var sel = document.getElementById('wifiStaticSelect');
  document.getElementById('staticFields').style.display = (sel.value === '1') ? 'block' : 'none';
 }
 function rssiToPercent(rssi) { if (rssi <= -100) return 0; if (rssi >= -50) return 100; return 2 * (rssi + 100); }
 function signalBars(pct) { if (pct > 75) return '&#9650;&#9650;&#9650;'; if (pct > 50) return '&#9650;&#9650;'; if (pct > 25) return '&#9650;'; return ''; }
 function scanWifi() {
  var btn = document.getElementById('scanBtn');
  var list = document.getElementById('apList');
  btn.disabled = true; btn.textContent = 'Scanning...';
  list.innerHTML = ''; list.classList.add('open');
  fetch('/wifi-scan').then(function(r) { return r.json(); }).then(function(aps) {
   btn.disabled = false; btn.textContent = 'Scan';
   if (!aps.length) { list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#64748b;">No networks found</span></div>'; return; }
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
  }).catch(function() {
   btn.disabled = false; btn.textContent = 'Scan';
   list.innerHTML = '<div class="ap-item"><span class="ap-name" style="color:#ef4444;">Scan failed</span></div>';
  });
 }
 function selectAP(el) {
  document.getElementById('wifiSsid').value = el.dataset.ssid;
  document.getElementById('apList').classList.remove('open');
  var pw = document.getElementById('wifiPass');
  if (el.dataset.secure === '0') { pw.value = ''; pw.placeholder = 'Open network - no password'; }
  else { pw.placeholder = 'WiFi password'; }
 }
 fetch('/radar-data').then(function(r) { return r.json(); }).then(function(d) {
  document.getElementById('wifiSsid').value = d.wifiSsid || '';
  document.getElementById('wifiStaticSelect').value = d.wifiStatic ? '1' : '0';
  document.getElementById('wifiIp').value = d.wifiIp || '';
  document.getElementById('wifiGw').value = d.wifiGw || '';
  document.getElementById('wifiSubnet').value = d.wifiSubnet || '';
  document.getElementById('wifiDns1').value = d.wifiDns1 || '';
  document.getElementById('wifiDns2').value = d.wifiDns2 || '';
  toggleStaticFields();
 }).catch(function() {});
 document.querySelector('form').addEventListener('submit', function(e) {
  if (!confirm('Save and connect DeskBuddy?')) e.preventDefault();
 });
 </script>
</body>
</html>
 )rawhtml";

inline void handleSetup() {
  server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
}

inline void handleSaveCredentials() {
  preferences.begin("deskbuddy", false);

  if (server.hasArg("wifiSsid")) { appConfig.wifiSsid = server.arg("wifiSsid"); preferences.putString("wifiSsid", appConfig.wifiSsid.c_str()); }
  if (server.hasArg("wifiPass")) {
    String val = server.arg("wifiPass");
    if (val.length() > 0) { appConfig.wifiPass = val; preferences.putString("wifiPass", appConfig.wifiPass.c_str()); }
  }
  if (server.hasArg("wifiStatic")) { appConfig.wifiStaticEnabled = (server.arg("wifiStatic").toInt() == 1); preferences.putBool("wifiStatic", appConfig.wifiStaticEnabled); }
  if (server.hasArg("wifiIp")) { appConfig.wifiIp = server.arg("wifiIp"); preferences.putString("wifiIp", appConfig.wifiIp.c_str()); }
  if (server.hasArg("wifiGw")) { appConfig.wifiGw = server.arg("wifiGw"); preferences.putString("wifiGw", appConfig.wifiGw.c_str()); }
  if (server.hasArg("wifiSubnet")) { appConfig.wifiSubnet = server.arg("wifiSubnet"); preferences.putString("wifiSubnet", appConfig.wifiSubnet.c_str()); }
  if (server.hasArg("wifiDns1")) { appConfig.wifiDns1 = server.arg("wifiDns1"); preferences.putString("wifiDns1", appConfig.wifiDns1.c_str()); }
  if (server.hasArg("wifiDns2")) { appConfig.wifiDns2 = server.arg("wifiDns2"); preferences.putString("wifiDns2", appConfig.wifiDns2.c_str()); }

  // Safety: static IP requires a usable IP address; otherwise fall back to DHCP
  if (appConfig.wifiStaticEnabled && appConfig.wifiIp.length() == 0) {
    appConfig.wifiStaticEnabled = false;
    preferences.putBool("wifiStatic", false);
  }

  if (server.hasArg("mqttBroker")) { appConfig.mqttBroker = server.arg("mqttBroker"); preferences.putString("mqttBroker", appConfig.mqttBroker.c_str()); }
  if (server.hasArg("mqttPort")) { appConfig.mqttPort = server.arg("mqttPort").toInt(); preferences.putInt("mqttPort", appConfig.mqttPort); }

  if (server.hasArg("telemUrl")) {
    String val = server.arg("telemUrl");
    if (val.length() > 0) { appConfig.telemetryEndpoint = val; preferences.putString("telemUrl", appConfig.telemetryEndpoint.c_str()); }
  }

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
  appState.lastWebActivityTime = millis();
  DynamicJsonDocument doc(4096);
  doc["presence"] = (appState.currentPresenceState != STATE_AWAY);
  doc["state"] = getPresenceStateName(appState.currentPresenceState);
  doc["presenceDetected"] = appState.sensorPresenceDetected;
  doc["rawPresent"] = appState.rawPresent;
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
  doc["buddyFontIdx"] = appConfig.buddyFontIndex;
  doc["targetHours"] = appConfig.targetHours;
  doc["hasMail"] = appConfig.hasMail;
  doc["telemetryEnabled"] = appConfig.telemetryEnabled;
  doc["telemetryEndpoint"] = appConfig.telemetryEndpoint;
  doc["fwVersion"] = DESKBUDDY_VERSION;
  doc["time24h"] = appConfig.time24h;
  doc["tempUnitF"] = appConfig.tempUnitF;
  doc["userName"] = appConfig.userName;
  doc["focusDistLim"] = appConfig.focusDistanceLimit;
  doc["motionRatioLim"] = appConfig.motionRatioLimit;
  doc["motionRatio"] = (appState.sessionDeskTime > 0) ? std::min((int)((appState.sessionMotionTime * 100) / appState.sessionDeskTime), 100) : 0;
  doc["recentMotionRatio"] = appState.recentMotionRatio;
  doc["totalMotionTime"] = formatTime(appStats.totalMotionTime);
  doc["motionCount"] = appStats.motionCount;
  doc["distLimit"] = appConfig.deskDistanceLimit;
  doc["filterWindow"] = appConfig.filterWindow;
  doc["motionWindow"] = appConfig.motionWindow;
  doc["pointsPoorMax"] = appConfig.pointsPoorMax;
  doc["pointsExcellentMin"] = appConfig.pointsExcellentMin;
  
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
  // Note: wifiPass is intentionally NOT echoed to clients.
  doc["wifiSsid"] = appConfig.wifiSsid;
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
  
  // Add AI response thread-safely (bounded so the 4KB doc cannot overflow)
  String aiMsg;
  bool isAiGen = false;
  xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
  aiMsg = appState.aiResponse;
  isAiGen = appState.lastResponseIsAi;
  xSemaphoreGive(appState.aiMutex);
  if (aiMsg.length() > 1500) aiMsg = aiMsg.substring(0, 1500);
  doc["aiMessage"] = aiMsg;
  doc["isAiGenerated"] = isAiGen;
  doc["aiLoading"] = appState.isAILoading;
  
  String json;
  json.reserve(2500); // single allocation; avoids 4-5 incremental reallocs per poll
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

inline void handleTftMessages() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("messages");
  int start = tftMsgHistory.head;
  for (int i = 0; i < tftMsgHistory.count; i++) {
    int idx = (start - 1 - i + TftMessageHistory::MAX_MSGS) % TftMessageHistory::MAX_MSGS;
    JsonObject obj = arr.createNestedObject();
    obj["epoch"] = tftMsgHistory.buffer[idx].epoch;
    obj["text"] = tftMsgHistory.buffer[idx].text;
    obj["eventType"] = tftMsgHistory.buffer[idx].eventType;
    obj["isAi"] = tftMsgHistory.buffer[idx].isAi;
  }
  String json;
  json.reserve(2000); // single allocation; avoids 4-5 incremental reallocs per poll
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
    if (server.hasArg("buddyFontIdx")) {
      int val = server.arg("buddyFontIdx").toInt();
      if (val != appConfig.buddyFontIndex) { appConfig.buddyFontIndex = val; preferences.putInt("buddyFontIdx", appConfig.buddyFontIndex); }
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
    if (server.hasArg("tempUnitF")) {
      bool val = (server.arg("tempUnitF").toInt() == 1);
      if (val != appConfig.tempUnitF) { appConfig.tempUnitF = val; preferences.putBool("tempUnitF", appConfig.tempUnitF); }
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
    if (server.hasArg("motionWindow")) {
      int val = server.arg("motionWindow").toInt();
      if (val < 1) val = 1;
      if (val > RECENT_MOTION_WINDOW_S) val = RECENT_MOTION_WINDOW_S;
      if (val != appConfig.motionWindow) { appConfig.motionWindow = val; preferences.putInt("motionWindow", appConfig.motionWindow); }
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
    if (server.hasArg("pointsPoorMax")) {
      int val = server.arg("pointsPoorMax").toInt();
      if (val != appConfig.pointsPoorMax) { appConfig.pointsPoorMax = val; preferences.putInt("pointsPoorMax", appConfig.pointsPoorMax); }
    }
    if (server.hasArg("pointsExcellentMin")) {
      int val = server.arg("pointsExcellentMin").toInt();
      if (val != appConfig.pointsExcellentMin) { appConfig.pointsExcellentMin = val; preferences.putInt("pointsExcellentMin", appConfig.pointsExcellentMin); }
    }

    if (server.hasArg("telemEn")) {
      bool val = (server.arg("telemEn").toInt() == 1);
      if (val != appConfig.telemetryEnabled) { appConfig.telemetryEnabled = val; preferences.putBool("telemEn", appConfig.telemetryEnabled); }
    }
    if (server.hasArg("telemUrl")) {
      String val = server.arg("telemUrl");
      if (val != appConfig.telemetryEndpoint) { appConfig.telemetryEndpoint = val; preferences.putString("telemUrl", appConfig.telemetryEndpoint.c_str()); }
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

inline void handleSystemLogs() {
  server.send(200, "application/json", "{\"logs\":[]}");
}

inline void handleSystemLogsClear() {
  server.send(200, "text/plain", "Logs Cleared");
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

    // Trigger the behaviour directly and also publish to MQTT debug channel
    // for external tool visibility when connected.
    if (appState.mqttConnected) {
      String cmd = "TRIGGER " + String(eventType) + " " + String(forceMode);
      if (detail.length() > 0) {
        cmd += " " + detail;
      }
      enqueueMqttPublish(MQTT_DEBUG_CMD_TOPIC, cmd);
      Logger::log("WEB", "Debug trigger via MQTT: %s", cmd.c_str());
    }
#if DESKBUDDY_DEBUG
    // In dev mode, the MQTT round-trip already calls triggerBehaviour.
    // In release, we call it directly since handleDebugCommand is stripped.
#else
    extern void triggerBehaviour(int eventType, String detail, int forceMode);
    triggerBehaviour(eventType, detail, forceMode);
#endif

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
  DynamicJsonDocument doc(8192);
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

static const char FILE_MANAGER_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
 <link rel="icon" href="/favicon.ico">
 <meta name="viewport" content="width=device-width, initial-scale=1">
 <title>DeskBuddy File Manager</title>
 <link rel="stylesheet" href="/web/style.css">
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

inline void handleFileManager() {
  server.send_P(200, "text/html; charset=utf-8", FILE_MANAGER_HTML);
}



inline void handleStats() {
  DynamicJsonDocument doc(512);
  doc["fsReadCount"] = appStats.fsReadCount;
  doc["fsWriteCount"] = appStats.fsWriteCount;
  doc["freeHeap"] = ESP.getFreeHeap() / 1024;
  doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  doc["version"] = DESKBUDDY_VERSION;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
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
  server.on("/api/points", HTTP_GET, handleGetPoints);
  server.on("/api/timer", HTTP_ANY, handleTimerApi);
  server.on("/api/odometer", HTTP_ANY, handleSetOdometer);
  server.on("/api/odometer_labels", HTTP_ANY, handleSetOdometerLabels);
  server.on("/settings", handleSettings);
  server.on("/radar-data", handleRadarData);
  server.on("/api/tft-messages", handleTftMessages);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/credentials", HTTP_GET, handleCredentials);
  server.on("/save-credentials", HTTP_POST, handleSaveCredentials);
  server.on("/wifi-scan", HTTP_GET, handleWifiScan);
  server.on("/reset-stats", handleResetStats);
  server.on("/factory-reset", handleFactoryReset);
  server.on("/trigger-event", handleTriggerEvent);
  server.on("/api/logs", HTTP_GET, handleSystemLogs);
  server.on("/api/stats", HTTP_GET, handleStats);
  server.on("/api/logs/clear", HTTP_POST, handleSystemLogsClear);
  server.on("/reset-esp", []() {
    server.send(200, "text/plain", "Rebooting");
    delay(500);
    LittleFS.end();
    ESP.restart();
  });
#if DESKBUDDY_DEBUG
  server.on("/files", HTTP_GET, handleFilesList);
  server.on("/file-manager", HTTP_GET, handleFileManager);
  server.on("/download", HTTP_GET, handleDownloadFile);
  server.on("/delete-file", HTTP_GET, handleDeleteFile);
  server.on("/upload", HTTP_POST, handleUploadResponse, handleFileUpload);
#endif

  server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");
  server.on("/web/style.css", []() {
  server.send_P(200, "text/css", WEB_STYLE_CSS);
});

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
