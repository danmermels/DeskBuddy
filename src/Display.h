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

// Externs for global state variables from main.cpp
extern TFT_eSPI tft;
extern int clockFace;
extern int currentPresenceState;
extern bool hasMail;
extern unsigned long lastStateTransitionTime;
extern unsigned long sitDownTime;
extern unsigned long aiScreenEndTime;
extern volatile bool hasNewAIResponse;
extern SemaphoreHandle_t geminiMutex;
extern String aiResponse;
extern volatile bool lastResponseIsAi;
extern volatile bool isAILoading;
extern int lastTriggeredEventType;
extern NTPClient timeClient;
extern bool time24h;
extern uint32_t fsReadCount;
extern uint32_t fsWriteCount;

extern const RGBColor stateColors[];
extern RGBColor currentRingColor;
extern RGBColor startRingColor;
extern RGBColor targetRingColor;
extern unsigned long ringTransitionStart;
extern const unsigned long ringTransitionDuration;

// Forward declarations for clock faces (defined in Faceplates.h)
extern void drawMinimalistClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawHiTechClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDefaultClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);
extern void drawDevClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail);

// Draw custom PackBits-RLE compressed image from LittleFS to TFT
inline bool drawRLEImage(const char* filename, int16_t x, int16_t y, uint16_t overrideColor = 0) {
  fsReadCount++;
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

// Helper to draw auto-wrapped text in the center of the round TFT
inline void drawFaceplateMessage(const char* bgImage, String text, uint16_t textColor, bool isAi, uint16_t aiLabelColor = TFT_LIGHTGREY) {
  if (bgImage != nullptr) {
    if (!drawRLEImage(bgImage, 0, 0)) {
      tft.fillScreen(TFT_BLACK);
    }
  } else {
    tft.fillScreen(TFT_BLACK);
  }

  tft.setTextColor(textColor);
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
    tft.setTextColor(aiLabelColor);
    tft.drawString("(AI GENERATED)", 120, 210, 2);
  }
}

inline void updateTFTDisplay(unsigned long now) {
  static int lastDisplayedPage = -1; // -1 = Away, 0 = Clock, -2 = Alert
  static int lastDisplayedState = -1;
  static int lastClockFace = -1;
  static String activeAlertMessage = "";
  static bool activeAlertIsAi = false;

  static bool pendingWelcomeAlert = false;
  static String welcomeAlertMessage = "";
  static bool welcomeAlertIsAi = false;

  bool newAlert = false;
  if (hasNewAIResponse) {
    xSemaphoreTake(geminiMutex, portMAX_DELAY);
    String msg = aiResponse;
    bool isAi = lastResponseIsAi;
    hasNewAIResponse = false;
    xSemaphoreGive(geminiMutex);

    if (lastTriggeredEventType == EVENT_WELCOME_BACK || lastTriggeredEventType == EVENT_FIRST_SIT) {
      pendingWelcomeAlert = true;
      welcomeAlertMessage = msg;
      welcomeAlertIsAi = isAi;
    } else {
      activeAlertMessage = msg;
      activeAlertIsAi = isAi;
      aiScreenEndTime = now + 8000;
      newAlert = true;
      publishMqttMessage(msg);
    }
  }

  if (pendingWelcomeAlert && (now - sitDownTime >= 15000UL)) {
    pendingWelcomeAlert = false;
    activeAlertMessage = welcomeAlertMessage;
    activeAlertIsAi = welcomeAlertIsAi;
    aiScreenEndTime = now + 8000;
    newAlert = true;
    publishMqttMessage(welcomeAlertMessage);
  }

  bool isAlertActive = (now < aiScreenEndTime);
  int targetPage = -1;
  if (currentPresenceState == STATE_AWAY) {
    pendingWelcomeAlert = false;
    welcomeAlertMessage = "";
    if (hasNewAIResponse) {
      if (lastTriggeredEventType == EVENT_WELCOME_BACK || lastTriggeredEventType == EVENT_FIRST_SIT) {
        hasNewAIResponse = false;
      }
    }
    if (now - lastStateTransitionTime < 60000UL) {
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
                     (clockFace != lastClockFace) || 
                     (time24h != lastTime24h) ||
                     newAlert;

  if (clockFace != lastClockFace) {
    tft.fillScreen(TFT_BLACK);
    lastClockFace = clockFace;
  }

  if (time24h != lastTime24h) {
    lastTime24h = time24h;
    tft.fillScreen(TFT_BLACK);
  }

  if (targetPage != lastDisplayedPage) {
    tft.fillScreen(TFT_BLACK);
    lastDisplayedPage = targetPage;
  }

  lastDisplayedState = currentPresenceState;

  // 1. If user is AWAY (and grace period has expired)
  if (currentPresenceState == STATE_AWAY && (now - lastStateTransitionTime >= 60000UL)) {
    if (forceRedraw) {
      drawRLEImage("/away.rle", 0, 0);
    }
    return;
  }



  // 2. Call active faceplate
  bool wifiAvailable = (WiFi.status() == WL_CONNECTED);
  bool internetAvailable = wifiAvailable && timeClient.isTimeSet();

  switch (clockFace) {
    case 1:
      drawMinimalistClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, hasMail);
      break;
    case 2:
      drawHiTechClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, hasMail);
      break;
    case 3:
      drawDevClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, hasMail);
      break;
    case 0:
    default:
      drawDefaultClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, hasMail);
      break;
  }
}

#endif // DISPLAY_H
