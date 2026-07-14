#ifndef GEMINI_H
#define GEMINI_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Behaviour.h"

// Extern references for global variables and helpers in main.cpp
extern SemaphoreHandle_t geminiMutex;
extern String currentPrompt;
extern volatile bool lastResponseIsAi;
extern int lastTriggeredEventType;
extern String lastTriggeredEventDetail;
extern String aiResponse;
extern volatile bool hasNewAIResponse;
extern String currentUserName;
extern String userName;
extern int aiMode;
extern int dailyAiRequestCount;
extern unsigned long totalDeskTime;
extern unsigned long totalFocusTime;
extern unsigned long totalBreakTime;
extern int breakCount;
extern int productivityScore;
extern volatile bool isAILoading;
extern unsigned long longestSittingStreak;
extern int historyDaysCount;
extern int currentPresenceState;
extern volatile uint32_t currentSitDownSessionId;
extern uint32_t geminiQuerySessionId;

extern String formatTime(unsigned long ms);
inline String resolveLocalPlaceholders(String temp, String detail) {
  temp.replace("{name}", userName);
  if (detail == "") {
    temp.replace("{detail}", "a while");
  } else {
    temp.replace("{detail}", detail);
  }
  if (lastTriggeredEventType == EVENT_FIRST_SIT) {
    temp.replace("{score}", "100");
    temp.replace("{deskTime}", "0m");
    temp.replace("{focusTime}", "0m");
    temp.replace("{breakTime}", "0m");
    temp.replace("{breakCount}", "0");
  } else {
    temp.replace("{score}", String(productivityScore));
    temp.replace("{deskTime}", formatTime(totalDeskTime));
    temp.replace("{focusTime}", formatTime(totalFocusTime));
    temp.replace("{breakTime}", formatTime(totalBreakTime));
    temp.replace("{breakCount}", String(breakCount));
  }
  temp.replace("{longestStreak}", formatTime(longestSittingStreak));
  temp.replace("{historyDays}", String(historyDaysCount));
  return temp;
}

inline String resolvePromptPlaceholders(String temp, String detail) {
  extern const char* PROMPT_PREAMBLE_COACH;
  extern const char* PROMPT_PREAMBLE_CRITIC;
  extern const char* PROMPT_PREAMBLE_NERD;
  extern const char* PROMPT_PREAMBLE_ZEN;
  extern int aiPersona;
  extern int getLearnedWorkdayStart();
  extern int getLearnedWorkdayEnd();
  extern int getLearnedLunchHour();

  const char* activePreamble = PROMPT_PREAMBLE_COACH;
  if (aiPersona == 1) activePreamble = PROMPT_PREAMBLE_CRITIC;
  else if (aiPersona == 2) activePreamble = PROMPT_PREAMBLE_NERD;
  else if (aiPersona == 3) activePreamble = PROMPT_PREAMBLE_ZEN;

  String fullPrompt = String(activePreamble) + "\n\n" + temp;

  fullPrompt.replace("{name}", userName);
  fullPrompt.replace("{detail}", detail);
  if (lastTriggeredEventType == EVENT_FIRST_SIT) {
    fullPrompt.replace("{score}", "100");
    fullPrompt.replace("{deskTime}", "0m");
    fullPrompt.replace("{focusTime}", "0m");
    fullPrompt.replace("{breakTime}", "0m");
    fullPrompt.replace("{breakCount}", "0");
  } else {
    fullPrompt.replace("{score}", String(productivityScore));
    fullPrompt.replace("{deskTime}", formatTime(totalDeskTime));
    fullPrompt.replace("{focusTime}", formatTime(totalFocusTime));
    fullPrompt.replace("{breakTime}", formatTime(totalBreakTime));
    fullPrompt.replace("{breakCount}", String(breakCount));
  }
  fullPrompt.replace("{longestStreak}", formatTime(longestSittingStreak));
  fullPrompt.replace("{historyDays}", String(historyDaysCount));
  
  char startBuf[10], endBuf[10], lunchBuf[10];
  snprintf(startBuf, sizeof(startBuf), "%02d:00", getLearnedWorkdayStart());
  snprintf(endBuf, sizeof(endBuf), "%02d:00", getLearnedWorkdayEnd());
  snprintf(lunchBuf, sizeof(lunchBuf), "%02d:00", getLearnedLunchHour());
  
  fullPrompt.replace("{learnedStart}", String(startBuf));
  fullPrompt.replace("{learnedEnd}", String(endBuf));
  fullPrompt.replace("{learnedLunch}", String(lunchBuf));

  return fullPrompt;
}

// Asynchronous FreeRTOS Task for Gemini HTTPS Queries
inline void queryGeminiTask(void * parameter) {
  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  String prompt = currentPrompt;
  uint32_t querySessionId = geminiQuerySessionId;
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
        
        bool discard = false;
        if (lastTriggeredEventType == EVENT_FIRST_SIT || lastTriggeredEventType == EVENT_WELCOME_BACK) {
          if (querySessionId != currentSitDownSessionId || currentPresenceState == STATE_AWAY) {
            discard = true;
          }
        }
        
        xSemaphoreTake(geminiMutex, portMAX_DELAY);
        if (!discard) {
          lastResponseIsAi = true;
          aiResponse = generatedText;
          hasNewAIResponse = true;
        }
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
      case EVENT_LUNCH_REMINDER: quote = localLunchReminder[randIdx]; break;
      default:                  quote = localWelcomeBack[randIdx]; break;
    }
    
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    lastResponseIsAi = false;
    String nameCopy = currentUserName;
    xSemaphoreGive(geminiMutex);

    String personalQuote = resolveLocalPlaceholders(String(quote), lastTriggeredEventDetail);
    
    bool discard = false;
    if (lastTriggeredEventType == EVENT_FIRST_SIT || lastTriggeredEventType == EVENT_WELCOME_BACK) {
      if (querySessionId != currentSitDownSessionId || currentPresenceState == STATE_AWAY) {
        discard = true;
      }
    }
    
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    if (!discard) {
      aiResponse = personalQuote;
      hasNewAIResponse = true;
    }
    xSemaphoreGive(geminiMutex);
  }
  
  isAILoading = false;
  vTaskDelete(NULL); // One-shot task deletion
}

// Coordinated behaviour trigger: runs background Gemini task or picks local fallback
inline void triggerBehaviour(int eventType, String detail = "", int forceMode = 0) {
  lastTriggeredEventType = eventType;

  xSemaphoreTake(geminiMutex, portMAX_DELAY);
  lastTriggeredEventDetail = detail;
  currentUserName = userName;
  xSemaphoreGive(geminiMutex);

  bool useAI = false;
  if (forceMode == 1) {
    useAI = true;
  } else if (forceMode == 2) {
    useAI = false;
  } else {
    if (aiMode == 2) {
      // Frequent mode: all events can trigger AI
      useAI = true;
    } else if (aiMode == 1) {
      // Balanced mode: AI triggers for FIRST_SIT, STRETCH, WELCOME_BACK, and LUNCH_REMINDER
      if (eventType == EVENT_FIRST_SIT || eventType == EVENT_STRETCH || eventType == EVENT_WELCOME_BACK || eventType == EVENT_LUNCH_REMINDER) {
        useAI = true;
      }
    }
    // Enforce daily cap (max 15 requests per day) for normal triggers
    if (useAI && dailyAiRequestCount >= 15) {
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

    if (!isAILoading) {
      if (forceMode != 1) {
        dailyAiRequestCount++;
      }
      xSemaphoreTake(geminiMutex, portMAX_DELAY);
      currentPrompt = basePrompt;
      geminiQuerySessionId = currentSitDownSessionId;
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
      case EVENT_LUNCH_REMINDER: quote = localLunchReminder[randIdx]; break;
    }

    String personalQuote = resolveLocalPlaceholders(String(quote), detail);

    // Immediately post fallback quote to display thread-safely
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    lastResponseIsAi = false;
    aiResponse = personalQuote;
    hasNewAIResponse = true;
    xSemaphoreGive(geminiMutex);
  }
}

#endif // GEMINI_H
