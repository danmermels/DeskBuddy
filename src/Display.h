#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include "Behaviour.h"
#include "MqttService.h"

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

// Forward declarations for faceplate sprite cleanup
void cleanupDeskbuddySprites();


// Externs for global state variables from main.cpp
extern TFT_eSPI tft;
extern NTPClient timeClient;
extern const int AI_RESPONSE_MAX_CHARS;
extern const int DISPLAY_CHARS_PER_LINE;
extern const RGBColor stateColors[];

// Forward declarations for clock faces (defined in Faceplates.h)
extern void drawMinimalistClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawHiTechClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDefaultClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDevClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawAviatorClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDeskbuddyFaceplate(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);

extern TFT_eSprite hourHandSprite;
extern TFT_eSprite minuteHandSprite;

extern TFT_eSprite secondHandSprite;
extern TFT_eSprite centerBgSprite;

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
        if (overrideColor != 0) {
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
          
          color = (out_r << 11) | (out_g << 5) | out_b;
        }
        tft.pushColor(color, count);
      }
    } else {
      // Raw non-repeating packet
      for (int i = 0; i < count; i++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, 2) == 2) {
          if (overrideColor != 0) {
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
            
            color = (out_r << 11) | (out_g << 5) | out_b;
          }
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

  // 1. Identify screen/page type
  enum PageType { PAGE_STANDARD, PAGE_JOURNAL_DASHBOARD, PAGE_JOURNAL_TASKS, PAGE_DUE_NOW };
  PageType pType = PAGE_STANDARD;
  
  if (appState.lastTriggeredEventType == EVENT_JOURNAL) {
    if (text.indexOf("TODO") != -1) {
      pType = PAGE_JOURNAL_DASHBOARD;
    } else {
      pType = PAGE_JOURNAL_TASKS;
    }
  } else if (appState.lastTriggeredEventType == EVENT_TASK_DUE) {
    pType = PAGE_DUE_NOW;
  }

  // 2. Load layout configurations
  int startY = 45;
  int lineHeight = 28;
  int baseCharsPerLine = DISPLAY_CHARS_PER_LINE;
  const char* titleFont = fontName;
  const char* contentFont = fontName;
  uint16_t titleColorDefault = textColor;
  uint16_t contentColorDefault = textColor;

  if (pType == PAGE_JOURNAL_DASHBOARD) {
    startY = JournalConfig::pageOneTitleY;
    lineHeight = 18;
    baseCharsPerLine = 28;
    titleFont = JournalConfig::pageOneTitleFont;
    contentFont = JournalConfig::pageOneTaskFont;
    titleColorDefault = tft.color565(JournalConfig::pageOneTitleColor.r, JournalConfig::pageOneTitleColor.g, JournalConfig::pageOneTitleColor.b);
    contentColorDefault = tft.color565(JournalConfig::pageOneTaskColor.r, JournalConfig::pageOneTaskColor.g, JournalConfig::pageOneTaskColor.b);
  } else if (pType == PAGE_JOURNAL_TASKS) {
    startY = JournalConfig::tasksTitleY;
    lineHeight = 18;
    baseCharsPerLine = 28;
    titleFont = JournalConfig::tasksTitleFont;
    contentFont = JournalConfig::tasksTaskFont;
    titleColorDefault = tft.color565(JournalConfig::tasksTitleColor.r, JournalConfig::tasksTitleColor.g, JournalConfig::tasksTitleColor.b);
    contentColorDefault = tft.color565(JournalConfig::tasksTaskColor.r, JournalConfig::tasksTaskColor.g, JournalConfig::tasksTaskColor.b);
  } else if (pType == PAGE_DUE_NOW) {
    startY = JournalConfig::dueTitleY;
    lineHeight = 18;
    baseCharsPerLine = 28;
    titleFont = JournalConfig::dueTitleFont;
    contentFont = JournalConfig::dueTitleFont;
    titleColorDefault = tft.color565(JournalConfig::dueTitleColor.r, JournalConfig::dueTitleColor.g, JournalConfig::dueTitleColor.b);
    contentColorDefault = tft.color565(JournalConfig::dueTextColor.r, JournalConfig::dueTextColor.g, JournalConfig::dueTextColor.b);
  }

  tft.setTextDatum(MC_DATUM); // Middle-Center align text
  
  int y = startY;
  String line = "";
  int startIdx = 0;
  int lineIndex = 0;
  
  // Wrap string into lines, dynamically adjusting line capacity for round display tapering
  while (startIdx < text.length()) {
    int yc = y + (lineHeight / 2);              // Center of current line vertically
    int dy = yc - 120;            // Distance from screen center (120)
    float widthAtY = 240.0f;
    if (abs(dy) < 120) {
      widthAtY = 2.0f * sqrt(14400.0f - (float)(dy * dy));
    } else {
      widthAtY = 80.0f;           // Safeguard minimum width
    }
    
    // Scale allowed chars based on relative width (with a minor safety margin of 95%)
    int charsForThisLine = (int)(baseCharsPerLine * (widthAtY / 240.0f) * 0.95f);
    if (charsForThisLine < 8) charsForThisLine = 8; // Guarantee a minimum wrap width

    int nextNewline = text.indexOf('\n', startIdx);
    bool useExplicitNewline = (pType != PAGE_STANDARD);
    if (useExplicitNewline) {
      if (nextNewline != -1) {
        line = text.substring(startIdx, nextNewline);
        startIdx = nextNewline + 1;
      } else {
        line = text.substring(startIdx);
        startIdx = text.length();
      }
    } else {
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
    }
    // Determine font & default color for this line
    const char* activeFont = (lineIndex == 0) ? titleFont : contentFont;
    uint16_t currentLineColor = (lineIndex == 0) ? titleColorDefault : contentColorDefault;
    int currentY = y;

    if (pType == PAGE_DUE_NOW) {
      // Check if this line is the time field (starts with [RED] or is the last line)
      bool isTimeField = (line.indexOf("[RED]") != -1) || (startIdx >= text.length());
      if (lineIndex == 0) {
        activeFont = JournalConfig::dueTitleFont;
        currentLineColor = tft.color565(JournalConfig::dueTitleColor.r, JournalConfig::dueTitleColor.g, JournalConfig::dueTitleColor.b);
      } else if (isTimeField) {
        activeFont = JournalConfig::dueTimeFont;
        currentLineColor = tft.color565(JournalConfig::dueTimeColor.r, JournalConfig::dueTimeColor.g, JournalConfig::dueTimeColor.b);
        currentY = JournalConfig::dueTimeY;
      } else {
        activeFont = contentFont;
        currentLineColor = tft.color565(JournalConfig::dueTextColor.r, JournalConfig::dueTextColor.g, JournalConfig::dueTextColor.b);
      }
    }

    // Load dynamic font if configured
    bool fontLoaded = false;
    if (activeFont != nullptr && strlen(activeFont) > 0) {
      tft.loadFont(activeFont, LittleFS);
      fontLoaded = true;
    }

    // Color code support
    if (line.indexOf("[RED]") != -1) { 
      currentLineColor = tft.color565(JournalConfig::dueTimeColor.r, JournalConfig::dueTimeColor.g, JournalConfig::dueTimeColor.b); 
      line.replace("[RED]", ""); 
    }
    else if (line.indexOf("[GREEN]") != -1) { currentLineColor = tft.color565(34, 197, 94); line.replace("[GREEN]", ""); }
    else if (line.indexOf("[YELLOW]") != -1) { currentLineColor = tft.color565(245, 158, 11); line.replace("[YELLOW]", ""); }
    else if (line.indexOf("[BLUE]") != -1) { currentLineColor = tft.color565(56, 189, 248); line.replace("[BLUE]", ""); }
    else if (line.indexOf("[ORANGE]") != -1) { currentLineColor = tft.color565(249, 115, 22); line.replace("[ORANGE]", ""); }
    else if (line.indexOf("[GREY]") != -1) { currentLineColor = tft.color565(148, 163, 184); line.replace("[GREY]", ""); }
    else if (line.indexOf("[GRAY]") != -1) { currentLineColor = tft.color565(148, 163, 184); line.replace("[GRAY]", ""); }
    else if (line.indexOf("[WHITE]") != -1) { currentLineColor = TFT_WHITE; line.replace("[WHITE]", ""); }

    line.replace("\r", "");
    tft.setTextColor(currentLineColor, bgColor);

    int fontIndex = (pType != PAGE_STANDARD) ? 2 : 4;

    int splitIdx = line.indexOf('|');
    if (splitIdx != -1) {
      String colLeft = line.substring(0, splitIdx);
      String colRight = line.substring(splitIdx + 1);

      uint16_t leftColor = currentLineColor;
      if (colLeft.indexOf("[RED]") != -1) { 
        leftColor = tft.color565(JournalConfig::dueTimeColor.r, JournalConfig::dueTimeColor.g, JournalConfig::dueTimeColor.b); 
        colLeft.replace("[RED]", ""); 
      }
      else if (colLeft.indexOf("[GREEN]") != -1) { leftColor = tft.color565(34, 197, 94); colLeft.replace("[GREEN]", ""); }
      else if (colLeft.indexOf("[YELLOW]") != -1) { leftColor = tft.color565(245, 158, 11); colLeft.replace("[YELLOW]", ""); }
      else if (colLeft.indexOf("[BLUE]") != -1) { leftColor = tft.color565(56, 189, 248); colLeft.replace("[BLUE]", ""); }
      else if (colLeft.indexOf("[ORANGE]") != -1) { leftColor = tft.color565(249, 115, 22); colLeft.replace("[ORANGE]", ""); }
      else if (colLeft.indexOf("[GREY]") != -1) { leftColor = tft.color565(148, 163, 184); colLeft.replace("[GREY]", ""); }
      else if (colLeft.indexOf("[GRAY]") != -1) { leftColor = tft.color565(148, 163, 184); colLeft.replace("[GRAY]", ""); }
      else if (colLeft.indexOf("[WHITE]") != -1) { leftColor = TFT_WHITE; colLeft.replace("[WHITE]", ""); }

      uint16_t rightColor = currentLineColor;
      if (colRight.indexOf("[RED]") != -1) { 
        rightColor = tft.color565(JournalConfig::dueTimeColor.r, JournalConfig::dueTimeColor.g, JournalConfig::dueTimeColor.b); 
        colRight.replace("[RED]", ""); 
      }
      else if (colRight.indexOf("[GREEN]") != -1) { rightColor = tft.color565(34, 197, 94); colRight.replace("[GREEN]", ""); }
      else if (colRight.indexOf("[YELLOW]") != -1) { rightColor = tft.color565(245, 158, 11); colRight.replace("[YELLOW]", ""); }
      else if (colRight.indexOf("[BLUE]") != -1) { rightColor = tft.color565(56, 189, 248); colRight.replace("[BLUE]", ""); }
      else if (colRight.indexOf("[ORANGE]") != -1) { rightColor = tft.color565(249, 115, 22); colRight.replace("[ORANGE]", ""); }
      else if (colRight.indexOf("[GREY]") != -1) { rightColor = tft.color565(148, 163, 184); colRight.replace("[GREY]", ""); }
      else if (colRight.indexOf("[GRAY]") != -1) { rightColor = tft.color565(148, 163, 184); colRight.replace("[GRAY]", ""); }
      else if (colRight.indexOf("[WHITE]") != -1) { rightColor = TFT_WHITE; colRight.replace("[WHITE]", ""); }

      colLeft.trim();
      colRight.trim();

      // Column split coordinates (keep standard 2-column alignments for screen dimensions)
      int leftAlignX = 70;
      int rightAlignX = 78;
      if (colLeft.indexOf("Due Today") != -1) {
        leftAlignX = 115;
        rightAlignX = 125;
      }

      // Draw left column right-aligned at leftAlignX
      tft.setTextColor(leftColor, bgColor);
      tft.setTextDatum(MR_DATUM);
      if (fontLoaded) {
        tft.drawString(colLeft, leftAlignX, currentY);
      } else {
        tft.drawString(colLeft, leftAlignX, currentY, fontIndex);
      }

      // Draw right column left-aligned at rightAlignX
      tft.setTextColor(rightColor, bgColor);
      tft.setTextDatum(ML_DATUM);
      if (fontLoaded) {
        tft.drawString(colRight, rightAlignX, currentY);
      } else {
        tft.drawString(colRight, rightAlignX, currentY, fontIndex);
      }
      
      tft.setTextDatum(MC_DATUM); // Restore default
    } else {
      tft.setTextColor(currentLineColor, bgColor);
      if (fontLoaded) {
        tft.drawString(line, 120, currentY);
      } else {
        tft.drawString(line, 120, currentY, fontIndex);
      }
    }

    if (fontLoaded) {
      tft.unloadFont();
    }

    y += lineHeight;
    lineIndex++;
    if (y > 228) break; // Avoid vertical overflow
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
    Serial.printf("[HEAPS] Free Heap: %u B (%u KB) | Min Free Heap: %u B (%u KB) | Max Alloc: %u B (%u KB)\n", 
                  freeHeap, freeHeap / 1024, minFreeHeap, minFreeHeap / 1024, maxAlloc, maxAlloc / 1024);
    
    if (mqttClient.connected()) {
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"freeHeap\":%u,\"minFreeHeap\":%u}", freeHeap, minFreeHeap);
      mqttClient.publish("deskbuddy/heap", payload);
    }
  }

  static int lastDisplayedPage = -1; // -1 = Away, 0 = Clock, -2 = Alert
  static int lastDisplayedState = -1;
  static int lastClockFace = -1;
  static String activeAlertMessage = "";
  static bool activeAlertIsAi = false;

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
      activeAlertMessage = msg;
      activeAlertIsAi = isAi;
      if (appState.lastTriggeredEventType == EVENT_JOURNAL) {
        int lineCount = 0;
        int idx = 0;
        while ((idx = msg.indexOf('\n', idx)) != -1) {
          lineCount++;
          idx++;
        }
        if (msg.length() > 0 && msg[msg.length() - 1] != '\n') {
          lineCount++;
        }
        bool isPage1 = (msg.indexOf("[YELLOW]TODO") != -1);
        appState.aiScreenEndTime = now + getAlertDurationMs(lineCount, isPage1);
      } else {
        appState.aiScreenEndTime = now + 8000;
      }
      newAlert = true;
      if (appState.lastTriggeredEventType != EVENT_JOURNAL) {
        tftMsgHistory.record(activeAlertMessage.c_str(), appState.lastTriggeredEventType, activeAlertIsAi);
      }
    }
  }

  if (appState.pendingWelcomeAlert && (now - appState.sitDownTime >= WELCOME_HOLD_MS)) {
    appState.pendingWelcomeAlert = false;
    activeAlertMessage = welcomeAlertMessage;
    activeAlertIsAi = welcomeAlertIsAi;
    appState.aiScreenEndTime = now + 8000;
    newAlert = true;
    if (appState.lastTriggeredEventType != EVENT_JOURNAL) {
      tftMsgHistory.record(activeAlertMessage.c_str(), appState.lastTriggeredEventType, activeAlertIsAi);
    }
  }

  bool isAlertActive = (now < appState.aiScreenEndTime);
  int targetPage = -1;
  if (appState.currentPresenceState == STATE_AWAY) {
    appState.pendingWelcomeAlert = false;
    welcomeAlertMessage = "";
    if (appState.hasNewAIResponse) {
      if (appState.lastTriggeredEventType == EVENT_WELCOME_BACK || appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
        appState.hasNewAIResponse = false;
      }
    }
    if (now - appState.lastStateTransitionTime < 60000UL) {
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
    Serial.printf("[TRANSITION] Changing faceplate: %d -> %d\n", lastClockFace, appConfig.clockFace);
    Serial.printf("[TRANSITION] Before Dealloc - Free Heap: %u B | Max Alloc: %u B\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    if (lastClockFace == 4) {
      hourHandSprite.deleteSprite();
      minuteHandSprite.deleteSprite();
      secondHandSprite.deleteSprite();
      centerBgSprite.deleteSprite();
      Serial.println("[SPRITES] Aviator watch hands and center canvas deallocated from RAM.");
    } else if (lastClockFace >= 5 && lastClockFace <= 9) {
      cleanupDeskbuddySprites();
    }

    Serial.printf("[TRANSITION] After Dealloc - Free Heap: %u B | Max Alloc: %u B\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
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

  lastDisplayedState = appState.currentPresenceState;

  // 1. If user is AWAY (and grace period has expired)
  if (appState.currentPresenceState == STATE_AWAY && (now - appState.lastStateTransitionTime >= 60000UL)) {
    if (forceRedraw) {
      drawRLEImage("/away.rle", 0, 0);
    }
    return;
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
