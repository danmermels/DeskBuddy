#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include "Behaviour.h"
#include "MqttService.h"
#include "MessageManager.h"

// State definitions if not already declared
#ifndef STATE_AWAY
#define STATE_AWAY        0
#define STATE_FOCUS       1
#define STATE_BUSY        2
#define STATE_DISTRACTED  3
#define STATE_REGULAR     4
#endif

// RGB Color structure matching main.cpp
#ifndef RGB_COLOR_STRUCT
#define RGB_COLOR_STRUCT
struct RGBColor {
  uint8_t r, g, b;
  bool operator==(const RGBColor& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const RGBColor& o) const { return !(*this == o); }
};
#endif

#include "State.h"

#include "Audio.h"

// Forward declarations for faceplate sprite cleanup
void cleanupDeskbuddySprites();


// Externs for global state variables from main.cpp
extern TFT_eSPI tft;
extern NTPClient timeClient;
extern const int AI_RESPONSE_MAX_CHARS;
extern const int DISPLAY_CHARS_PER_LINE;
extern const RGBColor stateColors[];
extern MessageManager messageManager;

// Forward declarations for clock faces (defined in Faceplates.h)
extern void drawMinimalistClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawHiTechClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDefaultClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDevClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawAviatorClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDeskbuddyFaceplate(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);

// Forward declarations for event faceplates (defined in Faceplates.h)
extern void drawTaskDueFaceplate(const TaskDueViewData& data, uint16_t bgColor = TFT_BLACK, const char* fontName = nullptr);
extern void drawJournalDashboardFaceplate(const JournalDashboardViewData& data, uint16_t bgColor = TFT_BLACK, const char* titleFont = nullptr, const char* labelFont = nullptr);
extern void drawJournalTasksFaceplate(const String& rawTaskList, uint16_t bgColor = TFT_BLACK, const char* fontName = nullptr, int maxLines = 12, int lineHeight = 16);

extern TFT_eSprite hourHandSprite;
extern TFT_eSprite minuteHandSprite;

extern TFT_eSprite secondHandSprite;
extern TFT_eSprite centerBgSprite;

// Forward declaration used by drawRLEImage
inline uint16_t applyColorTint(uint16_t color, uint16_t overrideColor);

// Draw custom PackBits-RLE compressed image from LittleFS to TFT
inline bool drawRLEImage(const char* filename, int16_t x, int16_t y, uint16_t overrideColor = 0) {
  appStats.fsReadCount++;
  fs::File file = LittleFS.open(filename, "r");
  if (!file) return false;

  uint16_t w, h;
  if (file.read((uint8_t*)&w, 2) != 2 || file.read((uint8_t*)&h, 2) != 2) {
    file.close();
    return false;
  }

  tft.setAddrWindow(x, y, w, h);

  while (file.available() > 0) {
    uint8_t header = file.read();
    uint8_t count = (header & 0x7F) + 1;
    if (header & 0x80) {
      // Repeating run packet
      uint16_t color;
      if (file.read((uint8_t*)&color, 2) == 2) {
        color = applyColorTint(color, overrideColor);
        tft.pushColor(color, count);
      }
    } else {
      // Raw non-repeating packet
      for (int i = 0; i < count; i++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, 2) == 2) {
          color = applyColorTint(color, overrideColor);
          tft.pushColor(color, 1);
        }
      }
    }
  }
  file.close();
  return true;
}

// Helper to apply overrideColor tinting based on pixel luminance
inline uint16_t applyColorTint(uint16_t color, uint16_t overrideColor) {
  if (overrideColor == 0) return color;
  uint16_t g_5 = (color >> 6) & 0x1F;
  uint16_t b_5 = color & 0x1F;
  uint16_t val = (g_5 > b_5) ? g_5 : b_5;
  
  uint16_t target_r = (overrideColor >> 11) & 0x1F;
  uint16_t target_g = (overrideColor >> 5) & 0x3F;
  uint16_t target_b = overrideColor & 0x1F;
  
  uint16_t out_r = (val * target_r) / 21;
  uint16_t out_g = (val * target_g) / 21;
  uint16_t out_b = (val * target_b) / 21;
  
  if (out_r > target_r) out_r = target_r;
  if (out_g > target_g) out_g = target_g;
  if (out_b > target_b) out_b = target_b;
  
  return (out_r << 11) | (out_g << 5) | out_b;
}

// Decode RLE image crop slice into a TFT_eSprite
inline bool drawRLEImageToSprite(TFT_eSprite &spr, const char* filename, int16_t cropX, int16_t cropY, int16_t cropW, int16_t cropH, uint16_t overrideColor = 0) {
  appStats.fsReadCount++;
  fs::File file = LittleFS.open(filename, "r");
  if (!file) return false;

  uint16_t imgW, imgH;
  if (file.read((uint8_t*)&imgW, 2) != 2 || file.read((uint8_t*)&imgH, 2) != 2) {
    file.close();
    return false;
  }

  int16_t curX = 0;
  int16_t curY = 0;

  while (file.available() > 0) {
    uint8_t header = file.read();
    uint8_t count = (header & 0x7F) + 1;
    if (header & 0x80) {
      uint16_t color;
      if (file.read((uint8_t*)&color, 2) == 2) {
        if (overrideColor != 0) color = applyColorTint(color, overrideColor);
        for (int i = 0; i < count; i++) {
          if (curX >= cropX && curX < cropX + cropW && curY >= cropY && curY < cropY + cropH) {
            spr.drawPixel(curX - cropX, curY - cropY, color);
          }
          curX++;
          if (curX >= imgW) {
            curX = 0;
            curY++;
          }
        }
      }
    } else {
      for (int i = 0; i < count; i++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, 2) == 2) {
          if (overrideColor != 0) color = applyColorTint(color, overrideColor);
          if (curX >= cropX && curX < cropX + cropW && curY >= cropY && curY < cropY + cropH) {
            spr.drawPixel(curX - cropX, curY - cropY, color);
          }
          curX++;
          if (curX >= imgW) {
            curX = 0;
            curY++;
          }
        }
      }
    }
  }
  file.close();
  return true;
}

// Decode full RLE image directly into a TFT_eSprite buffer (for watch hands, etc.)
inline bool drawFullRLEToSprite(TFT_eSprite &spr, const char* filename, uint16_t overrideColor = 0) {
  appStats.fsReadCount++;
  fs::File file = LittleFS.open(filename, "r");
  if (!file) return false;

  uint16_t w, h;
  if (file.read((uint8_t*)&w, 2) != 2 || file.read((uint8_t*)&h, 2) != 2) {
    file.close();
    return false;
  }

  int16_t curX = 0;
  int16_t curY = 0;

  while (file.available() > 0) {
    uint8_t header = file.read();
    uint8_t count = (header & 0x7F) + 1;
    if (header & 0x80) {
      uint16_t color;
      if (file.read((uint8_t*)&color, 2) == 2) {
        if (overrideColor != 0) color = applyColorTint(color, overrideColor);
        for (int i = 0; i < count; i++) {
          spr.drawPixel(curX, curY, color);
          curX++;
          if (curX >= w) {
            curX = 0;
            curY++;
          }
        }
      }
    } else {
      for (int i = 0; i < count; i++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, 2) == 2) {
          if (overrideColor != 0) color = applyColorTint(color, overrideColor);
          spr.drawPixel(curX, curY, color);
          curX++;
          if (curX >= w) {
            curX = 0;
            curY++;
          }
        }
      }
    }
  }
  file.close();
  return true;
}

// Helper to draw auto-wrapped text in the center of the round TFT
inline void drawFaceplateMessage(const char* bgImage, String text, uint16_t textColor, const char* fontName, bool isAi, uint16_t aiLabelColor = TFT_LIGHTGREY, uint16_t bgColor = TFT_BLACK) {
  if (bgImage != nullptr) {
    if (!drawRLEImage(bgImage, 0, 0)) {
      tft.fillScreen(bgColor);
    }
  } else {
    tft.fillScreen(bgColor);
  }

  // Route specialized event faceplates directly to Faceplates.h functions
  if (appState.lastTriggeredEventType == EVENT_JOURNAL || appState.lastTriggeredEventType == EVENT_PAGE) {
    if (text.indexOf("TODO") != -1 || text.indexOf("RESUMO") != -1 || text.indexOf("SUMMARY") != -1) {
      drawJournalDashboardFaceplate(getJournalDashboardData(), bgColor, fontName, fontName);
      return;
    } else {
      drawJournalTasksFaceplate(text, bgColor, fontName);
      return;
    }
  } else if (appState.lastTriggeredEventType == EVENT_TASK_DUE) {
    TaskDueViewData viewData;
    viewData.isOverdue = (text.indexOf("OVERDUE") != -1 || text.indexOf("ATRASADO") != -1);
    
    int pipeIdx = text.indexOf('|');
    if (pipeIdx != -1) {
      viewData.taskText = text.substring(0, pipeIdx);
      viewData.taskText.trim();
      viewData.dueTimeStr = text.substring(pipeIdx + 1);
      viewData.dueTimeStr.trim();
    } else {
      int n1 = text.indexOf('\n');
      if (n1 != -1) {
        int n2 = text.indexOf('\n', n1 + 1);
        if (n2 != -1) {
          viewData.taskText = text.substring(n1 + 1, n2);
          viewData.taskText.trim();
          if (viewData.taskText.startsWith("- ")) viewData.taskText = viewData.taskText.substring(2);
          viewData.dueTimeStr = text.substring(n2 + 1);
          viewData.dueTimeStr.trim();
        } else {
          viewData.taskText = text.substring(n1 + 1);
          viewData.taskText.trim();
          if (viewData.taskText.startsWith("- ")) viewData.taskText = viewData.taskText.substring(2);
          viewData.dueTimeStr = "";
        }
      } else {
        viewData.taskText = text;
        viewData.dueTimeStr = "";
      }
    }
    
    drawTaskDueFaceplate(viewData, bgColor, fontName);
    return;
  }

  // Standard AI message rendering (nagging, points, general — no special title headers)
  int startY = 45;
  int lineHeight = 28;
  int baseCharsPerLine = DISPLAY_CHARS_PER_LINE;

  tft.setTextDatum(MC_DATUM);
  int y = startY;
  String line = "";
  int startIdx = 0;

  while (startIdx < text.length()) {
    int yc = y + (lineHeight / 2);
    int dy = yc - 120;
    float widthAtY = (abs(dy) < 120) ? 2.0f * sqrt(14400.0f - (float)(dy * dy)) : 80.0f;
    
    int charsForThisLine = (int)(baseCharsPerLine * (widthAtY / 240.0f) * 0.95f);
    if (charsForThisLine < 8) charsForThisLine = 8;

    int nextNewline = text.indexOf('\n', startIdx);
    int endIdx = startIdx + charsForThisLine;
    if (nextNewline != -1 && nextNewline < endIdx) {
      line = text.substring(startIdx, nextNewline);
      startIdx = nextNewline + 1;
    } else if (endIdx >= text.length()) {
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

    bool fontLoaded = false;
    if (fontName != nullptr && strlen(fontName) > 0 && LittleFS.exists("/" + String(fontName) + ".vlw")) {
      tft.loadFont(fontName, LittleFS);
      fontLoaded = true;
    }

    line.replace("\r", "");
    uint16_t currentLineColor = textColor;
    if (line.indexOf("[RED]") != -1)         { currentLineColor = tft.color565(239, 68, 68); line.replace("[RED]", ""); }
    else if (line.indexOf("[GREEN]") != -1)  { currentLineColor = tft.color565(34, 197, 94); line.replace("[GREEN]", ""); }
    else if (line.indexOf("[YELLOW]") != -1) { currentLineColor = tft.color565(245, 158, 11); line.replace("[YELLOW]", ""); }
    else if (line.indexOf("[BLUE]") != -1)   { currentLineColor = tft.color565(56, 189, 248); line.replace("[BLUE]", ""); }
    else if (line.indexOf("[ORANGE]") != -1) { currentLineColor = tft.color565(249, 115, 22); line.replace("[ORANGE]", ""); }
    else if (line.indexOf("[GREY]") != -1)   { currentLineColor = tft.color565(148, 163, 184); line.replace("[GREY]", ""); }
    else if (line.indexOf("[GRAY]") != -1)   { currentLineColor = tft.color565(148, 163, 184); line.replace("[GRAY]", ""); }
    else if (line.indexOf("[WHITE]") != -1)  { currentLineColor = TFT_WHITE; line.replace("[WHITE]", ""); }

    tft.setTextColor(currentLineColor, bgColor);
    if (fontLoaded) {
      tft.drawString(line, 120, y);
      tft.unloadFont();
    } else {
      tft.drawString(line, 120, y, 2); // Built-in System Font 2 (16px)
    }

    y += lineHeight;
    if (y > 228) break;
  }

  if (isAi) {
    tft.setTextColor(tft.color565(45, 152, 200), tft.color565(15, 23, 42));
    tft.drawString("AI GENERATED", 120, 210, 2);
  }
}

inline void updateTFTDisplay(unsigned long now) {
  // -- 1-Minute Heap & Memory telemetry logger (runs under all modes) ----------
  static unsigned long lastHeapLogTime = 0;
  if (now - lastHeapLogTime >= 60000UL) {
    lastHeapLogTime = now;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t maxAlloc = ESP.getMaxAllocHeap();
#if DESKBUDDY_DEBUG
    Serial.printf("[HEAPS] Free Heap: %u B (%u KB) | Min Free Heap: %u B (%u KB) | Max Alloc: %u B (%u KB)\n", 
                  freeHeap, freeHeap / 1024, minFreeHeap, minFreeHeap / 1024, maxAlloc, maxAlloc / 1024);
#endif
    
    if (mqttClient.connected()) {
#if DESKBUDDY_DEBUG
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"freeHeap\":%u,\"minFreeHeap\":%u}", freeHeap, minFreeHeap);
      mqttClient.publish("deskbuddy/heap", payload);
#endif
    }
  }

  static int lastDisplayedPage = -1; // -1 = Away, 0 = Clock, -2 = Alert
  static int lastClockFace = -1;
  static String activeAlertMessage = "";
  static bool activeAlertIsAi = false;
  static bool timerWasDrawn = false;

  static String welcomeAlertMessage = "";
  static bool welcomeAlertIsAi = false;

  bool newAlert = false;
  if (appState.hasNewAIResponse) {
    xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
    String msg = appState.aiResponse;
    bool isAi = appState.lastResponseIsAi;
    appState.hasNewAIResponse = false;
    xSemaphoreGive(appState.aiMutex);

    // Publish to MQTT immediately when any message is triggered
    publishMqttMessage(msg);

    if (appState.lastTriggeredEventType == EVENT_WELCOME_BACK || appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
      appState.pendingWelcomeAlert = true;
      welcomeAlertMessage = msg;
      welcomeAlertIsAi = isAi;
    } else {
      // F12: split oversized messages into a follow-up 2nd screen (EVENT_PAGE). Runs after the
      // MQTT echo above so the full text is still published. JOURNAL/TASK_DUE have their own layouts.
      int lineCount = 1;
      for (int i = 0; i < msg.length(); i++) {
        if (msg[i] == '\n') lineCount++;
      }
      bool isPage1 = (appState.lastTriggeredEventType == EVENT_JOURNAL && (msg.indexOf("[YELLOW]TODO") != -1 || msg.indexOf("[YELLOW]RESUMO") != -1));
      unsigned long durationMs = getAlertDurationMs(lineCount, isPage1);

      int curEvent = appState.lastTriggeredEventType;
      if (curEvent != EVENT_JOURNAL && curEvent != EVENT_TASK_DUE && msg.length() > MSG_PAGE_MAX_CHARS) {
        int splitAt = msg.lastIndexOf(' ', MSG_PAGE_MAX_CHARS);
        if (splitAt > 20) {
          String rest = msg.substring(splitAt + 1);
          msg = msg.substring(0, splitAt);
          messageManager.scheduleMessageWithPriority(
            EVENT_PAGE,
            rest,
            MessageManager::P_HIGH, durationMs, MessageManager::R_NORMAL
          );
          appState.journalSequenceActive = true;
          Logger::log("DISPLAY", "Message split into 2nd screen (p1=%d, p2=%d chars)", msg.length(), rest.length());
        }
      }
      activeAlertMessage = msg;
      activeAlertIsAi = isAi;
      appState.aiScreenEndTime = millis() + durationMs;
      newAlert = true;
      if (audioEnabled) playKnock();
      if (appState.lastTriggeredEventType != EVENT_JOURNAL && appState.lastTriggeredEventType != EVENT_TASK_DUE) {
        tftMsgHistory.record(activeAlertMessage.c_str(), appState.lastTriggeredEventType, activeAlertIsAi);
      }
    }
  }

  if (appState.pendingWelcomeAlert && (now - appState.sitDownTime >= WELCOME_HOLD_MS)) {
    appState.pendingWelcomeAlert = false;
    activeAlertMessage = welcomeAlertMessage;
    activeAlertIsAi = welcomeAlertIsAi;
    int welcomeLines = 1;
    for (int i = 0; i < welcomeAlertMessage.length(); i++) {
      if (welcomeAlertMessage[i] == '\n') welcomeLines++;
    }
    appState.aiScreenEndTime = millis() + getAlertDurationMs(welcomeLines, false);
    newAlert = true;
    if (audioEnabled) playKnock();
    if (appState.lastTriggeredEventType != EVENT_JOURNAL && appState.lastTriggeredEventType != EVENT_TASK_DUE) {
      tftMsgHistory.record(activeAlertMessage.c_str(), appState.lastTriggeredEventType, activeAlertIsAi);
    }
  }

  // Seamless Multi-Page Sequence Handover: if current page expired but a follow-up page is queued,
  // dequeue and display it immediately without dropping to clock faceplate.
  if (millis() >= appState.aiScreenEndTime && appState.journalSequenceActive) {
    MessageManager::DueMessage seqMsg = messageManager.getNextDueMessage(true);
    if (seqMsg.eventType != -1) {
      appState.lastTriggeredEventType = seqMsg.eventType;
      String msg = seqMsg.content;
      
      // If receiving serialized multi-page payload in sequence
      if (seqMsg.eventType == EVENT_JOURNAL && msg.startsWith("PAGE:")) {
        int pipe1 = msg.indexOf('|');
        int pipe2 = (pipe1 != -1) ? msg.indexOf('|', pipe1 + 1) : -1;
        if (pipe1 != -1 && pipe2 != -1) {
          int pageIdx = msg.substring(5, pipe1).toInt();
          int pLines = msg.substring(pipe1 + 1, pipe2).toInt();
          int separatorIdx = msg.indexOf("|||", pipe2 + 1);
          String pageContent = (separatorIdx != -1) ? msg.substring(pipe2 + 1, separatorIdx) : msg.substring(pipe2 + 1);
          String nextPages = (separatorIdx != -1) ? msg.substring(separatorIdx + 3) : "";

          activeAlertMessage = pageContent;
          activeAlertIsAi = false;
          appState.aiScreenEndTime = millis() + getAlertDurationMs(pLines, false);
          newAlert = true;
          if (audioEnabled) playKnock();

          if (nextPages.length() > 0) {
            unsigned long dur = getAlertDurationMs(pLines, false);
            messageManager.scheduleMessageWithPriority(
              EVENT_JOURNAL, nextPages, MessageManager::P_HIGH, dur, MessageManager::R_NORMAL
            );
          } else {
            appState.journalSequenceActive = false;
          }
        }
      } else {
        int lineCount = 1;
        for (int i = 0; i < msg.length(); i++) {
          if (msg[i] == '\n') lineCount++;
        }
        bool isPage1 = (seqMsg.eventType == EVENT_JOURNAL && (msg.indexOf("[YELLOW]TODO") != -1 || msg.indexOf("[YELLOW]RESUMO") != -1));
        unsigned long durationMs = getAlertDurationMs(lineCount, isPage1);

        activeAlertMessage = msg;
        activeAlertIsAi = false;
        appState.aiScreenEndTime = millis() + durationMs;
        newAlert = true;
        if (audioEnabled) playKnock();
      }
    } else {
      appState.journalSequenceActive = false;
    }
  }

  bool isAlertActive = (millis() < appState.aiScreenEndTime);
  if (appState.manualTriggerOverride && !isAlertActive && !appState.isAILoading && !appState.hasNewAIResponse) {
    appState.manualTriggerOverride = false;
  }

  // Timer overlay page: owns the screen while a stopwatch/countdown runs (or during the
  // post-reset 3s / paused 10s hold). Alerts temporarily take priority; the timer
  // returns when they end, and a paused/done timer releases to the faceplate after its hold.
  timerTick(now);
  bool timerVisible = timerShouldDraw(now) && !isAlertActive;
  if (timerVisible) {
    bool wasDrawn = timerWasDrawn;
    timerWasDrawn = true;
    drawTimerOverlay(now, !wasDrawn);
    return;
  }
  if (timerWasDrawn) {
    timerWasDrawn = false;
    lastDisplayedPage = -99; // force a full redraw of the normal page next iteration
  }

  int targetPage = -1;
  if (appState.currentPresenceState == STATE_AWAY) {
    appState.pendingWelcomeAlert = false;
    welcomeAlertMessage = "";
    if (appState.hasNewAIResponse && !appState.manualTriggerOverride) {
      if (appState.lastTriggeredEventType == EVENT_WELCOME_BACK || appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
        appState.hasNewAIResponse = false;
      }
    }
    if (appState.manualTriggerOverride && isAlertActive) {
      targetPage = -2; // Manual MQTT trigger: force the alert even while away
    } else if (now - appState.lastStateTransitionTime < 60000UL) {
      targetPage = 0; // Show Clock page during the 1-minute grace period
    } else {
      targetPage = -1; // Away page
    }
  } else if (isAlertActive) {
    targetPage = -2; // Alert page
  } else {
    targetPage = 0;  // Clock page
  }

  static bool lastTime24h = true;

  bool forceRedraw = (targetPage != lastDisplayedPage) || 
                     (appConfig.clockFace != lastClockFace) || 
                     (appConfig.time24h != lastTime24h) ||
                     newAlert;

  if (appConfig.clockFace != lastClockFace) {
    tft.fillScreen(TFT_BLACK);
#if DESKBUDDY_DEBUG
    Serial.printf("[TRANSITION] Changing faceplate: %d -> %d\n", lastClockFace, appConfig.clockFace);
    Serial.printf("[TRANSITION] Before Dealloc - Free Heap: %u B | Max Alloc: %u B\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif

    if (lastClockFace == 4) {
      hourHandSprite.deleteSprite();
      minuteHandSprite.deleteSprite();
      secondHandSprite.deleteSprite();
      centerBgSprite.deleteSprite();
#if DESKBUDDY_DEBUG
      Serial.println("[SPRITES] Aviator watch hands and center canvas deallocated from RAM.");
#endif
    } else if (lastClockFace >= 5 && lastClockFace <= 9) {
      cleanupDeskbuddySprites();
    }

#if DESKBUDDY_DEBUG
    Serial.printf("[TRANSITION] After Dealloc - Free Heap: %u B | Max Alloc: %u B\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif
    lastClockFace = appConfig.clockFace;
  }

  if (appConfig.time24h != lastTime24h) {
    lastTime24h = appConfig.time24h;
    tft.fillScreen(TFT_BLACK);
  }

  if (targetPage != lastDisplayedPage) {
    tft.fillScreen(TFT_BLACK);
    lastDisplayedPage = targetPage;
  }

  // 1. If user is AWAY (and grace period has expired) - DEV Mode (clockFace 3) bypasses this to show continuous debug telemetry
  if (appConfig.clockFace != 3 && appState.currentPresenceState == STATE_AWAY && (now - appState.lastStateTransitionTime >= 60000UL)) {
    if (!appState.manualTriggerOverride || !isAlertActive) {
      if (forceRedraw) {
        drawRLEImage("/away.rle", 0, 0);
      }
      return;
    }
  }



  // 2. Call active faceplate
  bool wifiAvailable = (WiFi.status() == WL_CONNECTED);
  bool internetAvailable = wifiAvailable && timeClient.isTimeSet();

  switch (appConfig.clockFace) {
    case 1:
      drawMinimalistClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 2:
      drawHiTechClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 3:
      drawDevClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 4:
      drawAviatorClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      drawDeskbuddyFaceplate(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 0:
    default:
      drawDefaultClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
  }
}

#endif // DISPLAY_H
