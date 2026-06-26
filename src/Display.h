#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include "Behaviour.h"

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
extern unsigned long aiScreenEndTime;
extern volatile bool hasNewAIResponse;
extern SemaphoreHandle_t geminiMutex;
extern String aiResponse;
extern volatile bool lastResponseIsAi;
extern volatile bool isAILoading;
extern int lastTriggeredEventType;

extern const RGBColor stateColors[];
extern RGBColor currentRingColor;
extern RGBColor startRingColor;
extern RGBColor targetRingColor;
extern unsigned long ringTransitionStart;
extern const unsigned long ringTransitionDuration;

// Forward declarations for clock faces (defined in Faceplates.h)
extern void drawMinimalistClockFace(unsigned long now, bool forceRedraw);
extern void drawHiTechClockFace(unsigned long now, bool forceRedraw);
extern void drawDefaultClockFace(unsigned long now, String &lastMetricText, uint16_t &lastMetricColor);

// Draw custom PackBits-RLE compressed image from LittleFS to TFT
inline void drawRLEImage(const char* filename, int16_t x, int16_t y) {
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
inline void drawCenteredWrappedText(String text, uint16_t color, bool isAi = false) {
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

inline void updateTFTDisplay(unsigned long now) {
  static unsigned long lastTFTUpdate = 0;
  static int lastDisplayedPage = -1;
  static int lastDisplayedState = -1;
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

  // Bezel ring is only on clockFace 0 (Default) and 1 (Minimalist)
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
    
    // Draw 3px thick bezel ring with smooth subpixel antialiasing using TFT_eSPI
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

  // If we are waiting for the AI welcome response, keep showing away screen
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

#endif // DISPLAY_H
