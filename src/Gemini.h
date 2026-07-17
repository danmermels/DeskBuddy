#ifndef GEMINI_H
#define GEMINI_H

#include <Arduino.h>
#include <ESP32_AI_Connect.h>
#include "Behaviour.h"

#include "State.h"
#include <NTPClient.h>

extern NTPClient timeClient;
extern volatile uint32_t currentSitDownSessionId;
extern uint32_t geminiQuerySessionId;

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

inline String resolvePromptPlaceholders(String templateStr, String detail) {
  extern const char* PROMPT_PREAMBLE_COACH;
  extern const char* PROMPT_PREAMBLE_CRITIC;
  extern const char* PROMPT_PREAMBLE_NERD;
  extern const char* PROMPT_PREAMBLE_ZEN;
    extern int getLearnedWorkdayStart(int dayIndex);
  extern int getLearnedWorkdayEnd(int dayIndex);
  extern int getLearnedLunchHour(int dayIndex);

  const char* activePreamble = PROMPT_PREAMBLE_COACH;
  if (appConfig.aiPersona == 1) activePreamble = PROMPT_PREAMBLE_CRITIC;
  else if (appConfig.aiPersona == 2) activePreamble = PROMPT_PREAMBLE_NERD;
  else if (appConfig.aiPersona == 3) activePreamble = PROMPT_PREAMBLE_ZEN;

  String fullPrompt = String(activePreamble) + "\n\n" + appState.temp;

  fullPrompt.replace("{name}", appConfig.userName);
  if (detail == "") {
    fullPrompt.replace("{detail}", "a while");
  } else {
    fullPrompt.replace("{detail}", detail);
  }
  fullPrompt.replace("{score}", String(appStats.productivityScore));
  fullPrompt.replace("{deskTime}", formatTime(appStats.totalDeskTime));
  fullPrompt.replace("{focusTime}", formatTime(appStats.totalFocusTime));
  fullPrompt.replace("{breakTime}", formatTime(appStats.totalBreakTime));
  fullPrompt.replace("{breakCount}", String(appStats.breakCount));
  fullPrompt.replace("{longestStreak}", formatTime(appStats.longestSittingStreak));
  int currentDay = timeClient.isTimeSet() ? timeClient.getDay() : 1;
  fullPrompt.replace("{historyDays}", String(appStats.historyDaysCountWeekly[currentDay]));
  
  char startBuf[10], endBuf[10], lunchBuf[10];
  snprintf(startBuf, sizeof(startBuf), "%02d:00", getLearnedWorkdayStart(currentDay));
  snprintf(endBuf, sizeof(endBuf), "%02d:00", getLearnedWorkdayEnd(currentDay));
  snprintf(lunchBuf, sizeof(lunchBuf), "%02d:00", getLearnedLunchHour(currentDay));
  
  fullPrompt.replace("{learnedStart}", String(startBuf));
  fullPrompt.replace("{learnedEnd}", String(endBuf));
  fullPrompt.replace("{learnedLunch}", String(lunchBuf));

  return fullPrompt;
}

// Asynchronous FreeRTOS Task for Gemini HTTPS Queries
inline void queryGeminiTask(void * parameter) {
  xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
  String prompt = appState.currentPrompt;
  uint32_t querySessionId = geminiQuerySessionId;
  xSemaphoreGive(appState.geminiMutex);

  bool success = false;
  
  static ESP32_AI_Connect ai("openai-compatible", appConfig.groqApiKey.c_str(), "llama-3.3-70b-versatile", "https://api.groq.com/openai/v1/chat/completions");
  ai.setChatTemperature(0.5);
  ai.setChatMaxTokens(AI_RESPONSE_MAX_CHARS * 2 + 10);
  
  String response = ai.chat(prompt);
  int httpCode = ai.getChatResponseCode();
  
  if (httpCode == 200 && response.length() > 0) {
    response.trim();
    
    if (response.startsWith("\"") && response.endsWith("\"")) {
      response = response.substring(1, response.length() - 1);
    }
    
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
    }
    xSemaphoreGive(appState.geminiMutex);
    success = true;
  }

  // Graceful fallback: If Gemini query fails, load a local fallback quote immediately
  if (!success) {
    const char* quote = "";
    int randIdx = random(20);
    switch (appState.lastTriggeredEventType) {
      case EVENT_FIRST_SIT:     quote = localFirstSit[randIdx]; break;
      case EVENT_WELCOME_BACK:  quote = localWelcomeBack[randIdx]; break;
      case EVENT_STRETCH:       quote = localStretch[randIdx]; break;
      case EVENT_FOCUS_END:     quote = localFocus[randIdx]; break;
      case EVENT_SLACKER:       quote = localSlacker[randIdx]; break;
      case EVENT_STREAK_BEATEN: quote = localStreakBeaten[randIdx]; break;
      case EVENT_LUNCH_REMINDER: quote = localLunchReminder[randIdx]; break;
      default:                  quote = localWelcomeBack[randIdx]; break;
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
    }
    xSemaphoreGive(appState.geminiMutex);
  }
  
  appState.isAILoading = false;
  vTaskDelete(NULL); // One-shot task deletion
}

// Coordinated behaviour trigger: runs background Gemini task or picks local fallback
inline void triggerBehaviour(int eventType, String detail = "", int forceMode = 0) {
  appState.lastTriggeredEventType = eventType;

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
      // Balanced mode: AI triggers for FIRST_SIT, STRETCH, WELCOME_BACK, and LUNCH_REMINDER
      if (eventType == EVENT_FIRST_SIT || eventType == EVENT_STRETCH || eventType == EVENT_WELCOME_BACK || eventType == EVENT_LUNCH_REMINDER) {
        useAI = true;
      }
    }
    // Enforce daily cap (max 15 requests per day) for normal triggers
    if (useAI && appStats.dailyAiRequestCount >= 15) {
      useAI = false;
    }
  }

  if (useAI) {
    String basePrompt = "";
    switch (eventType) {
      case EVENT_FIRST_SIT:     basePrompt = resolvePromptPlaceholders(PROMPT_FIRST_SIT_OF_DAY, detail); break;
      case EVENT_WELCOME_BACK:  basePrompt = resolvePromptPlaceholders(PROMPT_WELCOME_BACK, detail); break;
      case EVENT_STRETCH:       basePrompt = resolvePromptPlaceholders(PROMPT_STRETCH_REMINDER, detail); break;
      case EVENT_FOCUS_END:     basePrompt = resolvePromptPlaceholders(PROMPT_FOCUS_CONGRATS, detail); break;
      case EVENT_SLACKER:       basePrompt = resolvePromptPlaceholders(PROMPT_SLACKER_ROAST, detail); break;
      case EVENT_STREAK_BEATEN: basePrompt = resolvePromptPlaceholders(PROMPT_STREAK_BEATEN, detail); break;
      case EVENT_LUNCH_REMINDER: basePrompt = resolvePromptPlaceholders(PROMPT_LUNCH_REMINDER, detail); break;
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
      xTaskCreate(
        queryGeminiTask,
        "GeminiQuery",
        12288,
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
      case EVENT_LUNCH_REMINDER: quote = localLunchReminder[randIdx]; break;
    }

    String personalQuote = resolveLocalPlaceholders(String(quote), detail);

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
    appState.lastResponseIsAi = false;
    appState.aiResponse = personalQuote;
    appState.hasNewAIResponse = true;
    xSemaphoreGive(appState.geminiMutex);
  }
}

#endif // GEMINI_H
