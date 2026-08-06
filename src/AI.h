#ifndef AI_H
#define AI_H

#include <Arduino.h>
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

extern String formatTime(unsigned long ms);
extern const int AI_RESPONSE_MAX_CHARS;
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

inline const char* getLocalFallbackQuote(int eventType) {
  int persona = appConfig.aiPersona;
  if (persona < 0 || persona > 3) persona = 0;
  int randIdx = random(5);
  switch (eventType) {
    case EVENT_FIRST_SIT:     return localFirstSit[persona][randIdx];
    case EVENT_WELCOME_BACK:  return localWelcomeBack[persona][randIdx];
    case EVENT_LATEHOURS_SIT: return localLateHours[persona][randIdx];
    case EVENT_STRETCH:       return localStretch[persona][randIdx];
    case EVENT_FOCUS_END:     return localFocus[persona][randIdx];
    case EVENT_SLACKER:       return localSlacker[persona][randIdx];
    case EVENT_STREAK_BEATEN: return localStreakBeaten[persona][randIdx];
    case EVENT_LUNCH_REMINDER: return localLunchReminder[persona][randIdx];
    case EVENT_EXCESSIVE_BREAKS: return localExcessiveBreaks[persona][randIdx];
    case EVENT_GOAL_COMPLETED:   return localGoalCompleted[persona][randIdx];
    case EVENT_NAGGING:          return localNagging[persona][randIdx];
    case EVENT_POINTS:           return localPoints[persona][randIdx];
    case EVENT_CURATION:         return localCuration[persona][randIdx];
  }
  return localWelcomeBack[persona][randIdx];
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

void publishMqttDebugRequest(const String& url, const String& body) {
  if (appState.mqttConnected) {
    String fullReq = "---------- AI Request ----------\n";
    fullReq += "URL: " + url + "\n";
    fullReq += "Body: " + body + "\n";
    fullReq += "-------------------------------";
    enqueueMqttPublish("deskbuddy/debug/ai/request", fullReq);
  }
}

void publishMqttDebugResponse(int httpCode, const String& payload) {
  if (appState.mqttConnected) {
    String fullResp = "---------- AI Response ----------\n";
    fullResp += "HTTP Code: " + String(httpCode) + "\n";
    fullResp += "Payload: " + payload + "\n";
    fullResp += "--------------------------------";
    enqueueMqttPublish("deskbuddy/debug/ai/response", fullResp);
  }
}

// Asynchronous FreeRTOS Task for AI HTTPS Queries (Persistent background worker)
void aiQueryTask(void * parameter) {
  while (true) {
    // Wait for task notification to execute a query
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    String prompt = appState.currentPrompt;
    uint32_t querySessionId = aiQuerySessionId;
    xSemaphoreGive(appState.aiMutex);

    Logger::log("BEHAVIOUR", "AI Request: session=%u promptLen=%d", querySessionId, prompt.length());

    bool success = false;
    int httpCode = -1;
    
    // Create the AI connect client on the task stack only during active query
    {
      ESP32_AI_Connect ai("openai-compatible", appConfig.groqApiKey.c_str(), "llama-3.3-70b-versatile", "https://api.groq.com/openai/v1/chat/completions");
      ai.setChatTemperature(0.5);
      ai.setChatMaxTokens(AI_RESPONSE_MAX_CHARS * 2 + 10);
      
      String response = ai.chat(prompt);
      httpCode = ai.getChatResponseCode();

      if (httpCode == 200 && response.length() > 0) {
        response.trim();
        
        if (response.startsWith("\"") && response.endsWith("\"")) {
          response = response.substring(1, response.length() - 1);
        }
        
        Logger::log("BEHAVIOUR", "AI Response Success: len=%d resp=\"%s\"", response.length(), response.c_str());
        
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
      Logger::log("BEHAVIOUR", "AI Request Failed: http=%d (%s) WiFi=%s IP=%s RSSI=%d. Loading local fallback.",
                  httpCode, HTTPClient::errorToString(httpCode).c_str(),
                  wifiStatusStr.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      
      const char* quote = getLocalFallbackQuote(appState.lastTriggeredEventType);

      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      appState.lastResponseIsAi = false;
      String nameCopy = appState.currentUserName;
      xSemaphoreGive(appState.aiMutex);

      String personalQuote = resolveLocalPlaceholders(String(quote), appState.lastTriggeredEventDetail);
      
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
    
    appState.isAILoading = false;
  }
}

// Coordinated behaviour trigger: runs background AI task or picks local fallback
inline void triggerBehaviour(int eventType, String detail = "", int forceMode = 0) {
  appState.lastTriggeredEventType = eventType;
  Logger::log("BEHAVIOUR", "triggerBehaviour: event=%d detail=\"%s\" force=%d", eventType, detail.c_str(), forceMode);

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
      int pipe2 = detail.indexOf('|', pipe1 + 1);
      if (pipe1 != -1 && pipe2 != -1) {
        int pageIdx = detail.substring(5, pipe1).toInt();
        int pLines = detail.substring(pipe1 + 1, pipe2).toInt();
        
        String remaining = detail.substring(pipe2 + 1);
        int separatorIdx = remaining.indexOf("|||");
        
        String pageContent = "";
        String nextPages = "";
        if (separatorIdx != -1) {
          pageContent = remaining.substring(0, separatorIdx);
          nextPages = remaining.substring(separatorIdx + 3);
        } else {
          pageContent = remaining;
        }

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
        }
      }
      return;
    }
  }

  if (eventType == EVENT_TASK_DUE) {
    std::vector<String> tasks;
    int start = 0;
    int idx = 0;
    while ((idx = detail.indexOf('|', start)) != -1) {
      tasks.push_back(detail.substring(start, idx));
      start = idx + 1;
    }
    if (start < detail.length()) {
      tasks.push_back(detail.substring(start));
    }
    if (tasks.empty() && detail.length() > 0) {
      tasks.push_back(detail);
    }

    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", h, m);
    String timeStr = String(timeBuf);

    String pageContent = "[YELLOW]DUE NOW\n\n";
    for (size_t i = 0; i < tasks.size(); i++) {
      pageContent += "[WHITE]- " + tasks[i];
      if (i < tasks.size() - 1) {
        pageContent += "\n";
      }
    }
    pageContent += "\n\n[RED]" + timeStr;

    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = pageContent;
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
        
        // Immediately load fallback
        const char* quote = getLocalFallbackQuote(eventType);

        String personalQuote = resolveLocalPlaceholders(String(quote), detail);
        xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = personalQuote;
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.aiMutex);
      }
    } else {
      Logger::log("BEHAVIOUR", "AI query already loading; using local fallback instead of dropping event %d", eventType);
      const char* quote = getLocalFallbackQuote(eventType);

      String personalQuote = resolveLocalPlaceholders(String(quote), detail);
      Logger::log("BEHAVIOUR", "Picked local fallback (AI busy): \"%s\"", personalQuote.c_str());
      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      appState.lastResponseIsAi = false;
      appState.aiResponse = personalQuote;
      appState.hasNewAIResponse = true;
      xSemaphoreGive(appState.aiMutex);
    }
  } else {
    const char* quote = getLocalFallbackQuote(eventType);

    String personalQuote = resolveLocalPlaceholders(String(quote), detail);
    Logger::log("BEHAVIOUR", "Picked local fallback: \"%s\"", personalQuote.c_str());

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = personalQuote;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.aiMutex);
  }
}

#endif // AI_H
