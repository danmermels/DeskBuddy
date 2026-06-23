#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ld2410.h>
#include <SimpleKalmanFilter.h>
#include <Preferences.h>
#include "Behaviour.h"
#include "AwayImage.h"
#include "../Credentials.h"

// User States
#define STATE_AWAY      0
#define STATE_STILL     1
#define STATE_ACTIVE    2
#define STATE_RESTLESS  3

// Distance threshold for presence at the desk (in cm) and attention state thresholds
int deskDistanceLimit = 120;
float activeThreshold = 15.0;
float restlessThreshold = 80.0;

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
ld2410 radar;
WebServer server(80);

// Kalman Filters for smoothing radar signals
// Parameters: SimpleKalmanFilter(measurement_uncertainty, estimation_uncertainty, process_noise)
SimpleKalmanFilter movingDistFilter(5.0, 5.0, 0.05);
SimpleKalmanFilter movingEnergyFilter(5.0, 5.0, 0.05);
SimpleKalmanFilter staticDistFilter(5.0, 5.0, 0.05);
SimpleKalmanFilter staticEnergyFilter(5.0, 5.0, 0.05);

// Filtered values
float filteredMovingDist = 0.0;
float filteredMovingEnergy = 0.0;
float filteredStaticDist = 0.0;
float filteredStaticEnergy = 0.0;

// Productivity & Timing Metrics
unsigned long totalDeskTime = 0;
unsigned long totalFocusTime = 0;
unsigned long totalBreakTime = 0;
int breakCount = 0;
int productivityScore = 0;
unsigned long latestBreakDuration = 0;

int currentPresenceState = STATE_AWAY;
unsigned long lastStateTransitionTime = 0;
unsigned long lastLoopTime = 0;
unsigned long continuousPresenceStart = 0;
unsigned long continuousStillStart = 0;
unsigned long lastStretchReminderTime = 0;

// Asynchronous Gemini AI Variables
volatile bool isAILoading = false;
String aiResponse = "";
volatile bool hasNewAIResponse = false;
String currentPrompt = "";
SemaphoreHandle_t geminiMutex = NULL;
volatile bool otaInProgress = false;

// Persistent Preferences & Settings
Preferences preferences;
float targetHours = 8.0;
int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
int dailyAiRequestCount = 0;
bool firstSitToday = true;
int lastNtpDay = -1;
int lastTriggeredEventType = EVENT_FIRST_SIT;

// Animated Ring Colors & Parameters
struct RGBColor {
  uint8_t r, g, b;
  bool operator==(const RGBColor& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGBColor& o) const { return !(*this == o); }
};

const RGBColor stateColors[] = {
  {80, 80, 80},     // STATE_AWAY: Dark Grey
  {0, 120, 255},    // STATE_STILL: Deep Blue
  {0, 220, 80},     // STATE_ACTIVE: Forest Green
  {255, 50, 50}     // STATE_RESTLESS: Soft Red
};

RGBColor currentRingColor = {80, 80, 80};
RGBColor startRingColor = {80, 80, 80};
RGBColor targetRingColor = {80, 80, 80};
unsigned long ringTransitionStart = 0;
const unsigned long ringTransitionDuration = 1000; // 1 second

// NTP Client & Weather Data
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned long refreshTime = 0;
unsigned long refreshWeather = 0;
int temp = 0;
String weatherDesc = "Clear";
struct tm ts;
char buf[80];

// Static IP Configuration
IPAddress local_IP(192, 168, 15, 160);  // Static IP for DeskBuddy
IPAddress gateway(192, 168, 15, 1);     // Gateway
IPAddress subnet(255, 255, 255, 0);     // Subnet Mask
IPAddress primaryDNS(1, 1, 1, 1);       // Primary DNS
IPAddress secondaryDNS(8, 8, 8, 8);     // Secondary DNS

// UI Pages
int uiPage = 0;
unsigned long aiScreenEndTime = 0;

// Formatting helper for durations
String formatTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;
  if (hours > 0) {
    return String(hours) + "h" + String(minutes) + "m";
  }
  return String(minutes) + "m";
}

// Converts state ID to string representation
const char* getPresenceStateName(int state) {
  switch (state) {
    case STATE_AWAY:      return "Away";
    case STATE_STILL:     return "Still (Focus)";
    case STATE_ACTIVE:    return "Active (Working)";
    case STATE_RESTLESS:  return "Restless";
    default:              return "Unknown";
  }
}

// Helper to draw auto-wrapped text in the center of the round TFT
void drawCenteredWrappedText(String text, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); // Middle-Center align text
  
  int y = 70;
  String line = "";
  int startIdx = 0;
  
  // Wrap string into lines of max 16 characters
  while (startIdx < text.length()) {
    int endIdx = startIdx + 16;
    if (endIdx >= text.length()) {
      line = text.substring(startIdx);
      startIdx = text.length();
    } else {
      int spaceIdx = text.lastIndexOf(' ', endIdx);
      if (spaceIdx > startIdx) {
        line = text.substring(startIdx, spaceIdx);
        startIdx = spaceIdx + 1;
      } else {
        line = text.substring(startIdx, endIdx);
        startIdx = endIdx;
      }
    }
    tft.drawString(line, 120, y, 4); // Draw text centered using Font 4
    y += 30;
    if (y > 180) break; // Avoid vertical overflow
  }
}

// Asynchronous FreeRTOS Task for Gemini HTTPS Queries
void queryGeminiTask(void * parameter) {
  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  String prompt = currentPrompt;
  xSemaphoreGive(geminiMutex);

  bool success = false;
  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL certificate verification for local speed
  
  HTTPClient https;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + String(GeminiApiKey);
  
  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");
    
    // Build JSON request payload
    DynamicJsonDocument reqDoc(1024);
    reqDoc["contents"][0]["parts"][0]["text"] = prompt;
    String payload;
    serializeJson(reqDoc, payload);
    
    int httpCode = https.POST(payload);
    if (httpCode == 200) {
      String response = https.getString();
      DynamicJsonDocument respDoc(2048);
      DeserializationError error = deserializeJson(respDoc, response);
      if (!error) {
        String generatedText = respDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        generatedText.trim();
        
        // Remove enclosing quotes if generated by LLM
        if (generatedText.startsWith("\"") && generatedText.endsWith("\"")) {
          generatedText = generatedText.substring(1, generatedText.length() - 1);
        }
        
        xSemaphoreTake(geminiMutex, portMAX_DELAY);
        aiResponse = generatedText;
        hasNewAIResponse = true;
        xSemaphoreGive(geminiMutex);
        success = true;
      }
    }
    https.end();
  }

  // Graceful fallback: If Gemini query fails, load a local fallback quote immediately
  if (!success) {
    const char* quote = "";
    int randIdx = random(20);
    switch (lastTriggeredEventType) {
      case EVENT_FIRST_SIT:     quote = localFirstSit[randIdx]; break;
      case EVENT_WELCOME_BACK:  quote = localWelcomeBack[randIdx]; break;
      case EVENT_STRETCH:       quote = localStretch[randIdx]; break;
      case EVENT_FOCUS_END:     quote = localFocus[randIdx]; break;
      case EVENT_SLACKER:       quote = localSlacker[randIdx]; break;
      default:                  quote = localWelcomeBack[randIdx]; break;
    }
    
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    aiResponse = String(quote);
    hasNewAIResponse = true;
    xSemaphoreGive(geminiMutex);
  }
  
  isAILoading = false;
  vTaskDelete(NULL); // One-shot task deletion
}

// Coordinated behaviour trigger: runs background Gemini task or picks local fallback
void triggerBehaviour(int eventType, String detail = "") {
  lastTriggeredEventType = eventType;
  bool useAI = false;
  if (aiMode == 2) {
    // Frequent mode: all events can trigger AI
    useAI = true;
  } else if (aiMode == 1) {
    // Balanced mode: AI triggers for FIRST_SIT, STRETCH, and WELCOME_BACK
    if (eventType == EVENT_FIRST_SIT || eventType == EVENT_STRETCH || eventType == EVENT_WELCOME_BACK) {
      useAI = true;
    }
  }

  // Enforce daily cap (max 15 requests per day)
  if (useAI && dailyAiRequestCount >= 15) {
    useAI = false;
  }

  if (useAI) {
    String basePrompt = "";
    char formattedPrompt[256];
    switch (eventType) {
      case EVENT_FIRST_SIT:
        basePrompt = PROMPT_FIRST_SIT_OF_DAY;
        break;
      case EVENT_WELCOME_BACK:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_WELCOME_BACK, detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_STRETCH:
        basePrompt = PROMPT_STRETCH_REMINDER;
        break;
      case EVENT_FOCUS_END:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_FOCUS_CONGRATS, detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_SLACKER:
        basePrompt = PROMPT_SLACKER_ROAST;
        break;
    }

    if (!isAILoading) {
      dailyAiRequestCount++;
      // Format details including Productivity Score & history
      String fullPrompt = basePrompt + "\nContext details:\n";
      fullPrompt += "At Desk Time: " + formatTime(totalDeskTime) + "\n";
      fullPrompt += "Focus Time: " + formatTime(totalFocusTime) + "\n";
      fullPrompt += "Break Time: " + formatTime(totalBreakTime) + "\n";
      fullPrompt += "Breaks taken: " + String(breakCount) + "\n";
      fullPrompt += "Productivity Score: " + String(productivityScore) + "%\n";
      fullPrompt += "Instruction: Respond with one short, witty sentence in English or Portuguese under 30 characters.";

      xSemaphoreTake(geminiMutex, portMAX_DELAY);
      currentPrompt = fullPrompt;
      xSemaphoreGive(geminiMutex);
      
      isAILoading = true;
      xTaskCreate(
        queryGeminiTask,
        "GeminiQuery",
        8192,
        NULL,
        1,
        NULL
      );
    }
  } else {
    // Local Fallback selection (picks from 20 available per event type)
    const char* quote = "";
    int randIdx = random(20);
    switch (eventType) {
      case EVENT_FIRST_SIT:     quote = localFirstSit[randIdx]; break;
      case EVENT_WELCOME_BACK:  quote = localWelcomeBack[randIdx]; break;
      case EVENT_STRETCH:       quote = localStretch[randIdx]; break;
      case EVENT_FOCUS_END:     quote = localFocus[randIdx]; break;
      case EVENT_SLACKER:       quote = localSlacker[randIdx]; break;
    }

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    aiResponse = String(quote);
    hasNewAIResponse = true;
    xSemaphoreGive(geminiMutex);
  }
}

// Web Server Route Handlers
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DeskBuddy Radar Dashboard</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; margin: 10px; width: 100%; max-width: 450px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #334155; }
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
      margin: 15px 0;
      min-height: 2.5rem;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .ai-loading-container {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      color: #fbbf24;
      font-size: 0.85rem;
      margin-top: 10px;
      font-weight: bold;
    }
    .spinner {
      width: 14px;
      height: 14px;
      border: 2px solid rgba(251, 191, 36, 0.2);
      border-top-color: #fbbf24;
      border-radius: 50%;
      animation: spin 1s linear infinite;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
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
  </style>
</head>
<body>
  <div class="card ai-card">
    <h1>DeskBuddy's Message</h1>
    <div class="ai-message" id="aiMessage">Waiting for activity...</div>
    <div class="ai-loading-container" id="aiLoading" style="display: none;">
      <div class="spinner"></div>
      <span>DeskBuddy is thinking...</span>
    </div>
  </div>

  <div class="card" style="width: 100%; max-width: 450px;">
    <h1>DeskBuddy Settings</h1>
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
        <span class="label">Daily Target Hours</span>
        <input type="number" step="0.1" min="0.1" max="24.0" name="targetHours" id="targetHoursInput" class="settings-input">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Active State Threshold (Energy)</span>
          <span class="value" id="actThreshVal">15%</span>
        </div>
        <input type="range" name="actThresh" id="actThreshSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('actThreshVal').innerText = this.value + '%'">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Restless State Threshold (Energy)</span>
          <span class="value" id="restThreshVal">80%</span>
        </div>
        <input type="range" name="restThresh" id="restThreshSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('restThreshVal').innerText = this.value + '%'">
      </div>
      <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
        <div style="display: flex; justify-content: space-between;">
          <span class="label">Desk Distance Limit (cm)</span>
          <span class="value" id="distLimitVal">120 cm</span>
        </div>
        <input type="range" name="distLimit" id="distLimitSlider" min="50" max="300" step="5" class="slider" oninput="document.getElementById('distLimitVal').innerText = this.value + ' cm'">
      </div>
      <div style="text-align: center; margin-top: 15px;">
        <button type="submit" class="btn">Save Configuration</button>
      </div>
    </form>
  </div>

  <div class="card" style="width: 100%; max-width: 450px;">
    <h1>Test Behavior Triggers</h1>
    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; justify-items: center; margin-top: 10px;">
      <button class="btn" style="width: 100%; font-size: 0.85rem; padding: 8px 12px;" onclick="triggerTest(0)">First Sit</button>
      <button class="btn" style="width: 100%; font-size: 0.85rem; padding: 8px 12px;" onclick="triggerTest(1)">Welcome Back</button>
      <button class="btn" style="width: 100%; font-size: 0.85rem; padding: 8px 12px;" onclick="triggerTest(2)">Stretch</button>
      <button class="btn" style="width: 100%; font-size: 0.85rem; padding: 8px 12px;" onclick="triggerTest(3)">Focus End</button>
      <button class="btn" style="grid-column: span 2; width: 100%; font-size: 0.85rem; padding: 8px 12px;" onclick="triggerTest(4)">Slacker Roast</button>
    </div>
  </div>

  <div class="card">
    <h1>DeskBuddy Live Radar</h1>
    <div class="metric">
      <span class="label">Presence Status</span>
      <span class="value" id="presence"><span class="badge badge-away">AWAY</span></span>
    </div>
    <div class="metric">
      <span class="label">User State</span>
      <span class="value" id="state">-</span>
    </div>
    <div class="metric">
      <span class="label">Moving Distance</span>
      <span class="value" id="movingDist">-</span>
    </div>
    <div class="metric">
      <span class="label">Moving Energy</span>
      <span class="value" id="movingEnergy">-</span>
    </div>
    <div class="metric">
      <span class="label">Stationary Distance</span>
      <span class="value" id="staticDist">-</span>
    </div>
    <div class="metric">
      <span class="label">Stationary Energy</span>
      <span class="value" id="staticEnergy">-</span>
    </div>
  </div>
  
  <div class="card">
    <h1>Productivity History</h1>
    <div class="metric">
      <span class="label">Total Desk Time</span>
      <span class="value" id="deskTime">-</span>
    </div>
    <div class="metric">
      <span class="label">Total Focus Time</span>
      <span class="value" id="focusTime">-</span>
    </div>
    <div class="metric">
      <span class="label">Total Break Time</span>
      <span class="value" id="breakTime">-</span>
    </div>
    <div class="metric">
      <span class="label">Break Count</span>
      <span class="value" id="breaks">-</span>
    </div>
    <div class="metric">
      <span class="label">Productivity Score</span>
      <span class="value" id="score">-</span>
    </div>
  </div>

  <script>
    function updateMetrics() {
      fetch('/radar-data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('presence').innerHTML = data.presence 
            ? '<span class="badge badge-present">PRESENT</span>' 
            : '<span class="badge badge-away">AWAY</span>';
          document.getElementById('state').innerText = data.state;
          document.getElementById('movingDist').innerText = data.movingDist + " cm";
          document.getElementById('movingEnergy').innerText = data.movingEnergy + " %";
          document.getElementById('staticDist').innerText = data.staticDist + " cm";
          document.getElementById('staticEnergy').innerText = data.staticEnergy + " %";
          
          document.getElementById('deskTime').innerText = data.deskTime;
          document.getElementById('focusTime').innerText = data.focusTime;
          document.getElementById('breakTime').innerText = data.breakTime;
          document.getElementById('breaks').innerText = data.breaks;
          
          let scoreColor = "score-high";
          if (data.score < 40) scoreColor = "score-low";
          else if (data.score < 70) scoreColor = "score-med";
          document.getElementById('score').innerHTML = `<span class="${scoreColor}">${data.score}%</span>`;
          
          // Update AI message & loading status
          if (data.aiMessage && data.aiMessage.trim() !== "") {
            document.getElementById('aiMessage').innerText = data.aiMessage;
          } else {
            document.getElementById('aiMessage').innerText = "Waiting for activity...";
          }
          document.getElementById('aiLoading').style.display = data.aiLoading ? "flex" : "none";
          
          // Populate settings fields once on load
          if (!window.settingsPopulated) {
            document.getElementById('aiModeSelect').value = data.aiMode;
            document.getElementById('targetHoursInput').value = data.targetHours;
            
            document.getElementById('actThreshSlider').value = data.actThresh;
            document.getElementById('actThreshVal').innerText = data.actThresh + '%';
            
            document.getElementById('restThreshSlider').value = data.restThresh;
            document.getElementById('restThreshVal').innerText = data.restThresh + '%';
            
            document.getElementById('distLimitSlider').value = data.distLimit;
            document.getElementById('distLimitVal').innerText = data.distLimit + ' cm';
            
            window.settingsPopulated = true;
          }
        })
        .catch(err => console.error("Error fetching radar data:", err));
    }
    
    updateMetrics();
    setInterval(updateMetrics, 1000);

    function triggerTest(eventType) {
      fetch('/trigger-test?event=' + eventType)
        .then(response => {
          if (!response.ok) {
            alert('Trigger failed.');
          }
        });
    }
  </script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

void handleRadarData() {
  DynamicJsonDocument doc(512);
  doc["presence"] = (currentPresenceState != STATE_AWAY);
  doc["state"] = getPresenceStateName(currentPresenceState);
  
  if (radar.isConnected()) {
    doc["movingDist"] = (int)filteredMovingDist;
    doc["movingEnergy"] = (int)filteredMovingEnergy;
    doc["staticDist"] = (int)filteredStaticDist;
    doc["staticEnergy"] = (int)filteredStaticEnergy;
  } else {
    doc["movingDist"] = 0;
    doc["movingEnergy"] = 0;
    doc["staticDist"] = 0;
    doc["staticEnergy"] = 0;
  }
  
  doc["deskTime"] = formatTime(totalDeskTime);
  doc["focusTime"] = formatTime(totalFocusTime);
  doc["breakTime"] = formatTime(totalBreakTime);
  doc["breaks"] = breakCount;
  doc["score"] = productivityScore;
  doc["aiMode"] = aiMode;
  doc["targetHours"] = targetHours;
  doc["actThresh"] = activeThreshold;
  doc["restThresh"] = restlessThreshold;
  doc["distLimit"] = deskDistanceLimit;
  
  // Add AI response thread-safely
  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  doc["aiMessage"] = aiResponse;
  xSemaphoreGive(geminiMutex);
  doc["aiLoading"] = isAILoading;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSaveSettings() {
  if (server.hasArg("aiMode") && server.hasArg("targetHours")) {
    aiMode = server.arg("aiMode").toInt();
    targetHours = server.arg("targetHours").toFloat();
    
    if (server.hasArg("actThresh")) activeThreshold = server.arg("actThresh").toFloat();
    if (server.hasArg("restThresh")) restlessThreshold = server.arg("restThresh").toFloat();
    if (server.hasArg("distLimit")) deskDistanceLimit = server.arg("distLimit").toInt();
    
    preferences.begin("deskbuddy", false);
    preferences.putInt("aiMode", aiMode);
    preferences.putFloat("targetHours", targetHours);
    preferences.putFloat("actThresh", activeThreshold);
    preferences.putFloat("restThresh", restlessThreshold);
    preferences.putInt("distLimit", deskDistanceLimit);
    preferences.end();
    
    // Dynamically adjust physical radar gates according to new range limit
    if (radar.isConnected()) {
      int requiredGates = (deskDistanceLimit + 74) / 75;
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

void handleTriggerTest() {
  if (server.hasArg("event")) {
    int eventType = server.arg("event").toInt();
    if (eventType == EVENT_WELCOME_BACK) {
      triggerBehaviour(eventType, "15m");
    } else if (eventType == EVENT_FOCUS_END) {
      triggerBehaviour(eventType, "45m");
    } else {
      triggerBehaviour(eventType);
    }
    server.send(200, "text/plain", "Triggered");
  } else {
    server.send(400, "text/plain", "Missing event parameter");
  }
}

// Asynchronous WiFi Reconnection Checker
void checkWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      // Serial.println("WiFi disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
      WiFi.begin(SSID, PASS);
    }
  }
}

void updateTFTDisplay(unsigned long now) {
  static unsigned long lastTFTUpdate = 0;
  static int lastDisplayedPage = -1;
  static int lastDisplayedState = -1;
  static int lastDisplayedTimeMin = -1;
  static bool forceRingRedraw = false;

  // Handle bezel ring animation transition
  RGBColor targetColor = stateColors[currentPresenceState];
  if (targetColor != targetRingColor) {
    startRingColor = currentRingColor;
    targetRingColor = targetColor;
    ringTransitionStart = now;
  }

  static unsigned long lastRingUpdate = 0;
  bool isTransitioning = (currentRingColor != targetRingColor);
  if (isTransitioning || forceRingRedraw || (now - lastRingUpdate > 50)) {
    if (isTransitioning) {
      unsigned long elapsed = now - ringTransitionStart;
      if (elapsed >= ringTransitionDuration) {
        currentRingColor = targetRingColor;
      } else {
        float t = (float)elapsed / ringTransitionDuration;
        t = (1.0f - cosf(t * 3.14159265f)) / 2.0f; // Cosine ease-in-out
        currentRingColor.r = startRingColor.r + t * (targetRingColor.r - startRingColor.r);
        currentRingColor.g = startRingColor.g + t * (targetRingColor.g - startRingColor.g);
        currentRingColor.b = startRingColor.b + t * (targetRingColor.b - startRingColor.b);
      }
    }
    
    // Draw 3px thick bezel ring
    uint16_t color565 = tft.color565(currentRingColor.r, currentRingColor.g, currentRingColor.b);
    tft.drawCircle(120, 120, 118, color565);
    tft.drawCircle(120, 120, 117, color565);
    tft.drawCircle(120, 120, 116, color565);
    lastRingUpdate = now;
    forceRingRedraw = false;
  }

  // 1. Manage AI Response alert screen
  if (now < aiScreenEndTime) {
    // We are currently displaying the Gemini AI Screen
    // Return early to prevent overwriting it
    return;
  }

  // Check if we have a new AI response to display
  if (hasNewAIResponse) {
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    String responseCopy = aiResponse;
    hasNewAIResponse = false;
    xSemaphoreGive(geminiMutex);

    // Enter AI screen mode for 8 seconds
    aiScreenEndTime = now + 8000;
    drawCenteredWrappedText(responseCopy, TFT_SKYBLUE);
    lastDisplayedPage = -2; // Reset page state to force redraw when AI screen finishes
    forceRingRedraw = true;
    return;
  }

  // 2. Refresh control: only update screen every 500ms
  if (now - lastTFTUpdate < 500) {
    return;
  }
  lastTFTUpdate = now;

  // If user is AWAY
  if (currentPresenceState == STATE_AWAY) {
    if (lastDisplayedState != STATE_AWAY) {
      tft.fillScreen(TFT_BLACK);
      tft.setSwapBytes(true);
      tft.pushImage(0, 0, 240, 240, away_img_data);

      lastDisplayedState = STATE_AWAY;
      lastDisplayedPage = -1;
      forceRingRedraw = true;
    }
    return;
  }

  // If user is PRESENT, draw clock face
  lastDisplayedState = currentPresenceState;

  // Clear screen if we just transitioned from Away or AI screen
  if (lastDisplayedPage != 0) {
    tft.fillScreen(TFT_BLACK);
    lastDisplayedPage = 0;
    forceRingRedraw = true;
  }

  tft.setTextDatum(MC_DATUM);

  // Weather section (top)
  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  tft.drawString(String(temp) + "C | " + weatherDesc, 120, 50, 4);

  // Time section (center)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String timeStr = timeClient.getFormattedTime().substring(0, 5);
  tft.drawString(timeStr, 120, 105, 7); // Large digital font

  // Date section (below time)
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(buf, 120, 150, 2);

  // Cycle through metrics at the bottom (Y=190) every 15 seconds
  static unsigned long lastMetricSwitch = 0;
  static int metricIndex = 0;
  
  if (now - lastMetricSwitch > 15000) {
    metricIndex = (metricIndex + 1) % 6;
    lastMetricSwitch = now;
    tft.fillRect(15, 175, 210, 32, TFT_BLACK); // Clear text area
  }

  String metricText = "";
  uint16_t metricColor = TFT_WHITE;

  switch (metricIndex) {
    case 0: {
      int pct = 0;
      if (targetHours > 0.0f) {
        pct = (int)((totalDeskTime * 100.0f) / (targetHours * 3600.0f * 1000.0f));
      }
      if (pct > 100) pct = 100;
      metricText = "Day Done: " + String(pct) + "%";
      metricColor = tft.color565(251, 191, 36); // Vibrant amber yellow
      break;
    }
    case 1:
      metricText = "Score: " + String(productivityScore) + "%";
      if (productivityScore >= 70) metricColor = TFT_GREEN;
      else if (productivityScore >= 40) metricColor = TFT_YELLOW;
      else metricColor = TFT_RED;
      break;
    case 2:
      metricText = "Sitting: " + formatTime(now - continuousPresenceStart);
      metricColor = TFT_LIGHTGREY;
      break;
    case 3:
      metricText = "Last Break: " + formatTime(latestBreakDuration);
      metricColor = TFT_LIGHTGREY;
      break;
    case 4:
      metricText = "Breaks: " + String(breakCount);
      metricColor = TFT_LIGHTGREY;
      break;
    case 5:
      metricText = "Focus: " + formatTime(totalFocusTime);
      metricColor = TFT_SKYBLUE;
      break;
  }

  tft.setTextColor(metricColor, TFT_BLACK);
  tft.drawString(metricText, 120, 190, 4);
}

void setup(void) {
  Serial.begin(115200);
  delay(2000);
  Serial.flush();

  // Load persistent configurations
  preferences.begin("deskbuddy", false);
  aiMode = preferences.getInt("aiMode", 1);
  targetHours = preferences.getFloat("targetHours", 8.0);
  activeThreshold = preferences.getFloat("actThresh", 15.0);
  restlessThreshold = preferences.getFloat("restThresh", 80.0);
  deskDistanceLimit = preferences.getInt("distLimit", 120);
  preferences.end();

  // Initialize TFT Display & show splash screen
  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 240, 240, away_img_data);
  unsigned long bootStartTime = millis();

  // Initialize Serial1 for Radar on Pins 0 (RX) and 5 (TX)
  Serial1.begin(256000, SERIAL_8N1, 0, 5); 
  delay(500);
  // Serial.print("LD2410 radar sensor initialising: ");
  if (radar.begin(Serial1)) {
    // Serial.println("OK");
    // Serial.print("Configuring radar sensor (max range 3.0m, 5s timeout): ");
    int requiredGates = (deskDistanceLimit + 74) / 75;
    if (requiredGates < 2) requiredGates = 2;
    if (requiredGates > 8) requiredGates = 8;
    if (radar.setMaxValues(requiredGates, requiredGates, 5)) {
      // Serial.println("SUCCESS");
    } else {
      // Serial.println("FAIL");
    }
  } else {
    // Serial.println("not connected");
  }

  // Set Hostname & Configure static IP
  WiFi.setHostname("DeskBuddy");
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(SSID, PASS);

  // Serial.println("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    // Serial.print(".");
  }
  // Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

  // Setup NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/radar-data", handleRadarData);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/trigger-test", handleTriggerTest);
  server.begin();
  // Serial.println("Web Server started on Port 80.");

  // Setup Mutex for Gemini Thread Safety
  geminiMutex = xSemaphoreCreateMutex();

  // Setup OTA Updates
  ArduinoOTA
    .onStart([]() {
      otaInProgress = true;
      // Serial.println("OTA Update started...");
    })
    .onEnd([]() {
      // Serial.println("\nOTA Update finished successfully! Rebooting...");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      // Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      // Serial.printf("Error[%u]\n", error);
    });
  ArduinoOTA.begin();
  // Serial.println("OTA enabled.");

  lastLoopTime = millis();
  lastStateTransitionTime = millis();
  
  // Force NTP and Weather update on the very first loop execution
  refreshTime = millis() - 3600000 - 1000;
  refreshWeather = millis() - 3600000 - 1000;
  
  // Trigger initial test query to verify Gemini connection
  // Serial.println("Triggering test query to Gemini AI...");
  // triggerBehaviour(EVENT_FIRST_SIT);

  // Ensure splash screen displays for at least 4 seconds total at boot
  unsigned long elapsedBoot = millis() - bootStartTime;
  if (elapsedBoot < 4000) {
    delay(4000 - elapsedBoot);
  }
}

void loop(void) {
  // Poll critical background systems
  ArduinoOTA.handle();
  if (otaInProgress) {
    delay(50);
    return;
  }
  server.handleClient();
  checkWiFiConnection();

  unsigned long now = millis();
  unsigned long elapsed = now - lastLoopTime;
  lastLoopTime = now;

  // Midnight Reset Check
  if (WiFi.status() == WL_CONNECTED && timeClient.isTimeSet()) {
    int currentDay = timeClient.getDay();
    if (lastNtpDay == -1) {
      lastNtpDay = currentDay;
    } else if (currentDay != lastNtpDay) {
      lastNtpDay = currentDay;
      firstSitToday = true;
      breakCount = 0;
      totalDeskTime = 0;
      totalFocusTime = 0;
      totalBreakTime = 0;
      dailyAiRequestCount = 0;
    }
  }

  static bool simulationOverride = false;
  static bool simPresent = false;
  static int simState = STATE_AWAY;

  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'p') {
      // Serial.println("\n[SIMULATION] Override: Force PRESENT.");
      simulationOverride = true;
      simPresent = true;
      simState = STATE_ACTIVE;
    } else if (cmd == 'a') {
      // Serial.println("\n[SIMULATION] Override: Force AWAY.");
      simulationOverride = true;
      simPresent = false;
      simState = STATE_AWAY;
    } else if (cmd == 'r') {
      // Serial.println("\n[SIMULATION] Override CLEARED. Resuming Radar mode.");
      simulationOverride = false;
    }
  }

  static bool stablePresence = false;
  static unsigned long lastPresenceChangeTime = 0;

  bool rawPresent = false;
  int rawState = STATE_AWAY;

  if (simulationOverride) {
    rawPresent = simPresent;
    rawState = simState;
  } else {
    // Read from the physical radar sensor
    radar.read();
    if (radar.isConnected()) {
      // 1. Process Moving Target Filter
      if (radar.movingTargetDetected()) {
        filteredMovingDist = movingDistFilter.updateEstimate((float)radar.movingTargetDistance());
        filteredMovingEnergy = movingEnergyFilter.updateEstimate((float)radar.movingTargetEnergy());
      } else {
        filteredMovingDist = 0.0;
        filteredMovingEnergy = 0.0;
      }

      // 2. Process Stationary Target Filter
      if (radar.stationaryTargetDetected()) {
        filteredStaticDist = staticDistFilter.updateEstimate((float)radar.stationaryTargetDistance());
        filteredStaticEnergy = staticEnergyFilter.updateEstimate((float)radar.stationaryTargetEnergy());
      } else {
        filteredStaticDist = 0.0;
        filteredStaticEnergy = 0.0;
      }

      // 3. Process Presence & State Logic using filtered values
      if (radar.presenceDetected()) {
        bool nearMoving = false;
        bool nearStatic = false;
        
        if (filteredMovingDist > 0.0 && filteredMovingDist <= deskDistanceLimit) {
          nearMoving = true;
        }
        if (filteredStaticDist > 0.0 && filteredStaticDist <= deskDistanceLimit) {
          nearStatic = true;
        }
        
        if (nearMoving || nearStatic) {
          rawPresent = true;
          if (nearMoving && filteredMovingEnergy > restlessThreshold) {
            rawState = STATE_RESTLESS; // Heavy movement without holding a stable body presence (stretching/fidgeting)
          } else if (nearMoving && filteredMovingEnergy > activeThreshold) {
            rawState = STATE_ACTIVE;   // Normal desk movements (typing/mouse work)
          } else {
            rawState = STATE_STILL;    // Very quiet body presence, deep focus
          }
        }
      }
    }
  }

  // Debouncing logic to filter sensor instability/boundary jitter
  if (rawPresent != stablePresence) {
    unsigned long debounceLimit = rawPresent ? 2000 : 10000; // 2s to confirm presence, 10s to confirm away
    if (now - lastPresenceChangeTime > debounceLimit) {
      stablePresence = rawPresent;
      // Serial.printf("[RADAR] Presence stable transition to: %s\n", stablePresence ? "PRESENT" : "AWAY");
    }
  } else {
    lastPresenceChangeTime = now;
  }

  bool targetPresent = stablePresence;
  int targetState = stablePresence ? ((rawState != STATE_AWAY) ? rawState : STATE_ACTIVE) : STATE_AWAY;

  // High-frequency debug printout for sensor calibration (disabled for performance)
  /*
  static unsigned long lastCalibrationPrint = 0;
  if (now - lastCalibrationPrint > 100) { // Print every 100ms
    lastCalibrationPrint = now;
    if (radar.isConnected()) {
      Serial.printf("Presence:%d MovDist:%.1f MovEnergy:%.1f StaDist:%.1f StaEnergy:%.1f\n",
        radar.presenceDetected() ? 1 : 0,
        filteredMovingDist,
        filteredMovingEnergy,
        filteredStaticDist,
        filteredStaticEnergy
      );
    } else {
      Serial.println("Radar sensor not connected / reading...");
    }
  }
  */

  // Handle Presence State Machine Transitions
  if (targetPresent) {
    if (currentPresenceState == STATE_AWAY) {
      // Transition: Away -> Present
      unsigned long breakDuration = now - lastStateTransitionTime;
      breakCount++;
      latestBreakDuration = breakDuration;
      
      // Trigger First Sit or Welcome Back
      if (firstSitToday) {
        firstSitToday = false;
        triggerBehaviour(EVENT_FIRST_SIT);
      } else if (breakDuration > 10000) {
        triggerBehaviour(EVENT_WELCOME_BACK, formatTime(breakDuration));
      }
      
      currentPresenceState = targetState;
      lastStateTransitionTime = now;
      continuousPresenceStart = now;
      lastStretchReminderTime = now;
      if (targetState == STATE_STILL) {
        continuousStillStart = now;
      }
    } else {
      // Accumulate desk time
      totalDeskTime += elapsed;
      if (currentPresenceState == STATE_STILL) {
        totalFocusTime += elapsed;
      }
      
      // Keep state with debouncing for quiet focus state
      static unsigned long lastStateChangeAttempt = 0;
      static int pendingState = STATE_AWAY;

      if (targetState != currentPresenceState) {
        if (currentPresenceState == STATE_STILL) {
          // Currently in Still (Focus). Transitioning out to Active or Restless requires debounce.
          if (pendingState != targetState) {
            pendingState = targetState;
            lastStateChangeAttempt = now;
          } else if (now - lastStateChangeAttempt > 10000) { // Require 10 seconds of continuous Active/Restless
            currentPresenceState = targetState;
            lastStateTransitionTime = now;
          }
        } else {
          // Currently in Active or Restless. 
          if (targetState == STATE_STILL) {
            // Quieting down: require 30 seconds of continuous Still to enter focus
            if (pendingState != STATE_STILL) {
              pendingState = STATE_STILL;
              lastStateChangeAttempt = now;
            } else if (now - lastStateChangeAttempt > 30000) {
              currentPresenceState = STATE_STILL;
              lastStateTransitionTime = now;
              continuousStillStart = now;
            }
          } else {
            // Transitions between Active and Restless are instantaneous
            currentPresenceState = targetState;
            lastStateTransitionTime = now;
            pendingState = currentPresenceState;
          }
        }
      } else {
        pendingState = currentPresenceState;
      }
      
      // Trigger Stretch alert after 45 minutes of continuous presence
      if (now - lastStretchReminderTime > 2700000UL) {
        triggerBehaviour(EVENT_STRETCH);
        lastStretchReminderTime = now;
      }

      // Trigger Slacker Roast if sitting > 1 hour and score < 35%
      static unsigned long lastSlackerRoastTime = 0;
      unsigned long continuousSittingTime = now - continuousPresenceStart;
      if (continuousSittingTime > 3600000UL && productivityScore < 35) {
        if (now - lastSlackerRoastTime > 3600000UL) {
          triggerBehaviour(EVENT_SLACKER);
          lastSlackerRoastTime = now;
        }
      }
    }
  } else {
    if (currentPresenceState != STATE_AWAY) {
      // Transition: Present -> Away
      unsigned long focusSessionDuration = 0;
      if (currentPresenceState == STATE_STILL) {
        focusSessionDuration = now - continuousStillStart;
      }
      // Serial.println("User stood up.");
      
      // Trigger Focus session congrats if user focused for > 15s
      if (focusSessionDuration > 15000) {
        triggerBehaviour(EVENT_FOCUS_END, formatTime(focusSessionDuration));
      }
      
      currentPresenceState = STATE_AWAY;
      lastStateTransitionTime = now;
    } else {
      // Accumulate break time
      totalBreakTime += elapsed;
    }
  }

  // Update dynamic productivity score
  if (totalDeskTime > 0) {
    productivityScore = (totalFocusTime * 100) / totalDeskTime;
  } else {
    productivityScore = 0;
  }

  // Handle NTP Time Updates
  if (WiFi.status() == WL_CONNECTED && now - refreshTime > 3600000) {
    timeClient.update();
    refreshTime = now;
  }

  // Handle Weather Fetch Updates (every 1 hour)
  if (WiFi.status() == WL_CONNECTED && now - refreshWeather > 3600000) {
    HTTPClient http;
    http.begin(String(OpenWeatherCall) + OpenWeatherKey);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, payload);
      if (!error) {
        temp = (float)(jsonBuffer["main"]["temp"]);
        if (jsonBuffer["weather"].is<JsonArray>() && jsonBuffer["weather"].as<JsonArray>().size() > 0) {
          weatherDesc = jsonBuffer["weather"][0]["main"].as<String>();
        }
        time_t rawtime = jsonBuffer["dt"];
        rawtime = rawtime - 10800;
        ts = *localtime(&rawtime);
        strftime(buf, sizeof(buf), "%a %d-%m", &ts);
      }
    }
    http.end();
    refreshWeather = now;
  }

  // Update TFT Display
  updateTFTDisplay(now);

  // Periodic status printout (disabled for performance)
  /*
  static unsigned long lastStatusPrint = 0;
  if (now - lastStatusPrint > 5000) {
    lastStatusPrint = now;
    Serial.printf("Desk Time: %s | Focus Time: %s | Break Time: %s | Breaks: %d | Score: %d%% | State: %s\n",
      formatTime(totalDeskTime).c_str(),
      formatTime(totalFocusTime).c_str(),
      formatTime(totalBreakTime).c_str(),
      breakCount,
      productivityScore,
      getPresenceStateName(currentPresenceState)
    );
  }
  */

  delay(10);
}