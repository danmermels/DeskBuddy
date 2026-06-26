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
#include <Preferences.h>
#include <LittleFS.h>
#include "Behaviour.h"
// FuturaFont.h moved to Faceplates.h
#include "../Credentials.h"

// User States
#define STATE_AWAY        0
#define STATE_FOCUS       1
#define STATE_BUSY        2
#define STATE_DISTRACTED  3
#define STATE_REGULAR     4

// Configuration limits
int deskDistanceLimit = 120;
int focusDistanceLimit = 50;
int motionRatioLimit = 15;

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
ld2410 radar;
WebServer server(80);

#include <algorithm>

class RollingMedianFilter {
public:
  RollingMedianFilter(int maxSize = 100) {
    _maxSize = maxSize;
    _buffer = new float[_maxSize];
    clear();
  }

  ~RollingMedianFilter() {
    delete[] _buffer;
  }

  void clear() {
    _head = 0;
    _count = 0;
    for (int i = 0; i < _maxSize; i++) {
      _buffer[i] = 0.0;
    }
  }

  void add(float val) {
    _buffer[_head] = val;
    _head = (_head + 1) % _maxSize;
    if (_count < _maxSize) {
      _count++;
    }
  }

  float getMedian(int windowSize) {
    if (windowSize <= 0) return 0.0;
    if (windowSize > _maxSize) windowSize = _maxSize;
    
    int countToCopy = (_count < windowSize) ? _count : windowSize;
    if (countToCopy == 0) return 0.0;

    float* temp = new float[countToCopy];
    int idx = _head;
    for (int i = 0; i < countToCopy; i++) {
      idx = (idx - 1 + _maxSize) % _maxSize;
      temp[i] = _buffer[idx];
    }

    std::sort(temp, temp + countToCopy);
    
    float result;
    if (countToCopy % 2 == 1) {
      result = temp[countToCopy / 2];
    } else {
      result = (temp[countToCopy / 2 - 1] + temp[countToCopy / 2]) / 2.0;
    }
    delete[] temp;
    return result;
  }

private:
  float* _buffer;
  int _maxSize;
  int _head;
  int _count;
};

// Rolling Median Filter for smoothing detection distance signal
RollingMedianFilter detectionDistFilter(100);
// Rolling Median Filter for motion target debouncing to filter spikes
RollingMedianFilter motionFilter(10);

// Filtered values
float filteredDetectionDist = 0.0;

// Productivity & Timing Metrics
unsigned long totalDeskTime = 0;
unsigned long totalFocusTime = 0;
unsigned long totalBreakTime = 0;
int breakCount = 0;
int productivityScore = 0;
unsigned long latestBreakDuration = 0;
unsigned long overnightBreakDuration = 0;
uint32_t lastAwayEpoch = 0;

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
volatile bool lastResponseIsAi = false;
String currentPrompt = "";
String lastTriggeredEventDetail = "";
String currentUserName = "human";
SemaphoreHandle_t geminiMutex = NULL;
volatile bool otaInProgress = false;

// Persistent Preferences & Settings
Preferences preferences;
float targetHours = 8.0;
int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent
int clockFace = 0;
int dailyAiRequestCount = 0;
String userName = "human";
bool firstSitToday = true;
uint32_t firstSitEpoch = 0;
unsigned long longestSittingStreak = 0;
bool streakAlertTriggered = false;
int lastNtpDay = -1;
int lastTriggeredEventType = EVENT_FIRST_SIT;
float filterWindow = 2.0;

// Radar Gate Sensitivity Thresholds (0-100)
int g0mSens = 100;
int g0sSens = 50;
int g1mSens = 100;
int g1sSens = 50;
int g2mSens = 100;
int g2sSens = 50;
int g3mSens = 80;
int g3sSens = 50;
int g4mSens = 100;
int g4sSens = 50;
int g5mSens = 100;
int g5sSens = 50;
int g6mSens = 100;
int g6sSens = 50;

// Raw radar values
int rawDetectionDist = 0;
bool sensorPresenceDetected = false;
bool sensorMovingTargetDetected = false;
bool sensorStaticPresenceDetected = false;

// Session-specific and cumulative motion tracking
unsigned long sessionDeskTime = 0;
unsigned long sessionMotionTime = 0;
unsigned long totalMotionTime = 0;
int motionCount = 0;

// Session-specific distance tracking
unsigned long sessionDistanceSum = 0;
unsigned long sessionDistanceCount = 0;
float sessionDistanceAverage = 0.0;

// Animated Ring Colors & Parameters
struct RGBColor {
  uint8_t r, g, b;
  bool operator==(const RGBColor& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGBColor& o) const { return !(*this == o); }
};

const RGBColor stateColors[] = {
  {80, 80, 80},     // STATE_AWAY: Dark Grey
  {0, 120, 255},    // STATE_FOCUS: Deep Blue
  {0, 220, 80},     // STATE_BUSY: Forest Green
  {255, 50, 50},    // STATE_DISTRACTED: Soft Red
  {200, 200, 200}   // STATE_REGULAR: Soft White
};

RGBColor currentRingColor = {80, 80, 80};
RGBColor startRingColor = {80, 80, 80};
RGBColor targetRingColor = {80, 80, 80};
unsigned long ringTransitionStart = 0;
const unsigned long ringTransitionDuration = 1000; // 1 second

// NTP Client & Weather Data
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned long lastHourlyUpdate = 0;
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

// Formatting helper for epoch timestamp to HH:MM (local time already shifted offset)
String formatEpochTime(uint32_t epoch) {
  if (epoch == 0) return "Not registered yet";
  time_t rawTime = (time_t)epoch;
  struct tm * gmTimeInfo = gmtime(&rawTime);
  char timeStr[10];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", gmTimeInfo->tm_hour, gmTimeInfo->tm_min);
  return String(timeStr);
}

// Converts state ID to string representation
const char* getPresenceStateName(int state) {
  switch (state) {
    case STATE_AWAY:        return "Away";
    case STATE_FOCUS:       return "Focus";
    case STATE_BUSY:        return "Busy";
    case STATE_DISTRACTED:  return "Distracted";
    case STATE_REGULAR:     return "Regular Activity";
    default:                return "Unknown";
  }
}

// Draw custom PackBits-RLE compressed image from LittleFS to TFT
void drawRLEImage(const char* filename, int16_t x, int16_t y) {
  fs::File file = LittleFS.open(filename, "r");
  if (!file) return;

  uint16_t w, h;
  if (file.read((uint8_t*)&w, 2) != 2 || file.read((uint8_t*)&h, 2) != 2) {
    file.close();
    return;
  }

  tft.setAddrWindow(x, y, w, h);

  while (file.available() > 0) {
    uint8_t header = file.read();
    uint8_t count = (header & 0x7F) + 1;
    if (header & 0x80) {
      // Repeating run packet
      uint16_t color;
      if (file.read((uint8_t*)&color, 2) == 2) {
        tft.pushColor(color, count);
      }
    } else {
      // Raw non-repeating packet
      for (int i = 0; i < count; i++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, 2) == 2) {
          tft.pushColor(color, 1);
        }
      }
    }
  }
  file.close();
}

// Helper to draw auto-wrapped text in the center of the round TFT
void drawCenteredWrappedText(String text, uint16_t color, bool isAi = false) {
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

  if (isAi) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("(AI GENERATED)", 120, 210, 2);
  }
}


#include "Faceplates.h"

// Dynamic quote personalization helper
String personalizeQuote(String quote, String name) {
  char formattedQuote[128];
  snprintf(formattedQuote, sizeof(formattedQuote), quote.c_str(), name.c_str());
  return String(formattedQuote);
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
        lastResponseIsAi = true;
        if (lastTriggeredEventType == EVENT_WELCOME_BACK || lastTriggeredEventType == EVENT_FIRST_SIT || lastTriggeredEventType == EVENT_STREAK_BEATEN) {
          char welcomeMsg[128];
          snprintf(welcomeMsg, sizeof(welcomeMsg), "%s (%s)", generatedText.c_str(), lastTriggeredEventDetail.c_str());
          aiResponse = String(welcomeMsg);
        } else {
          aiResponse = generatedText;
        }
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
      case EVENT_STREAK_BEATEN: quote = localStreakBeaten[randIdx]; break;
      default:                  quote = localWelcomeBack[randIdx]; break;
    }
    
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    lastResponseIsAi = false;
    String nameCopy = currentUserName;
    xSemaphoreGive(geminiMutex);

    String personalQuote = personalizeQuote(String(quote), nameCopy);
    
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    if (lastTriggeredEventType == EVENT_WELCOME_BACK || lastTriggeredEventType == EVENT_FIRST_SIT || lastTriggeredEventType == EVENT_STREAK_BEATEN) {
      char welcomeMsg[128];
      snprintf(welcomeMsg, sizeof(welcomeMsg), "%s (%s)", personalQuote.c_str(), lastTriggeredEventDetail.c_str());
      aiResponse = String(welcomeMsg);
    } else {
      aiResponse = personalQuote;
    }
    hasNewAIResponse = true;
    xSemaphoreGive(geminiMutex);
  }
  
  isAILoading = false;
  vTaskDelete(NULL); // One-shot task deletion
}

// Coordinated behaviour trigger: runs background Gemini task or picks local fallback
void triggerBehaviour(int eventType, String detail = "") {
  lastTriggeredEventType = eventType;

  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  lastTriggeredEventDetail = detail;
  currentUserName = userName;
  xSemaphoreGive(geminiMutex);

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
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_FIRST_SIT_OF_DAY, userName.c_str(), detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_WELCOME_BACK:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_WELCOME_BACK, userName.c_str(), detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_STRETCH:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_STRETCH_REMINDER, userName.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_FOCUS_END:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_FOCUS_CONGRATS, userName.c_str(), detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_SLACKER:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_SLACKER_ROAST, userName.c_str());
        basePrompt = String(formattedPrompt);
        break;
      case EVENT_STREAK_BEATEN:
        snprintf(formattedPrompt, sizeof(formattedPrompt), PROMPT_STREAK_BEATEN, userName.c_str(), detail.c_str());
        basePrompt = String(formattedPrompt);
        break;
    }

    if (!isAILoading) {
      dailyAiRequestCount++;
      // Format details including Productivity Score & history
      String fullPrompt = basePrompt + "\nContext details:\n";
      fullPrompt += "User's Name: " + userName + "\n";
      fullPrompt += "At Desk Time: " + formatTime(totalDeskTime) + "\n";
      fullPrompt += "Focus Time: " + formatTime(totalFocusTime) + "\n";
      fullPrompt += "Break Time: " + formatTime(totalBreakTime) + "\n";
      fullPrompt += "Breaks taken: " + String(breakCount) + "\n";
      fullPrompt += "Productivity Score: " + String(productivityScore) + "%\n";
      fullPrompt += "Instruction: Address the user as " + userName + ". Respond with one short, witty sentence in English or Portuguese under 30 characters.";

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
      case EVENT_STREAK_BEATEN: quote = localStreakBeaten[randIdx]; break;
    }

    String personalQuote = personalizeQuote(String(quote), userName);

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    lastResponseIsAi = false;
    if (eventType == EVENT_WELCOME_BACK || eventType == EVENT_FIRST_SIT || eventType == EVENT_STREAK_BEATEN) {
      char welcomeMsg[128];
      snprintf(welcomeMsg, sizeof(welcomeMsg), "%s (%s)", personalQuote.c_str(), detail.c_str());
      aiResponse = String(welcomeMsg);
    } else {
      aiResponse = personalQuote;
    }
    hasNewAIResponse = true;
    xSemaphoreGive(geminiMutex);
  }
}

// Save daily statistics to LittleFS using an atomic rename pattern
void saveDailyStats() {
  fs::File file = LittleFS.open("/stats.json.tmp", "w");
  if (!file) {
    return;
  }
  DynamicJsonDocument doc(512);
  doc["firstSitToday"] = firstSitToday;
  doc["firstSitEpoch"] = firstSitEpoch;
  doc["breakCount"] = breakCount;
  doc["totalDeskTime"] = totalDeskTime;
  doc["totalFocusTime"] = totalFocusTime;
  doc["totalBreakTime"] = totalBreakTime;
  doc["overnightBreakDuration"] = overnightBreakDuration;
  doc["lastAwayEpoch"] = lastAwayEpoch;
  doc["dailyAiRequestCount"] = dailyAiRequestCount;
  doc["lastNtpDay"] = lastNtpDay;
  doc["longestSittingStreak"] = longestSittingStreak;
  doc["userName"] = userName;

  if (serializeJson(doc, file) == 0) {
    file.close();
    return;
  }
  file.close();

  if (LittleFS.exists("/stats.json")) {
    LittleFS.remove("/stats.json");
  }
  LittleFS.rename("/stats.json.tmp", "/stats.json");
}

// Load daily statistics from LittleFS
void loadDailyStats() {
  if (!LittleFS.exists("/stats.json")) {
    return;
  }
  fs::File file = LittleFS.open("/stats.json", "r");
  if (!file) {
    return;
  }
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);
  if (!error) {
    firstSitToday = doc["firstSitToday"] | true;
    firstSitEpoch = doc["firstSitEpoch"] | 0;
    breakCount = doc["breakCount"] | 0;
    totalDeskTime = doc["totalDeskTime"] | 0UL;
    totalFocusTime = doc["totalFocusTime"] | 0UL;
    totalBreakTime = doc["totalBreakTime"] | 0UL;
    overnightBreakDuration = doc["overnightBreakDuration"] | 0UL;
    lastAwayEpoch = doc["lastAwayEpoch"] | 0;
    dailyAiRequestCount = doc["dailyAiRequestCount"] | 0;
    lastNtpDay = doc["lastNtpDay"] | -1;
    longestSittingStreak = doc["longestSittingStreak"] | 0UL;
    if (doc.containsKey("userName")) {
      userName = doc["userName"].as<String>();
    }
  }
  file.close();
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
    .led {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      transition: background-color 0.2s ease, box-shadow 0.2s ease;
    }
    .led-off {
      background-color: #334155;
      box-shadow: inset 0 2px 4px rgba(0,0,0,0.4);
    }
    .presence-on {
      background-color: #10b981;
      box-shadow: 0 0 12px #10b981, inset 0 2px 4px rgba(255,255,255,0.4);
    }
    .movement-on {
      background-color: #3b82f6;
      box-shadow: 0 0 12px #3b82f6, inset 0 2px 4px rgba(255,255,255,0.4);
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>DeskBuddy</h1>
    <a href="/settings" class="cog-btn" title="Settings">
      <svg viewBox="0 0 24 24">
        <path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/>
      </svg>
    </a>
  </div>

  <div class="card ai-card">
    <h1>DeskBuddy's Message</h1>
    <div class="ai-message" id="aiMessage">Waiting for activity...</div>
    <div class="ai-badge" id="aiBadge">(AI Generated)</div>
    <div class="ai-loading-container" id="aiLoading" style="display: none;">
      <div class="spinner"></div>
      <span>DeskBuddy is thinking...</span>
    </div>
  </div>

  <div class="card">
    <h1 style="margin-bottom: 15px;">Live Sensor Indicators</h1>
    <div style="display: flex; justify-content: space-around; align-items: center;">
      <div style="display: flex; align-items: center; gap: 10px;">
        <div id="presenceLight" class="led led-off"></div>
        <span class="label" style="font-weight: 500;">Presence Detected</span>
      </div>
      <div style="display: flex; align-items: center; gap: 10px;">
        <div id="movementLight" class="led led-off"></div>
        <span class="label" style="font-weight: 500;">Motion Detected</span>
      </div>
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
      <span class="label">Overnight Break Duration</span>
      <span class="value" id="overnightBreak">-</span>
    </div>
    <div class="metric">
      <span class="label">Break Count</span>
      <span class="value" id="breaks">-</span>
    </div>
    <div class="metric">
      <span class="label">Latest Break Duration</span>
      <span class="value" id="latestBreak">-</span>
    </div>
    <div class="metric">
      <span class="label">Longest Sitting Streak</span>
      <span class="value" id="longestStreak">-</span>
    </div>
    <div class="metric">
      <span class="label">First Sit of Day</span>
      <span class="value" id="firstSitTime">-</span>
    </div>
    <div class="metric">
      <span class="label">Session Motion Ratio</span>
      <span class="value"><span id="motionRatio">-</span> %</span>
    </div>
    <div class="metric">
      <span class="label">Session Average Distance</span>
      <span class="value"><span id="sessionDistAvg">-</span> cm</span>
    </div>
    <div class="metric">
      <span class="label">Live Detection Distance</span>
      <span class="value"><span id="liveDetectionDist">-</span> cm</span>
    </div>
    <div class="metric">
      <span class="label">Total Motion Time</span>
      <span class="value" id="motionTime">-</span>
    </div>
    <div class="metric">
      <span class="label">Motion Trigger Count</span>
      <span class="value" id="motionCount">-</span>
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
          let presenceLight = document.getElementById('presenceLight');
          if (presenceLight) {
            presenceLight.className = data.presenceDetected ? 'led presence-on' : 'led led-off';
          }
          let movementLight = document.getElementById('movementLight');
          if (movementLight) {
            movementLight.className = data.movingTargetDetected ? 'led movement-on' : 'led led-off';
          }
          
          document.getElementById('deskTime').innerText = data.deskTime;
          document.getElementById('focusTime').innerText = data.focusTime;
          document.getElementById('breakTime').innerText = data.breakTime;
          document.getElementById('overnightBreak').innerText = data.overnightBreak;
          document.getElementById('breaks').innerText = data.breaks;
          document.getElementById('latestBreak').innerText = data.latestBreak;
          document.getElementById('longestStreak').innerText = data.longestStreak;
          document.getElementById('firstSitTime').innerText = data.firstSitTime;
          document.getElementById('motionRatio').innerText = data.motionRatio;
          document.getElementById('sessionDistAvg').innerText = data.sessionDistAvg;
          document.getElementById('liveDetectionDist').innerText = data.detectionDist;
          document.getElementById('motionTime').innerText = data.totalMotionTime;
          document.getElementById('motionCount').innerText = data.motionCount;
          
          let scoreColor = "score-high";
          if (data.score < 40) scoreColor = "score-low";
          else if (data.score < 70) scoreColor = "score-med";
          document.getElementById('score').innerHTML = `<span class="${scoreColor}">${data.score}%</span>`;
          
          if (data.aiMessage && data.aiMessage.trim() !== "") {
            document.getElementById('aiMessage').innerText = data.aiMessage;
          } else {
            document.getElementById('aiMessage').innerText = "Waiting for activity...";
          }
          document.getElementById('aiLoading').style.display = data.aiLoading ? "flex" : "none";
          
          const isAi = data.isAiGenerated && data.aiMessage && data.aiMessage.trim() !== "" && data.aiMessage !== "Waiting for activity...";
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

void handleSettings() {
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
              <span class="value" id="g0mSensVal">90</span>
            </div>
            <input type="range" name="g0mSens" id="g0mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 0 Static Sensitivity</span>
              <span class="value" id="g0sSensVal">90</span>
            </div>
            <input type="range" name="g0sSens" id="g0sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g0sSensVal').innerText = this.value">
          </div>
          <!-- Gate 1 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Moving Sensitivity</span>
              <span class="value" id="g1mSensVal">60</span>
            </div>
            <input type="range" name="g1mSens" id="g1mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 1 Static Sensitivity</span>
              <span class="value" id="g1sSensVal">40</span>
            </div>
            <input type="range" name="g1sSens" id="g1sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g1sSensVal').innerText = this.value">
          </div>
          <!-- Gate 2 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Moving Sensitivity</span>
              <span class="value" id="g2mSensVal">50</span>
            </div>
            <input type="range" name="g2mSens" id="g2mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 2 Static Sensitivity</span>
              <span class="value" id="g2sSensVal">40</span>
            </div>
            <input type="range" name="g2sSens" id="g2sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g2sSensVal').innerText = this.value">
          </div>
          <!-- Gate 3 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Moving Sensitivity</span>
              <span class="value" id="g3mSensVal">40</span>
            </div>
            <input type="range" name="g3mSens" id="g3mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 3 Static Sensitivity</span>
              <span class="value" id="g3sSensVal">40</span>
            </div>
            <input type="range" name="g3sSens" id="g3sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g3sSensVal').innerText = this.value">
          </div>
          <!-- Gate 4 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Moving Sensitivity</span>
              <span class="value" id="g4mSensVal">45</span>
            </div>
            <input type="range" name="g4mSens" id="g4mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 4 Static Sensitivity</span>
              <span class="value" id="g4sSensVal">40</span>
            </div>
            <input type="range" name="g4sSens" id="g4sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g4sSensVal').innerText = this.value">
          </div>
          <!-- Gate 5 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Moving Sensitivity</span>
              <span class="value" id="g5mSensVal">50</span>
            </div>
            <input type="range" name="g5mSens" id="g5mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 5 Static Sensitivity</span>
              <span class="value" id="g5sSensVal">40</span>
            </div>
            <input type="range" name="g5sSens" id="g5sSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g5sSensVal').innerText = this.value">
          </div>
          <!-- Gate 6 -->
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0; border-top: 1px solid #334155;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Moving Sensitivity</span>
              <span class="value" id="g6mSensVal">50</span>
            </div>
            <input type="range" name="g6mSens" id="g6mSensSlider" min="0" max="100" step="1" class="slider" oninput="document.getElementById('g6mSensVal').innerText = this.value">
          </div>
          <div class="metric" style="flex-direction: column; align-items: stretch; gap: 4px; padding: 12px 0;">
            <div style="display: flex; justify-content: space-between;">
              <span class="label">Gate 6 Static Sensitivity</span>
              <span class="value" id="g6sSensVal">40</span>
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

void handleRadarData() {
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

void handleSaveSettings() {
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



void handleResetStats() {
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
  static String lastMetricText = "";
  static uint16_t lastMetricColor = 0;

  // Handle bezel ring animation transition
  RGBColor targetColor = stateColors[currentPresenceState];
  if (targetColor != targetRingColor) {
    startRingColor = currentRingColor;
    targetRingColor = targetColor;
    ringTransitionStart = now;
  }

  static unsigned long lastRingUpdate = 0;
  bool isTransitioning = (currentRingColor != targetRingColor);
  bool ringRedrawn = false;

  // Rework mood ring so it is part of the face plate layout.
  // clockFace 0 (Default) and 1 (Minimalist) have it. clockFace 2 (HiTech) does not.
  bool faceplateHasRing = (clockFace == 0 || clockFace == 1);
  bool shouldDrawRing = faceplateHasRing && (currentPresenceState != STATE_AWAY) && (now >= aiScreenEndTime);

  if (shouldDrawRing && (forceRingRedraw || (isTransitioning && (now - lastRingUpdate > 50)))) {
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
    
    // Draw 3px thick bezel ring with smooth subpixel antialiasing using TFT_eSPI's drawSmoothRoundRect
    // Center: (120, 120), Outer Radius: 118, Inner Radius: 116 (3px solid thickness from 116 to 118)
    // Outer AA boundary is at radius 119 (pixel coordinates 1 to 239, fully within 240x240 screen boundary)
    // Inner AA boundary is at radius 115.
    uint16_t color565 = tft.color565(currentRingColor.r, currentRingColor.g, currentRingColor.b);
    tft.drawSmoothRoundRect(2, 2, 118, 116, 0, 0, color565, TFT_BLACK);
    lastRingUpdate = now;
    forceRingRedraw = false;
    ringRedrawn = true;
  } else if (!shouldDrawRing) {
    if (isTransitioning) {
      currentRingColor = targetRingColor; // Instantly catch up state in the background
    }
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
    bool isAiCopy = lastResponseIsAi;
    hasNewAIResponse = false;
    xSemaphoreGive(geminiMutex);

    // Enter AI screen mode for 8 seconds
    aiScreenEndTime = now + 8000;
    drawCenteredWrappedText(responseCopy, TFT_SKYBLUE, isAiCopy);
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
      drawRLEImage("/away.rle", 0, 0);

      lastDisplayedState = STATE_AWAY;
      lastDisplayedPage = -1;
      forceRingRedraw = true;
    }
    return;
  }

  // If we are waiting for the AI welcome response, keep showing the away image/splash screen
  if (isAILoading && (lastTriggeredEventType == EVENT_WELCOME_BACK || lastTriggeredEventType == EVENT_FIRST_SIT)) {
    return;
  }

  // If user is PRESENT, draw clock face
  lastDisplayedState = currentPresenceState;

  static int lastClockFace = -1;
  bool forceRedraw = false;
  if (clockFace != lastClockFace) {
    tft.fillScreen(TFT_BLACK);
    lastMetricText = "";
    forceRingRedraw = true;
    lastClockFace = clockFace;
    forceRedraw = true;
  }

  // Clear screen if we just transitioned from Away or AI screen
  if (lastDisplayedPage != 0) {
    tft.fillScreen(TFT_BLACK);
    lastDisplayedPage = 0;
    forceRingRedraw = true;
    lastMetricText = "";
    forceRedraw = true;
  }

  switch (clockFace) {
    case 1:
      drawMinimalistClockFace(now, forceRedraw || ringRedrawn);
      break;
    case 2:
      drawHiTechClockFace(now, forceRedraw || ringRedrawn);
      break;
    case 0:
    default:
      drawDefaultClockFace(now, lastMetricText, lastMetricColor);
      break;
  }
}

void setup(void) {
  // Load persistent configurations
  preferences.begin("deskbuddy", false);
  aiMode = preferences.getInt("aiMode", 1);
  clockFace = preferences.getInt("clockFace", 0);
  targetHours = preferences.getFloat("targetHours", 8.0);
  userName = preferences.getString("userName", "human");
  focusDistanceLimit = preferences.getInt("focusDistLim", 50);
  motionRatioLimit = preferences.getInt("motionRatioLim", 15);
  deskDistanceLimit = preferences.getInt("distLimit", 120);
  filterWindow = preferences.getFloat("filterWindow", 2.0);
  g0mSens = preferences.getInt("g0mSens", 90);
  g0sSens = preferences.getInt("g0sSens", 90);
  g1mSens = preferences.getInt("g1mSens", 60);
  g1sSens = preferences.getInt("g1sSens", 40);
  g2mSens = preferences.getInt("g2mSens", 50);
  g2sSens = preferences.getInt("g2sSens", 40);
  g3mSens = preferences.getInt("g3mSens", 40);
  g3sSens = preferences.getInt("g3sSens", 40);
  g4mSens = preferences.getInt("g4mSens", 45);
  g4sSens = preferences.getInt("g4sSens", 40);
  g5mSens = preferences.getInt("g5mSens", 50);
  g5sSens = preferences.getInt("g5sSens", 40);
  g6mSens = preferences.getInt("g6mSens", 50);
  g6sSens = preferences.getInt("g6sSens", 40);
  preferences.end();

  // Initialize LittleFS & load daily stats
  if (LittleFS.begin(true)) {
    loadDailyStats();
  }

  // Initialize TFT Display & show splash screen
  tft.init();
  tft.setRotation(0);
  drawRLEImage("/away.rle", 0, 0);
  
  unsigned long bootStartTime = millis();

  // Initialize Serial1 for Radar on Pins 0 (RX) and 5 (TX)
  Serial1.begin(256000, SERIAL_8N1, 0, 5); 
  delay(500);
  if (radar.begin(Serial1)) {
    // Configure sensor distance resolution to 0.2m (20cm) programmatically
    uint8_t enter_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(enter_cmd, sizeof(enter_cmd));
    delay(150);
    uint8_t res_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xAA, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(res_cmd, sizeof(res_cmd));
    delay(150);
    uint8_t exit_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(exit_cmd, sizeof(exit_cmd));
    delay(200);
    
    // Restart to apply new resolution
    radar.requestRestart();
    delay(1000); // Give the module time to reboot and load firmware settings
    
    // Re-verify serial connection and configure gates
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

  // Set Hostname & Configure static IP
  WiFi.setHostname("DeskBuddy");
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(SSID, PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Setup NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  // Setup Web Server
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

  // Setup Mutex for Gemini Thread Safety
  geminiMutex = xSemaphoreCreateMutex();

  // Setup OTA Updates
  ArduinoOTA
    .onStart([]() {
      otaInProgress = true;
    })
    .onEnd([]() {
    })
    .onProgress([](unsigned int progress, unsigned int total) {
    })
    .onError([](ota_error_t error) {
    });
  ArduinoOTA.begin();

  lastLoopTime = millis();
  lastStateTransitionTime = millis();
  
  // Force NTP and Weather update on the very first loop execution
  lastHourlyUpdate = millis() - 3600000 - 1000;

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
      firstSitEpoch = 0;
      breakCount = 0;
      totalDeskTime = 0;
      totalFocusTime = 0;
      totalBreakTime = 0;
      overnightBreakDuration = 0;
      dailyAiRequestCount = 0;
      longestSittingStreak = 0;
      latestBreakDuration = 0;
      totalMotionTime = 0;
      motionCount = 0;
      saveDailyStats();
    }
  }

  // Safety NTP capture if first sit happened before time was synced
  if (firstSitEpoch == 0 && !firstSitToday && timeClient.isTimeSet()) {
    firstSitEpoch = timeClient.getEpochTime();
  }

  static bool stablePresence = false;
  static unsigned long lastPresenceChangeTime = 0;

  bool rawPresent = false;
  int rawState = STATE_AWAY;
  sensorPresenceDetected = false;
  sensorMovingTargetDetected = false;
  sensorStaticPresenceDetected = false;

  // Read from the physical radar sensor
  radar.read();
  if (radar.isConnected()) {
    sensorPresenceDetected = radar.presenceDetected();
    sensorStaticPresenceDetected = radar.stationaryTargetDetected();

    if (radar.presenceDetected()) {
      rawDetectionDist = radar.detectionDistance();
    } else {
      rawDetectionDist = 0;
    }

    // Update filtered values at a fixed 10Hz frequency (every 100ms)
    static unsigned long lastFilterUpdate = 0;
    static bool filteredMovingTarget = false;
    if (now - lastFilterUpdate >= 100) {
      lastFilterUpdate = now;
      
      // Filter motion detection
      motionFilter.add(radar.movingTargetDetected() ? 1.0f : 0.0f);
      filteredMovingTarget = (motionFilter.getMedian(10) > 0.5f);

      if (rawDetectionDist > 0) {
        detectionDistFilter.add((float)rawDetectionDist);
        // Accumulate session distance stats
        sessionDistanceSum += rawDetectionDist;
        sessionDistanceCount++;
        sessionDistanceAverage = (float)sessionDistanceSum / sessionDistanceCount;
      }
      int samples = (int)(filterWindow * 10.0f);
      if (samples < 1) samples = 1;
      if (samples > 100) samples = 100;
      if (rawDetectionDist > 0) {
        filteredDetectionDist = detectionDistFilter.getMedian(samples);
      }
    }
    
    sensorMovingTargetDetected = filteredMovingTarget;
  }

  rawPresent = sensorPresenceDetected;
  if (rawPresent) {
    float currentDist = (sessionDistanceAverage > 0.0) ? sessionDistanceAverage : (float)rawDetectionDist;
    bool inFocusZone = (currentDist > 0.0 && currentDist < focusDistanceLimit);
    int motionRatio = (sessionDeskTime > 0) ? (sessionMotionTime * 100) / sessionDeskTime : 0;
    if (motionRatio > 100) motionRatio = 100;
    bool highMotion = (motionRatio > motionRatioLimit);

    if (inFocusZone) {
      rawState = highMotion ? STATE_BUSY : STATE_FOCUS;
    } else {
      rawState = highMotion ? STATE_DISTRACTED : STATE_REGULAR;
    }
  } else {
    rawState = STATE_AWAY;
  }

  // Debouncing logic to filter sensor instability/boundary jitter
  if (rawPresent != stablePresence) {
    unsigned long debounceLimit = rawPresent ? 2000 : 10000; // 2s to confirm presence, 10s to confirm away
    if (now - lastPresenceChangeTime > debounceLimit) {
      stablePresence = rawPresent;
    }
  } else {
    lastPresenceChangeTime = now;
  }

  bool targetPresent = stablePresence;
  int targetState = stablePresence ? ((rawState != STATE_AWAY) ? rawState : STATE_REGULAR) : STATE_AWAY;



  // Handle Presence State Machine Transitions
  if (targetPresent) {
    // Accumulate desk time if static presence is detected
    if (sensorStaticPresenceDetected) {
      totalDeskTime += elapsed;
      sessionDeskTime += elapsed;
    }
    
    // Accumulate focus time
    if (currentPresenceState == STATE_FOCUS) {
      totalFocusTime += elapsed;
    }

    // Accumulate motion time
    if (sensorMovingTargetDetected) {
      sessionMotionTime += elapsed;
      totalMotionTime += elapsed;
    }

    // Count motion occurrences
    static bool lastMovingState = false;
    if (sensorMovingTargetDetected) {
      if (!lastMovingState) {
        motionCount++;
        lastMovingState = true;
      }
    } else {
      lastMovingState = false;
    }

    if (currentPresenceState == STATE_AWAY) {
      // Transition: Away -> Present
      unsigned long breakDuration = now - lastStateTransitionTime;
      
      if (firstSitToday) {
        firstSitToday = false;
        firstSitEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
        if (lastAwayEpoch > 0 && firstSitEpoch >= lastAwayEpoch) {
          overnightBreakDuration = firstSitEpoch - lastAwayEpoch;
        } else {
          overnightBreakDuration = 0;
        }
        triggerBehaviour(EVENT_FIRST_SIT, formatTime(overnightBreakDuration * 1000));
        
        // Reset session metrics on first sit
        sessionDeskTime = 0;
        sessionMotionTime = 0;
        sessionDistanceSum = 0;
        sessionDistanceCount = 0;
        sessionDistanceAverage = 0.0;
      } else if (breakDuration >= 180000UL) { // Only count break if away > 3 minutes
        breakCount++;
        latestBreakDuration = breakDuration;
        triggerBehaviour(EVENT_WELCOME_BACK, formatTime(breakDuration));
        
        // Reset session metrics on true break return
        sessionDeskTime = 0;
        sessionMotionTime = 0;
        sessionDistanceSum = 0;
        sessionDistanceCount = 0;
        sessionDistanceAverage = 0.0;
      }
      
      currentPresenceState = targetState;
      lastStateTransitionTime = now;
      continuousPresenceStart = now;
      lastStretchReminderTime = now;
      if (targetState == STATE_FOCUS) {
        continuousStillStart = now;
      }
    } else {
      // If we just entered focus state, record start time
      if (targetState == STATE_FOCUS && currentPresenceState != STATE_FOCUS) {
        continuousStillStart = now;
      }
      currentPresenceState = targetState;
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

    // Evaluate and update longest sitting streak
    unsigned long currentStreak = now - continuousPresenceStart;
    if (longestSittingStreak >= 60000UL && currentStreak > longestSittingStreak && !streakAlertTriggered) {
      streakAlertTriggered = true;
      triggerBehaviour(EVENT_STREAK_BEATEN, formatTime(longestSittingStreak));
    }
    if (currentStreak > longestSittingStreak && currentStreak >= 60000UL) {
      longestSittingStreak = currentStreak;
    }
  } else {
    if (currentPresenceState != STATE_AWAY) {
      // Transition: Present -> Away
      streakAlertTriggered = false;
      unsigned long focusSessionDuration = 0;
      if (currentPresenceState == STATE_FOCUS) {
        focusSessionDuration = now - continuousStillStart;
      }
      
      // Trigger Focus session congrats if user focused for > 15s
      if (focusSessionDuration > 15000) {
        triggerBehaviour(EVENT_FOCUS_END, formatTime(focusSessionDuration));
      }
      
      currentPresenceState = STATE_AWAY;
      lastStateTransitionTime = now;
      lastAwayEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
      saveDailyStats();
    } else {
      // Accumulate break time
      totalBreakTime += elapsed;
    }
    
    // Clear filters and reset values when user is AWAY
    rawDetectionDist = 0;
    filteredDetectionDist = 0.0;
    sessionDistanceSum = 0;
    sessionDistanceCount = 0;
    sessionDistanceAverage = 0.0;
    detectionDistFilter.clear();
    motionFilter.clear();
  }

  // Update dynamic productivity score
  uint32_t currentEpoch = timeClient.isTimeSet() ? timeClient.getEpochTime() : 0;
  uint32_t workdayElapsed = (firstSitEpoch > 0 && currentEpoch >= firstSitEpoch) ? (currentEpoch - firstSitEpoch) : 0;

  if (firstSitToday || workdayElapsed < 300) {
    // Default to 100% initially (first 5 minutes of work)
    productivityScore = 100;
  } else {
    float hoursElapsed = (float)workdayElapsed / 3600.0f;
    
    // 1. Break frequency penalty (target: 1 break/hour = 25% penalty)
    float penalty_breaks = 25.0f * ((float)breakCount / hoursElapsed);
    
    // 2. Break duration penalty (target: 10% of workday in breaks = 25% penalty)
    unsigned long activeBreakMs = 0;
    unsigned long workdayElapsedMs = (unsigned long)workdayElapsed * 1000;
    if (workdayElapsedMs > totalDeskTime) {
      activeBreakMs = workdayElapsedMs - totalDeskTime;
    }
    float breakTimeRatio = (float)(activeBreakMs / 1000.0f) / (float)workdayElapsed;
    float penalty_time = 25.0f * (breakTimeRatio / 0.10f);
    
    // 3. Focus bonus (Focus counts 1.5x)
    float bonus_focus = 0.0f;
    if (totalDeskTime > 0) {
      bonus_focus = 1.5f * (((float)totalFocusTime * 100.0f) / (float)totalDeskTime);
    }
    
    float raw_score = 100.0f - penalty_breaks - penalty_time + bonus_focus;
    productivityScore = (int)constrain(raw_score, 0.0f, 100.0f);
  }

  // Handle NTP Time & Weather Fetch Updates (every 1 hour)
  if (WiFi.status() == WL_CONNECTED && now - lastHourlyUpdate > 3600000) {
    timeClient.update();
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
    lastHourlyUpdate = now;
  }

  // Periodically save stats to LittleFS (every 60 seconds) if anything has changed
  static unsigned long lastStatsSave = 0;
  static bool statsInit = false;
  static unsigned long lastSavedDeskTime = 0;
  static unsigned long lastSavedFocusTime = 0;
  static unsigned long lastSavedBreakTime = 0;
  static int lastSavedBreakCount = 0;
  static uint32_t lastSavedFirstSitEpoch = 0;
  static unsigned long lastSavedLongestStreak = 0;
  static String lastSavedUserName = "";

  if (!statsInit) {
    lastSavedDeskTime = totalDeskTime;
    lastSavedFocusTime = totalFocusTime;
    lastSavedBreakTime = totalBreakTime;
    lastSavedBreakCount = breakCount;
    lastSavedFirstSitEpoch = firstSitEpoch;
    lastSavedLongestStreak = longestSittingStreak;
    lastSavedUserName = userName;
    statsInit = true;
  }

  if (now - lastStatsSave > 60000) {
    lastStatsSave = now;
    if (totalDeskTime != lastSavedDeskTime || 
        totalFocusTime != lastSavedFocusTime || 
        totalBreakTime != lastSavedBreakTime || 
        breakCount != lastSavedBreakCount ||
        firstSitEpoch != lastSavedFirstSitEpoch ||
        longestSittingStreak != lastSavedLongestStreak ||
        userName != lastSavedUserName) {
      saveDailyStats();
      lastSavedDeskTime = totalDeskTime;
      lastSavedFocusTime = totalFocusTime;
      lastSavedBreakTime = totalBreakTime;
      lastSavedBreakCount = breakCount;
      lastSavedFirstSitEpoch = firstSitEpoch;
      lastSavedLongestStreak = longestSittingStreak;
      lastSavedUserName = userName;
    }
  }

  // Update TFT Display
  updateTFTDisplay(now);

  delay(10);
}