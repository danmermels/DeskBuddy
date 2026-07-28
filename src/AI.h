#ifndef AI_H
#define AI_H

#include <Arduino.h>
#include <ESP32_AI_Connect.h>
#include "Behaviour.h"
#include "Curation.h"
#include "MqttService.h"
#include "MessageManager.h"

#include "State.h"
#include "Logger.h"
#include <NTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern NTPClient timeClient;
extern volatile uint32_t currentSitDownSessionId;
extern uint32_t geminiQuerySessionId;
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
  return templateStr;
}

inline String resolvePromptPlaceholders(int eventType, String templateStr, String detail) {
  extern const char* PROMPT_PREAMBLE_COACH;
  extern const char* PROMPT_PREAMBLE_CRITIC;
  extern const char* PROMPT_PREAMBLE_SWEET;
  extern const char* PROMPT_PREAMBLE_FRIEND;
  extern const char* PROMPT_BANNED_COACH;
  extern const char* PROMPT_BANNED_CRITIC;
  extern const char* PROMPT_BANNED_SWEET;
  extern const char* PROMPT_BANNED_FRIEND;
  extern int getLearnedWorkdayStart(int dayIndex);
  extern int getLearnedWorkdayEnd(int dayIndex);
  extern int getLearnedLunchHour(int dayIndex);

  const char* activePreamble = PROMPT_PREAMBLE_COACH;
  const char* activeBanned = PROMPT_BANNED_COACH;
  if (appConfig.aiPersona == 1) { activePreamble = PROMPT_PREAMBLE_CRITIC; activeBanned = PROMPT_BANNED_CRITIC; }
  else if (appConfig.aiPersona == 2) { activePreamble = PROMPT_PREAMBLE_SWEET; activeBanned = PROMPT_BANNED_SWEET; }
  else if (appConfig.aiPersona == 3) { activePreamble = PROMPT_PREAMBLE_FRIEND; activeBanned = PROMPT_BANNED_FRIEND; }

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

  // 5. Build structured prompt
  String fullPrompt = "[ROLE]\n";
  fullPrompt += String(activePreamble) + "\n";
  if (includeBanned) fullPrompt += String(activeBanned) + "\n";
  fullPrompt += "CRITICAL CONSTRAINT: Respond with exactly ONE short sentence in English. Keep it between 75-85 characters total (maximum 90, including spaces/punctuation). Output ONLY the raw response. Do not wrap in quotes.\n\n";
  
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

// Asynchronous FreeRTOS Task for Gemini HTTPS Queries (Persistent background worker)
void queryGeminiTask(void * parameter) {
  while (true) {
    // Wait for task notification to execute a query
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
    String prompt = appState.currentPrompt;
    uint32_t querySessionId = geminiQuerySessionId;
    xSemaphoreGive(appState.geminiMutex);

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
        if (appState.lastTriggeredEventType == EVENT_FIRST_SIT || appState.lastTriggeredEventType == EVENT_WELCOME_BACK) {
          if (querySessionId != currentSitDownSessionId || appState.currentPresenceState == STATE_AWAY) {
            discard = true;
          }
        }
        
        xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
        if (!discard) {
          appState.lastResponseIsAi = true;
          appState.aiResponse = response;
          appState.hasNewAIResponse = true;
        } else {
          Logger::log("BEHAVIOUR", "AI Response Discarded (sessionChanged=%d userAway=%d)", 
                      (querySessionId != currentSitDownSessionId), (appState.currentPresenceState == STATE_AWAY));
        }
        xSemaphoreGive(appState.geminiMutex);
        success = true;
      }
    }

    // Graceful fallback: If Gemini query fails, load a local fallback quote immediately
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
      
      const char* quote = "";
      int randIdx = random(5);
      int persona = appConfig.aiPersona;
      if (persona < 0 || persona > 3) persona = 0;

      switch (appState.lastTriggeredEventType) {
        case EVENT_FIRST_SIT:     quote = localFirstSit[persona][randIdx]; break;
        case EVENT_WELCOME_BACK:  quote = localWelcomeBack[persona][randIdx]; break;
        case EVENT_STRETCH:       quote = localStretch[persona][randIdx]; break;
        case EVENT_FOCUS_END:     quote = localFocus[persona][randIdx]; break;
        case EVENT_SLACKER:       quote = localSlacker[persona][randIdx]; break;
        case EVENT_STREAK_BEATEN: quote = localStreakBeaten[persona][randIdx]; break;
        case EVENT_LUNCH_REMINDER: quote = localLunchReminder[persona][randIdx]; break;
        default:                  quote = localWelcomeBack[persona][randIdx]; break;
      }
      
      xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
      appState.lastResponseIsAi = false;
      String nameCopy = appState.currentUserName;
      xSemaphoreGive(appState.geminiMutex);

      String personalQuote = resolveLocalPlaceholders(String(quote), appState.lastTriggeredEventDetail);
      
      bool discard = false;
      if (appState.lastTriggeredEventType == EVENT_FIRST_SIT || appState.lastTriggeredEventType == EVENT_WELCOME_BACK) {
        if (querySessionId != currentSitDownSessionId || appState.currentPresenceState == STATE_AWAY) {
          discard = true;
        }
      }
      
      xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
      if (!discard) {
        appState.aiResponse = personalQuote;
        appState.hasNewAIResponse = true;
        Logger::log("BEHAVIOUR", "AI Fallback loaded: \"%s\"", personalQuote.c_str());
      } else {
        Logger::log("BEHAVIOUR", "AI Fallback Discarded (sessionChanged=%d userAway=%d)",
                    (querySessionId != currentSitDownSessionId), (appState.currentPresenceState == STATE_AWAY));
      }
      xSemaphoreGive(appState.geminiMutex);
    }
    
    appState.isAILoading = false;
  }
}

// Coordinated behaviour trigger: runs background Gemini task or picks local fallback
inline void triggerBehaviour(int eventType, String detail = "", int forceMode = 0) {
  appState.lastTriggeredEventType = eventType;
  Logger::log("BEHAVIOUR", "triggerBehaviour: event=%d detail=\"%s\" force=%d", eventType, detail.c_str(), forceMode);

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

        xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = pageContent;
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.geminiMutex);

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
        xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = pages[0];
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.geminiMutex);

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

    xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = pageContent;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.geminiMutex);
    return;
  }

  xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
  appState.lastTriggeredEventDetail = detail;
  appState.currentUserName = appConfig.userName;
  xSemaphoreGive(appState.geminiMutex);

  bool useAI = false;
  if (forceMode == 1) {
    useAI = true;
  } else if (forceMode == 2) {
    useAI = false;
  } else {
    if (appConfig.aiMode == 2) {
      // Frequent mode: all events can trigger AI
      useAI = true;
    } else if (appConfig.aiMode == 1) {
      // Balanced mode: AI triggers for tasks, focus, and notifications
      if (eventType == EVENT_FIRST_SIT || eventType == EVENT_STRETCH || eventType == EVENT_WELCOME_BACK || eventType == EVENT_LUNCH_REMINDER || eventType == EVENT_EXCESSIVE_BREAKS || eventType == EVENT_GOAL_COMPLETED || eventType == EVENT_NAGGING) {
        useAI = true;
      }
    }
    // Enforce daily cap (max 15 requests per day) for normal triggers
    if (useAI && appStats.dailyAiRequestCount >= DAILY_AI_LIMIT) {
      useAI = false;
    }
  }

  // Check if WiFi is connected before attempting AI
  if (useAI) {
    if (WiFi.status() != WL_CONNECTED) {
      useAI = false;
      Logger::log("BEHAVIOUR", "WiFi not connected, bypassing AI to use local fallback.");
    }
  }

  if (useAI) {
    String basePrompt = "";
    switch (eventType) {
      case EVENT_FIRST_SIT:     basePrompt = resolvePromptPlaceholders(eventType, PROMPT_FIRST_SIT_OF_DAY, detail); break;
      case EVENT_WELCOME_BACK:  basePrompt = resolvePromptPlaceholders(eventType, PROMPT_WELCOME_BACK, detail); break;
      case EVENT_STRETCH:       basePrompt = resolvePromptPlaceholders(eventType, PROMPT_STRETCH_REMINDER, detail); break;
      case EVENT_FOCUS_END:     basePrompt = resolvePromptPlaceholders(eventType, PROMPT_FOCUS_CONGRATS, detail); break;
      case EVENT_SLACKER:       basePrompt = resolvePromptPlaceholders(eventType, PROMPT_SLACKER_ROAST, detail); break;
      case EVENT_STREAK_BEATEN: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_STREAK_BEATEN, detail); break;
      case EVENT_LUNCH_REMINDER: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_LUNCH_REMINDER, detail); break;
      case EVENT_EXCESSIVE_BREAKS: basePrompt = resolvePromptPlaceholders(eventType, PROMPT_EXCESSIVE_BREAKS, detail); break;
      case EVENT_GOAL_COMPLETED:   basePrompt = resolvePromptPlaceholders(eventType, PROMPT_GOAL_COMPLETED, detail); break;
      case EVENT_JOURNAL:          basePrompt = resolvePromptPlaceholders(eventType, PROMPT_JOURNAL, detail); break;
      case EVENT_NAGGING:          basePrompt = resolvePromptPlaceholders(eventType, PROMPT_NAGGING, detail); break;
      case EVENT_TASK_DUE:         basePrompt = resolvePromptPlaceholders(eventType, PROMPT_TASK_DUE, detail); break;
    }

    if (!appState.isAILoading) {
      if (forceMode != 1) {
        appStats.dailyAiRequestCount++;
      }
      xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
      appState.currentPrompt = basePrompt;
      geminiQuerySessionId = currentSitDownSessionId;
      xSemaphoreGive(appState.geminiMutex);
      
      appState.isAILoading = true;
      appState.lastAiQueryStartTime = millis();
      Logger::log("BEHAVIOUR", "Starting Gemini AI query task (dailyCount=%d)", appStats.dailyAiRequestCount);
      
      if (aiQueryTaskHandle != NULL) {
        xTaskNotifyGive(aiQueryTaskHandle);
      } else {
        Logger::log("BEHAVIOUR", "ERROR: Gemini Query Task handle is NULL. Using local fallback.");
        appState.isAILoading = false;
        
        // Immediately load fallback
        const char* quote = "";
        int randIdx = random(5);
        int persona = appConfig.aiPersona;
        if (persona < 0 || persona > 3) persona = 0;

        switch (eventType) {
          case EVENT_FIRST_SIT:     quote = localFirstSit[persona][randIdx]; break;
          case EVENT_WELCOME_BACK:  quote = localWelcomeBack[persona][randIdx]; break;
          case EVENT_STRETCH:       quote = localStretch[persona][randIdx]; break;
          case EVENT_FOCUS_END:     quote = localFocus[persona][randIdx]; break;
          case EVENT_SLACKER:       quote = localSlacker[persona][randIdx]; break;
          case EVENT_STREAK_BEATEN: quote = localStreakBeaten[persona][randIdx]; break;
          case EVENT_LUNCH_REMINDER: quote = localLunchReminder[persona][randIdx]; break;
          case EVENT_EXCESSIVE_BREAKS: quote = localExcessiveBreaks[persona][randIdx]; break;
          case EVENT_GOAL_COMPLETED:   quote = localGoalCompleted[persona][randIdx]; break;
          case EVENT_JOURNAL:          quote = localJournal[persona][randIdx]; break;
          case EVENT_NAGGING:          quote = localNagging[persona][randIdx]; break;
          case EVENT_TASK_DUE:         quote = localTaskDue[persona][randIdx]; break;
        }

        String personalQuote = resolveLocalPlaceholders(String(quote), detail);
        xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
        appState.lastResponseIsAi = false;
        appState.aiResponse = personalQuote;
        appState.hasNewAIResponse = true;
        xSemaphoreGive(appState.geminiMutex);
      }
    } else {
      Logger::log("BEHAVIOUR", "AI Query ignored: already loading");
    }
  } else {
    // Local Fallback selection (picks from available per event type and persona)
    const char* quote = "";
    int randIdx = random(5);
    int persona = appConfig.aiPersona;
    if (persona < 0 || persona > 3) persona = 0;

    switch (eventType) {
      case EVENT_FIRST_SIT:     quote = localFirstSit[persona][randIdx]; break;
      case EVENT_WELCOME_BACK:  quote = localWelcomeBack[persona][randIdx]; break;
      case EVENT_STRETCH:       quote = localStretch[persona][randIdx]; break;
      case EVENT_FOCUS_END:     quote = localFocus[persona][randIdx]; break;
      case EVENT_SLACKER:       quote = localSlacker[persona][randIdx]; break;
      case EVENT_STREAK_BEATEN: quote = localStreakBeaten[persona][randIdx]; break;
      case EVENT_LUNCH_REMINDER: quote = localLunchReminder[persona][randIdx]; break;
      case EVENT_EXCESSIVE_BREAKS: quote = localExcessiveBreaks[persona][randIdx]; break;
      case EVENT_GOAL_COMPLETED:   quote = localGoalCompleted[persona][randIdx]; break;
      case EVENT_JOURNAL:          quote = localJournal[persona][randIdx]; break;
      case EVENT_NAGGING:          quote = localNagging[persona][randIdx]; break;
      case EVENT_TASK_DUE:         quote = localTaskDue[persona][randIdx]; break;
    }

    String personalQuote = resolveLocalPlaceholders(String(quote), detail);
    Logger::log("BEHAVIOUR", "Picked local fallback: \"%s\"", personalQuote.c_str());

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = personalQuote;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.geminiMutex);
  }
}

#endif // AI_H
