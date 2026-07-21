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

  bool fontLoaded = false;
  if (fontName != nullptr && strlen(fontName) > 0) {
    tft.loadFont(fontName, LittleFS);
    fontLoaded = true;
  }

  tft.setTextColor(textColor, bgColor);
  tft.setTextDatum(MC_DATUM); // Middle-Center align text
  
  int y = 45;
  String line = "";
  int startIdx = 0;
  
  // Wrap string into lines of up to DISPLAY_CHARS_PER_LINE characters
  while (startIdx < text.length()) {
    int endIdx = startIdx + DISPLAY_CHARS_PER_LINE;
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
    if (fontLoaded) {
      tft.drawString(line, 120, y);
    } else {
      tft.drawString(line, 120, y, 4);
    }
    y += 28;
    if (y > 200) break; // Avoid vertical overflow (up to ~5 lines)
  }

  if (fontLoaded) {
    tft.unloadFont();
  }

  if (isAi) {
    tft.setTextColor(tft.color565(45, 152, 200), tft.color565(15, 23, 42));
    tft.drawString("AI GENERATED", 120, 210, 2);
  }
}

inline void updateTFTDisplay(unsigned long now) {
  static int lastDisplayedPage = -1; // -1 = Away, 0 = Clock, -2 = Alert
  static int lastDisplayedState = -1;
  static int lastClockFace = -1;
  static String activeAlertMessage = "";
  static bool activeAlertIsAi = false;

  static String welcomeAlertMessage = "";
  static bool welcomeAlertIsAi = false;

  bool newAlert = false;
  if (appState.hasNewAIResponse) {
    xSemaphoreTake(appState.geminiMutex, portMAX_DELAY);
    String msg = appState.aiResponse;
    bool isAi = appState.lastResponseIsAi;
    appState.hasNewAIResponse = false;
    xSemaphoreGive(appState.geminiMutex);

    // Publish to MQTT immediately when any message is triggered
    publishMqttMessage(msg);

    if (appState.lastTriggeredEventType == EVENT_WELCOME_BACK || appState.lastTriggeredEventType == EVENT_FIRST_SIT) {
      appState.pendingWelcomeAlert = true;
      welcomeAlertMessage = msg;
      welcomeAlertIsAi = isAi;
    } else {
      activeAlertMessage = msg;
      activeAlertIsAi = isAi;
      appState.aiScreenEndTime = now + 8000;
      newAlert = true;
    }
  }

  if (appState.pendingWelcomeAlert && (now - appState.sitDownTime >= 15000UL)) {
    appState.pendingWelcomeAlert = false;
    activeAlertMessage = welcomeAlertMessage;
    activeAlertIsAi = welcomeAlertIsAi;
    appState.aiScreenEndTime = now + 8000;
    newAlert = true;
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
    if (lastClockFace == 4) {
      hourHandSprite.deleteSprite();
      minuteHandSprite.deleteSprite();
      secondHandSprite.deleteSprite();
      centerBgSprite.deleteSprite();
      Serial.println("[SPRITES] Aviator watch hands and center canvas deallocated from RAM.");
    } else if (lastClockFace == 5) {
      cleanupDeskbuddySprites();
    }
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
      drawDeskbuddyFaceplate(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
    case 0:
    default:
      drawDefaultClockFace(now, forceRedraw, isAlertActive, activeAlertMessage, activeAlertIsAi, wifiAvailable, internetAvailable, appConfig.hasMail);
      break;
  }
}

#endif // DISPLAY_H
