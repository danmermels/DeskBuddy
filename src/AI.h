#ifndef AI_H
#define AI_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESP32_AI_Connect.h>
#include "Behaviour.h"
#include "Curation.h"
#include "Points.h"
#include "MqttService.h"
#include "MessageManager.h"

#include "State.h"
#include "Logger.h"
#include <NTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern NTPClient timeClient;
extern volatile uint32_t currentSitDownSessionId;
extern uint32_t aiQuerySessionId;
extern TaskHandle_t aiQueryTaskHandle;

// Defined in Faceplates.h: releases the current faceplate's RAM sprites so the
// TLS handshake has contiguous heap during an AI query.
void releaseFaceplateSprites();

extern String formatTime(unsigned long ms);
extern const int AI_RESPONSE_MAX_CHARS;

// Stage-0 diagnostic: log current free-heap and largest-free-block under the
// HEAPAI category. Tag strings must avoid the words "error"/"fail" so the
// control_center observer does not miscount them as AI errors.
inline void logHeapTrace(const char* tag) {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxAlloc = ESP.getMaxAllocHeap();
  Logger::log("HEAPAI", "%s free=%u max=%u", tag, freeHeap, maxAlloc);
}

inline String resolveLocalPlaceholders(String templateStr, String detail) {
  templateStr.replace("{name}", appConfig.userName);
  if (detail == "") {
    templateStr.replace("{detail}", "a while");
  } else {
    templateStr.replace("{detail}", detail);
  }
  if (appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
    templateStr.replace("{score}", "100");
    templateStr.replace("{deskTime}", "0m");
    templateStr.replace("{focusTime}", "0m");
    templateStr.replace("{breakTime}", "0m");
    templateStr.replace("{breakCount}", "0");
  } else {
    templateStr.replace("{score}", String(appStats.productivityScore));
    templateStr.replace("{deskTime}", formatTime(appStats.totalDeskTime));
    templateStr.replace("{focusTime}", formatTime(appStats.totalFocusTime));
    templateStr.replace("{breakTime}", formatTime(appStats.totalBreakTime));
    templateStr.replace("{breakCount}", String(appStats.breakCount));
  }
  templateStr.replace("{longestStreak}", formatTime(appStats.longestSittingStreak));
  int currentDay = timeClient.isTimeSet() ? timeClient.getDay() : 1;
  templateStr.replace("{historyDays}", String(appStats.historyDaysCountWeekly[currentDay]));
  if (timeClient.isTimeSet()) {
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeClient.getHours(), timeClient.getMinutes());
    templateStr.replace("{time}", String(timeBuf));
  } else {
    templateStr.replace("{time}", "unknown hour");
  }
  return templateStr;
}

inline String getLocalFallbackQuote(int eventType) {
  const char* keyName = "welcome_back";
  switch (eventType) {
    case EVENT_FIRST_SIT:        keyName = "first_sit"; break;
    case EVENT_WELCOME_BACK:     keyName = "welcome_back"; break;
    case EVENT_LATEHOURS_SIT:    keyName = "late_hours"; break;
    case EVENT_STRETCH:          keyName = "stretch"; break;
    case EVENT_FOCUS_END:        keyName = "focus_end"; break;
    case EVENT_SLACKER:          keyName = "slacker"; break;
    case EVENT_STREAK_BEATEN:    keyName = "streak_beaten"; break;
    case EVENT_LUNCH_REMINDER:   keyName = "lunch_reminder"; break;
    case EVENT_EXCESSIVE_BREAKS: keyName = "excessive_breaks"; break;
    case EVENT_GOAL_COMPLETED:   keyName = "goal_completed"; break;
    case EVENT_NAGGING:          keyName = "nagging"; break;
    case EVENT_POINTS:           keyName = "points"; break;
    case EVENT_CURATION:         keyName = "curation"; break;
  }

  const char* personaKey = "coach";
  if (appConfig.aiPersona == 1) personaKey = "critic";
  else if (appConfig.aiPersona == 2) personaKey = "sweet";
  else if (appConfig.aiPersona == 3) personaKey = "friend";

#ifdef DESKBUDDY_LANG_PTBR
  const char* primaryFile = "/fallbackquotes_ptbr.json";
#else
  const char* primaryFile = "/fallbackquotes_en.json";
#endif

  const char* fileToOpen = primaryFile;
  if (!LittleFS.exists(fileToOpen)) {
    fileToOpen = "/fallbackquotes.json";
  }

  if (LittleFS.exists(fileToOpen)) {
    fs::File file = LittleFS.open(fileToOpen, "r");
    if (file) {
      logHeapTrace("fallback-start");
      // Filter: only the [event][persona] array is parsed. The full 20KB file
      // previously needed a 32KB doc whose parse collapsed maxAlloc to ~17KB
      // (measured via [HEAPAI]) and triggered allocation aborts on the next
      // AI TLS attempt. The filtered array needs <8KB of pool.
      StaticJsonDocument<128> filter;
      filter[keyName][personaKey] = true;
      DynamicJsonDocument doc(8192);
      DeserializationError err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
      logHeapTrace("fallback-parse-done");
      file.close();
      if (!err) {
        JsonArray arr = doc[keyName][personaKey].as<JsonArray>();
        if (arr && arr.size() > 0) {
          int randIdx = random(arr.size());
          return arr[randIdx].as<String>();
        } else {
          Logger::log("BEHAVIOUR", "Fallback quotes key [%s][%s] not found or empty in %s", keyName, personaKey, fileToOpen);
        }
      } else {
        Logger::log("BEHAVIOUR", "Failed to parse %s: %s", fileToOpen, err.c_str());
      }
    } else {
      Logger::log("BEHAVIOUR", "Failed to open file: %s", fileToOpen);
    }
  } else {
    Logger::log("BEHAVIOUR", "Fallback quotes file not found: %s", fileToOpen);
  }

  return "Bem-vindo de volta, {name}!";
}

inline String resolvePromptPlaceholders(int eventType, String templateStr, String detail) {
  extern const char* PROMPT_PREAMBLE_COACH;
  extern const char* PROMPT_PREAMBLE_CRITIC;
  extern const char* PROMPT_PREAMBLE_SWEET;
  extern const char* PROMPT_PREAMBLE_FRIEND;
  extern const char* PROMPT_BANNED;
  extern int getLearnedWorkdayStart(int dayIndex);
  extern int getLearnedWorkdayEnd(int dayIndex);
  extern int getLearnedLunchHour(int dayIndex);

  const char* activePreamble = PROMPT_PREAMBLE_COACH;
  if (appConfig.aiPersona == 1) { activePreamble = PROMPT_PREAMBLE_CRITIC; }
  else if (appConfig.aiPersona == 2) { activePreamble = PROMPT_PREAMBLE_SWEET; }
  else if (appConfig.aiPersona == 3) { activePreamble = PROMPT_PREAMBLE_FRIEND; }

  // Static counter: include banned phrases ~2 out of 3 calls, let them escape every 3rd
  static uint8_t bannedCounter = 0;
  bannedCounter++;
  bool includeBanned = (bannedCounter % 3 != 0);

  // 1. Get current time and day of week
  String timeOfDayStr = "Unknown Time";
  String dayOfWeekStr = "Unknown Day";
  if (timeClient.isTimeSet()) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      char timeBuf[10];
      strftime(timeBuf, sizeof(timeBuf), "%H:%M", ptm);
      timeOfDayStr = String(timeBuf);
      
      char dayBuf[15];
      strftime(dayBuf, sizeof(dayBuf), "%A", ptm);
      dayOfWeekStr = String(dayBuf);
    }
  }

  // 2. Goal progress calculation
  int goalProgressPct = 0;
  if (appConfig.targetHours > 0.0f) {
    goalProgressPct = (int)((appStats.totalDeskTime * 100.0f) / (appConfig.targetHours * 3600.0f * 1000.0f));
  }
  if (goalProgressPct > 100) goalProgressPct = 100;

  // 3. Observations/Discrepancies
  String discrepancies = getCurationObservations(eventType, detail);
  if (discrepancies == "") {
    discrepancies = "- No unusual deviations from daily work routines detected.";
  }

  // 4. Resolve placeholders inside the templateStr
  // 50/50 chance: use actual name or "the user" to add natural variety
  String nameForPrompt = (random(2) == 0) ? appConfig.userName : "the user";
  String resolvedTemplate = templateStr;
  resolvedTemplate.replace("{name}", nameForPrompt);
  if (detail == "") {
    resolvedTemplate.replace("{detail}", "a while");
  } else {
    resolvedTemplate.replace("{detail}", detail);
  }
  if (appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
    resolvedTemplate.replace("{score}", "100");
    resolvedTemplate.replace("{deskTime}", "0m");
    resolvedTemplate.replace("{focusTime}", "0m");
    resolvedTemplate.replace("{breakTime}", "0m");
    resolvedTemplate.replace("{breakCount}", "0");
  } else {
    resolvedTemplate.replace("{score}", String(appStats.productivityScore));
    resolvedTemplate.replace("{deskTime}", formatTime(appStats.totalDeskTime));
    resolvedTemplate.replace("{focusTime}", formatTime(appStats.totalFocusTime));
    resolvedTemplate.replace("{breakTime}", formatTime(appStats.totalBreakTime));
    resolvedTemplate.replace("{breakCount}", String(appStats.breakCount));
  }
  resolvedTemplate.replace("{longestStreak}", formatTime(appStats.longestSittingStreak));
  int currentDayIdx = timeClient.isTimeSet() ? timeClient.getDay() : 1;
  resolvedTemplate.replace("{historyDays}", String(appStats.historyDaysCountWeekly[currentDayIdx]));
  resolvedTemplate.replace("{time}", timeOfDayStr);
  resolvedTemplate.replace("{dayStart}", "08:00");
  resolvedTemplate.replace("{dayEnd}", "18:00");
  resolvedTemplate.replace("{earlyLate}", "outside work hours");
  if (timeClient.isTimeSet()) {
    time_t epochNow = timeClient.getEpochTime();
    struct tm *ptmNow = localtime(&epochNow);
    if (ptmNow != nullptr) {
      int s = getLearnedWorkdayStart(ptmNow->tm_wday);
      int e = getLearnedWorkdayEnd(ptmNow->tm_wday);
      char startBuf[6], endBuf[6];
      snprintf(startBuf, sizeof(startBuf), "%02d:00", s);
      snprintf(endBuf, sizeof(endBuf), "%02d:00", e);
      resolvedTemplate.replace("{dayStart}", String(startBuf));
      resolvedTemplate.replace("{dayEnd}", String(endBuf));
      resolvedTemplate.replace("{earlyLate}", computeEarlyLateString(*ptmNow));
    }
  }

  // 5. Build structured prompt
  String fullPrompt = "[ROLE]\n";
  fullPrompt += String(activePreamble) + "\n";
  if (includeBanned) fullPrompt += String(PROMPT_BANNED) + "\n";
  fullPrompt += String(CRITICAL_CONSTRAINT) + "\n\n";
  
  fullPrompt += "[LIVE TELEMETRY]\n";
  fullPrompt += "User: " + appConfig.userName + "\n";
  fullPrompt += "Time: " + timeOfDayStr + " (" + dayOfWeekStr + ")\n";
  fullPrompt += "Weather: " + String(appState.temp) + "C, " + appState.weatherDesc + "\n";
  fullPrompt += "Desk Time Today: " + formatTime(appStats.totalDeskTime) + " (Goal Progress: " + String(goalProgressPct) + "%)\n";
  fullPrompt += "Focus Time: " + formatTime(appStats.totalFocusTime) + "\n";
  fullPrompt += "Productivity: " + String(appStats.productivityScore) + "%\n\n";

  fullPrompt += "[OBSERVATIONS]\n";
  fullPrompt += discrepancies + "\n\n";

  fullPrompt += "[ACTION REQUIRED]\n";
  fullPrompt += resolvedTemplate;

  return fullPrompt;
}

inline void publishMqttDebugRequest(const String& url, const String& body) {
  if (appState.mqttConnected) {
    enqueueMqttPublish("deskbuddy/debug/ai/request", body);
  }
}

inline void publishMqttDebugResponse(int httpCode, const String& payload) {
  if (appState.mqttConnected) {
    enqueueMqttPublish("deskbuddy/debug/ai/response", payload);
  }
}

inline void publishMqttDebugTrigger(int eventType, const String& detail, int forceMode) {
  if (appState.mqttConnected) {
    String payload = "{\"event\":" + String(eventType) + ",\"detail\":\"" + detail + "\",\"mode\":" + String(forceMode) + "}";
    enqueueMqttPublish("deskbuddy/debug/ai/trigger", payload);
  }
}

// Asynchronous FreeRTOS Task for AI HTTPS Queries (Persistent background worker)
inline void aiQueryTask(void * parameter) {
  while (true) {
    // Wait for task notification to execute a query
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    String prompt = appState.currentPrompt;
    uint32_t querySessionId = aiQuerySessionId;
    xSemaphoreGive(appState.aiMutex);

    Logger::log("BEHAVIOUR", "AI Request: session=%u promptLen=%d", querySessionId, prompt.length());
    publishMqttDebugRequest("https://api.groq.com/openai/v1/chat/completions", prompt);

    bool success = false;
    int httpCode = -1;
    String aiLastError;
    String aiRawResp;

    // Release the faceplate sprites so the TLS handshake has contiguous heap.
    // The display keeps the last frame while aiTlsInProgress is set and
    // re-initializes the sprites once the query completes.
    aiTlsInProgress = true;
    vTaskDelay(pdMS_TO_TICKS(60)); // let the display finish the current frame
    releaseFaceplateSprites();
    logHeapTrace("query-start");
    {
      ESP32_AI_Connect ai("openai-compatible", appConfig.groqApiKey.c_str(), "openai/gpt-oss-20b", "https://api.groq.com/openai/v1/chat/completions");
      ai.setChatTemperature(0.5);
      ai.setChatMaxTokens(1024); // gpt-oss reasons first; needs budget for chain-of-thought + answer
      ai.setChatParameters("{\"reasoning_effort\":\"low\"}"); // keep reasoning short for a one-liner
      
      String response = ai.chat(prompt);
      httpCode = ai.getChatResponseCode();
      aiLastError = ai.getLastError();
      aiRawResp = ai.getChatRawResponse();

      if (httpCode == 200 && response.length() > 0) {
        response.trim();
        if (response.length() > 2000) response = response.substring(0, 2000); // hard cap for heap/MQTT safety
        
        if (response.startsWith("\"") && response.endsWith("\"")) {
          response = response.substring(1, response.length() - 1);
        }
        
        Logger::log("BEHAVIOUR", "AI Response Success: len=%d resp=\"%s\"", response.length(), response.c_str());
        publishMqttDebugResponse(httpCode, response);
        
        bool discard = false;
      if (appState.lastTriggeredEventType == EVENT_FIRST_SIT || appState.lastTriggeredEventType == EVENT_WELCOME_BACK || appState.lastTriggeredEventType == EVENT_LATEHOURS_SIT) {
          if (querySessionId != currentSitDownSessionId || appState.currentPresenceState == STATE_AWAY) {
            discard = true;
          }
        }
        
        xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
        if (!discard) {
          appState.lastResponseIsAi = true;
          appState.aiResponse = response;
          appState.hasNewAIResponse = true;
        } else {
          Logger::log("BEHAVIOUR", "AI Response Discarded (sessionChanged=%d userAway=%d)", 
                      (querySessionId != currentSitDownSessionId), (appState.currentPresenceState == STATE_AWAY));
        }
        xSemaphoreGive(appState.aiMutex);
        success = true;
      }
    }
    logHeapTrace("query-end");
    aiTlsInProgress = false; // display re-inits the faceplate sprites on its next frame

    // Graceful fallback: If AI query fails, load a local fallback quote immediately
    if (!success) {
      String wifiStatusStr = "UNKNOWN";
      wl_status_t status = WiFi.status();
      switch (status) {
        case WL_IDLE_STATUS:     wifiStatusStr = "IDLE"; break;
        case WL_NO_SSID_AVAIL:   wifiStatusStr = "NO_SSID"; break;
        case WL_SCAN_COMPLETED:  wifiStatusStr = "SCAN_COMPLETED"; break;
        case WL_CONNECTED:       wifiStatusStr = "CONNECTED"; break;
        case WL_CONNECT_FAILED:  wifiStatusStr = "CONNECT_FAILED"; break;
        case WL_CONNECTION_LOST: wifiStatusStr = "CONNECTION_LOST"; break;
        case WL_DISCONNECTED:    wifiStatusStr = "DISCONNECTED"; break;
      }
      Logger::log("BEHAVIOUR", "AI Request Failed: http=%d (%s) err=[%s] raw=[%.250s] WiFi=%s IP=%s RSSI=%d. Loading local fallback.",
                  httpCode, HTTPClient::errorToString(httpCode).c_str(), aiLastError.c_str(), aiRawResp.c_str(),
                  wifiStatusStr.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      publishMqttDebugResponse(httpCode, "RAW:" + aiRawResp.substring(0, 3000)); // full body for diagnosis
      
      String quote = getLocalFallbackQuote(appState.lastTriggeredEventType);

      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      appState.lastResponseIsAi = false;
      String nameCopy = appState.currentUserName;
      xSemaphoreGive(appState.aiMutex);

      String personalQuote = resolveLocalPlaceholders(quote, appState.lastTriggeredEventDetail);
      
      bool discard = false;
      if (appState.lastTriggeredEventType == EVENT_FIRST_SIT || appState.lastTriggeredEventType == EVENT_WELCOME_BACK) {
        if (querySessionId != currentSitDownSessionId || appState.currentPresenceState == STATE_AWAY) {
          discard = true;
        }
      }
      
      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      if (!discard) {
        appState.aiResponse = personalQuote;
        appState.hasNewAIResponse = true;
        Logger::log("BEHAVIOUR", "AI Fallback loaded: \"%s\"", personalQuote.c_str());
      } else {
        Logger::log("BEHAVIOUR", "AI Fallback Discarded (sessionChanged=%d userAway=%d)",
                    (querySessionId != currentSitDownSessionId), (appState.currentPresenceState == STATE_AWAY));
      }
      xSemaphoreGive(appState.aiMutex);
    }

    // Stage-0 diagnostic: log AI task stack high-water mark once after the first query
    static bool hwmLogged = false;
    if (!hwmLogged) {
      hwmLogged = true;
      Logger::log("HWM", "aiQueryTask stack high-water=%u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }

    appState.isAILoading = false;
  }
}

// Coordinated behaviour trigger: runs background AI task or picks local fallback
inline void triggerBehaviour(int eventType, String detail = "", int forceMode = 0) {
  appState.lastTriggeredEventType = eventType;
  Logger::log("BEHAVIOUR", "triggerBehaviour: event=%d detail=\"%s\" force=%d", eventType, detail.c_str(), forceMode);
  publishMqttDebugTrigger(eventType, detail, forceMode);

  auto loadFallback = [&](int evType, const String &det) {
    String q = getLocalFallbackQuote(evType);
    String pq = resolveLocalPlaceholders(q, det);
    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = pq;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.aiMutex);
    return pq;
  };

  // Nagging without an explicit task (e.g. debug TRIGGER NAGGING) resolves the
  // single most-overdue task (daily OR monthly) so the nudge references an actual
  // task by name. This mirrors the first entry the seated 35-min nag would ring.
  if (eventType == EVENT_NAGGING && detail.length() == 0 && timeClient.isTimeSet()) {
    time_t epochNow = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochNow);
    if (ptm != nullptr) {
      int y = ptm->tm_year + 1900;
      int mon = ptm->tm_mon + 1;
      int d = ptm->tm_mday;
      char dStr[11], mStr[8];
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", y, mon, d);
      snprintf(mStr, sizeof(mStr), "%04d-%02d", y, mon);
      int nowMinutes = ptm->tm_hour * 60 + ptm->tm_min;
      std::vector<OverdueTask> queue = buildOverdueTaskQueue(String(dStr), String(mStr), dateToDays(String(dStr)), y, mon, d, nowMinutes);
      if (!queue.empty()) {
        if (appStats.nagQueueIndex >= (int)queue.size()) {
          appStats.nagQueueIndex = 0;
        }
        detail = queue[appStats.nagQueueIndex].text;
        appStats.nagQueueIndex++;
        saveDailyStats();
        Logger::log("BEHAVIOUR", "Nagging detail resolved to \"%s\" (cursor=%d queue=%d)", detail.c_str(), appStats.nagQueueIndex, (int)queue.size());
      }
    }
  }

  // Points check-in without an explicit detail (e.g. debug TRIGGER POINTS)
  // resolves the current-month points snapshot so the nudge always references
  // the live running total and category.
  if (eventType == EVENT_POINTS && detail.length() == 0) {
    detail = buildPointsDetail();
    Logger::log("BEHAVIOUR", "Points detail resolved to \"%s\"", detail.c_str());
  }

  // Curation without explicit detail (e.g. debug TRIGGER CURATION) resolves
  // the live observation so the nudge always references actual state.
  if (eventType == EVENT_CURATION && detail.length() == 0) {
    detail = getCurationNudge();
    if (detail.length() == 0) {
      Logger::log("BEHAVIOUR", "Curation: nothing noteworthy found, dropping event");
      return;
    }
    Logger::log("BEHAVIOUR", "Curation detail resolved to \"%s\"", detail.c_str());
  }

  if (eventType == EVENT_PAGE) {
    // F12: 2nd-screen follow-up page -- render the raw text locally, no AI query
    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = detail;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.aiMutex);
    Logger::log("BEHAVIOUR", "EVENT_PAGE: rendering 2nd screen (%d chars)", detail.length());
    return;
  }

  if (eventType == EVENT_JOURNAL) {
    if (detail.startsWith("PAGE:")) {
      int pipe1 = detail.indexOf('|');
      int pipe2 = (pipe1 != -1) ? detail.indexOf('|', pipe1 + 1) : -1;
      if (pipe1 != -1 && pipe2 != -1) {
        int pageIdx = detail.substring(5, pipe1).toInt();
        int pLines = detail.substring(pipe1 + 1, pipe2).toInt();
        
        int separatorIdx = detail.indexOf("|||", pipe2 + 1);
        String pageContent = (separatorIdx != -1) ? detail.substring(pipe2 + 1, separatorIdx) : detail.substring(pipe2 + 1);
        String nextPages = (separatorIdx != -1) ? detail.substring(separatorIdx + 3) : "";

        Logger::log("BEHAVIOUR", "Journal trigger: showing page %d (lines=%d)", pageIdx, pLines);

        xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = pageContent;
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.aiMutex);

        if (nextPages.length() > 0) {
          unsigned long currentDurationMs = getAlertDurationMs(pLines);
          extern MessageManager messageManager;
          messageManager.scheduleMessageWithPriority(
            EVENT_JOURNAL,
            nextPages,
            MessageManager::P_HIGH,
            currentDurationMs,
            MessageManager::R_NORMAL
          );
          Logger::log("BEHAVIOUR", "Journal trigger: scheduled next pages in %lu ms", currentDurationMs);
        } else {
          appState.journalSequenceActive = false;
        }
      }
      return;
    } else {
      std::vector<String> pages;
      std::vector<int> pageLines;
      buildTaskJournalSummary(pages, pageLines);
      Logger::log("BEHAVIOUR", "Journal trigger: generated %d pages", pages.size());

      if (pages.size() > 0) {
        xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = pages[0];
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.aiMutex);

        if (pages.size() > 1) {
          appState.journalSequenceActive = true;
          String serialized = "";
          for (size_t i = 1; i < pages.size(); i++) {
            serialized += "PAGE:" + String(i + 1) + "|" + String(pageLines[i]) + "|" + pages[i];
            if (i < pages.size() - 1) {
              serialized += "|||";
            }
          }

          unsigned long page1DurationMs = getAlertDurationMs(pageLines[0], true);
          extern MessageManager messageManager;
          messageManager.scheduleMessageWithPriority(
            EVENT_JOURNAL,
            serialized,
            MessageManager::P_HIGH,
            page1DurationMs,
            MessageManager::R_NORMAL
          );
          Logger::log("BEHAVIOUR", "Journal trigger: scheduled remaining pages in %lu ms", page1DurationMs);
        } else {
          appState.journalSequenceActive = false;
        }
      } else {
        appState.journalSequenceActive = false;
      }
      return;
    }
  }

  if (eventType == EVENT_TASK_DUE) {
    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = detail;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.aiMutex);
    return;
  }

  xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
  appState.lastTriggeredEventDetail = detail;
  appState.currentUserName = appConfig.userName;
  xSemaphoreGive(appState.aiMutex);

  bool useAI = false;
  if (forceMode == 1) {
    useAI = true;
  } else if (forceMode == 2) {
    useAI = false;
  } else {
    useAI = (WiFi.status() == WL_CONNECTED);
    if (!useAI) {
      Logger::log("BEHAVIOUR", "WiFi not connected, using local fallback.");
    }
  }

  // Emergency low-heap guard: only skip the TLS attempt when there is
  // genuinely no contiguous room for the handshake. LIVE check, never latched —
  // with the faceplate sprites resident maxAlloc hovers ~45KB, so a latched
  // flag would permanently block AI. A failed handshake is non-fatal and the
  // auto-retry + local fallback handle it.
  if (useAI && ESP.getMaxAllocHeap() < 20000UL) {
    Logger::log("BEHAVIOUR", "Heap critically low, using local fallback.");
    useAI = false;
  }

  if (useAI) {
    String basePrompt = "";
    switch (eventType) {
      case EVENT_FIRST_SIT:     basePrompt = resolvePromptPlaceholders(eventType, PROMPT_FIRST_SIT_OF_DAY, detail); break;
      case EVENT_WELCOME_BACK:  basePrompt = resolvePromptPlaceholders(eventType, PROMPT_WELCOME_BACK, detail); break;
      case EVENT_LATEHOURS_SIT: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_LATEHOURS_SIT, detail); break;
      case EVENT_STRETCH:       basePrompt = resolvePromptPlaceholders(eventType, PROMPT_STRETCH_REMINDER, detail); break;
      case EVENT_FOCUS_END:     basePrompt = resolvePromptPlaceholders(eventType, PROMPT_FOCUS_CONGRATS, detail); break;
      case EVENT_SLACKER:       basePrompt = resolvePromptPlaceholders(eventType, PROMPT_SLACKER_ROAST, detail); break;
      case EVENT_STREAK_BEATEN: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_STREAK_BEATEN, detail); break;
      case EVENT_LUNCH_REMINDER: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_LUNCH_REMINDER, detail); break;
      case EVENT_EXCESSIVE_BREAKS: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_EXCESSIVE_BREAKS, detail); break;
      case EVENT_GOAL_COMPLETED:   basePrompt = resolvePromptPlaceholders(eventType, PROMPT_GOAL_COMPLETED, detail); break;
      case EVENT_NAGGING:          basePrompt = resolvePromptPlaceholders(eventType, PROMPT_NAGGING, detail); break;
      case EVENT_POINTS:           basePrompt = resolvePromptPlaceholders(eventType, PROMPT_POINTS, detail); break;
      case EVENT_CURATION:        basePrompt = resolvePromptPlaceholders(eventType, PROMPT_CURATION, detail); break;
    }

    if (!appState.isAILoading) {
      if (forceMode != 1) {
        appStats.dailyAiRequestCount++;
      }
      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      appState.currentPrompt = basePrompt;
      aiQuerySessionId = currentSitDownSessionId;
      xSemaphoreGive(appState.aiMutex);
      
      appState.isAILoading = true;
      appState.lastAiQueryStartTime = millis();
      Logger::log("BEHAVIOUR", "Starting AI query task (dailyCount=%d)", appStats.dailyAiRequestCount);
      
      if (aiQueryTaskHandle != NULL) {
        xTaskNotifyGive(aiQueryTaskHandle);
      } else {
        Logger::log("BEHAVIOUR", "ERROR: AI Query Task handle is NULL. Using local fallback.");
        appState.isAILoading = false;
        String pq = loadFallback(eventType, detail);
        Logger::log("BEHAVIOUR", "Picked local fallback (null handle): \"%s\"", pq.c_str());
      }
    } else {
      Logger::log("BEHAVIOUR", "AI query already loading; using local fallback instead of dropping event %d", eventType);
      String pq = loadFallback(eventType, detail);
      Logger::log("BEHAVIOUR", "Picked local fallback (AI busy): \"%s\"", pq.c_str());
    }
  } else {
    String pq = loadFallback(eventType, detail);
    Logger::log("BEHAVIOUR", "Picked local fallback: \"%s\"", pq.c_str());
  }
}

#endif // AI_H
