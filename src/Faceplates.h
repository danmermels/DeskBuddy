#ifndef FACEPLATES_H
#define FACEPLATES_H

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include "Display.h"

#include "State.h"

// Extern references for global state variables from main.cpp
extern struct tm ts;
extern ld2410 radar;
extern const RGBColor stateColors[];
extern char buf[];
extern NTPClient timeClient;
extern String formatTime(unsigned long ms);

#include <PubSubClient.h>
extern PubSubClient mqttClient;

// ============================================================================
// SECTION 1: GRAPHICS & ASSETS INTERFACE
// ============================================================================

inline int get12hDisplayHour(int h) {
  if (appConfig.time24h) return h;
  int display_h = h % 12;
  if (display_h == 0) display_h = 12;
  return display_h;
}

inline int getDailyProductivityPct() {
  if (appConfig.targetHours <= 0.0f) return 0;
  int pct = (int)((appStats.totalDeskTime * 100.0f) / (appConfig.targetHours * 3600.0f * 1000.0f));
  if (pct > 100) pct = 100;
  return pct;
}

inline void drawMailIcon(int x, int y, uint16_t bgColor, uint16_t fgColor) {
  tft.fillRect(x, y, 17, 12, bgColor);
  tft.drawRect(x, y, 17, 12, fgColor);
  tft.drawLine(x, y, x + 8, y + 6, fgColor);
  tft.drawLine(x + 8, y + 6, x + 16, y, fgColor);
}


// ============================================================================
// SECTION 2: DEFAULT FACEPLATE
// ============================================================================

#define MSG_FONT_DEFAULT nullptr
#define FONT_DEFAULT_WEATHER 4
#define FONT_DEFAULT_CLOCK 6
#define FONT_DEFAULT_DATE 2
#define FONT_DEFAULT_STATS 4
/**
 * SECTION 2: DEFAULT FACEPLATE
 * Draws the default digital clock face.
 * Layout:
 * - Top: Temperature and weather description.
 * - Center: Large digital clock (using built-in Font 6).
 * - Below Center: Date string.
 * - Bottom: Rotational statistics (productivity score, desk time, focus duration, etc.).
 * - Outermost Bezel: Smooth round color ring matching the active presence state.
 */
void drawDefaultClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  // 1. Mood Ring Animation & Concentric Drawing
  RGBColor targetColor = stateColors[appState.currentPresenceState];
  if (targetColor != appState.targetRingColor) {
    appState.startRingColor = appState.currentRingColor;
    appState.targetRingColor = targetColor;
    appState.ringTransitionStart = now;
  }

  static unsigned long lastRingUpdate = 0;
  bool isTransitioning = (appState.currentRingColor != appState.targetRingColor);
  bool ringRedrawn = false;

  // Redraw rings when transitioning or forced
  if (forceRedraw || (isTransitioning && (now - lastRingUpdate > 50))) {
    if (isTransitioning) {
      unsigned long elapsed = now - appState.ringTransitionStart;
      if (elapsed >= appState.ringTransitionDuration) {
        appState.currentRingColor = appState.targetRingColor;
      } else {
        float t = (float)elapsed / appState.ringTransitionDuration;
        t = (1.0f - cosf(t * 3.14159265f)) / 2.0f; // Cosine ease-in-out
        appState.currentRingColor.r = appState.startRingColor.r + t * (appState.targetRingColor.r - appState.startRingColor.r);
        appState.currentRingColor.g = appState.startRingColor.g + t * (appState.targetRingColor.g - appState.startRingColor.g);
        appState.currentRingColor.b = appState.startRingColor.b + t * (appState.targetRingColor.b - appState.startRingColor.b);
      }
    }
    
    // Scale colors to create triple ring glow/depth effect (100%, 60%, 25%)
    uint8_t r = appState.currentRingColor.r;
    uint8_t g = appState.currentRingColor.g;
    uint8_t b = appState.currentRingColor.b;
    uint16_t outerColor  = tft.color565(r, g, b);
    uint16_t middleColor = tft.color565(r * 60 / 100, g * 60 / 100, b * 60 / 100);
    uint16_t innerColor  = tft.color565(r * 25 / 100, g * 25 / 100, b * 25 / 100);

    // Draw three concentric rings for anti-aliased visual depth
    tft.drawSmoothRoundRect(2, 2, 118, 115, 0, 0, outerColor, TFT_BLACK);
    tft.drawSmoothRoundRect(8, 8, 112, 110, 0, 0, middleColor, TFT_BLACK);
    tft.drawSmoothRoundRect(14, 14, 106, 104, 0, 0, innerColor, TFT_BLACK);
    
    lastRingUpdate = now;
    ringRedrawn = true;
  }

  static bool wasEvent = false;

  // 2. Alert/Event Message Mode
  if (showEvent) {
    if (forceRedraw || ringRedrawn) {
      drawFaceplateMessage("/default_msg.rle", message, TFT_SKYBLUE, MSG_FONT_DEFAULT, isAi, TFT_LIGHTGREY, TFT_BLACK);
      
      // Redraw bezel rings on top of the alert background
      uint8_t r = appState.currentRingColor.r;
      uint8_t g = appState.currentRingColor.g;
      uint8_t b = appState.currentRingColor.b;
      uint16_t outerColor  = tft.color565(r, g, b);
      uint16_t middleColor = tft.color565(r * 60 / 100, g * 60 / 100, b * 60 / 100);
      uint16_t innerColor  = tft.color565(r * 25 / 100, g * 25 / 100, b * 25 / 100);
      
      tft.drawSmoothRoundRect(2, 2, 118, 115, 0, 0, outerColor, TFT_BLACK);
      tft.drawSmoothRoundRect(8, 8, 112, 110, 0, 0, middleColor, TFT_BLACK);
      tft.drawSmoothRoundRect(14, 14, 106, 104, 0, 0, innerColor, TFT_BLACK);
    }
    wasEvent = true;
    return;
  }

  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
  }

  // 3. Normal Clock Drawing (Throttled to 500ms unless redraw or ring updated)
  static unsigned long lastDefaultFaceUpdate = 0;
  if (!forceRedraw && !ringRedrawn && (now - lastDefaultFaceUpdate < 500)) {
    return;
  }
  lastDefaultFaceUpdate = now;

  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
  }

  static String lastMetricText = "";
  static uint16_t lastMetricColor = 0;
  static unsigned long lastMetricSwitch = 0;
  static int metricIndex = 0;

  tft.setTextDatum(MC_DATUM);

  // Weather section (top)
  tft.setTextColor(tft.color565(120, 130, 140), TFT_BLACK);
  tft.drawString("WEATHER", 120, 42, FONT_DEFAULT_DATE); // Faint label in font 2
  
  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  tft.drawString(String(appState.temp) + "C  |  " + appState.weatherDesc, 120, 58, FONT_DEFAULT_WEATHER);

  // Draw Mail Indicator
  static bool lastHasMailDefault = false;
  if (forceRedraw || (appConfig.hasMail != lastHasMailDefault)) {
    if (appConfig.hasMail) {
      drawMailIcon(200, 46, TFT_BLACK, TFT_YELLOW);
    } else {
      tft.fillRect(200, 46, 17, 12, TFT_BLACK);
    }
    lastHasMailDefault = appConfig.hasMail;
  }

  // Time section (center)
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  int display_h = get12hDisplayHour(h);
  char timeStrBuf[6];
  snprintf(timeStrBuf, sizeof(timeStrBuf), "%02d:%02d", display_h, m);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(timeStrBuf), 120, 102, FONT_DEFAULT_CLOCK);

  // Date section (below time)
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(buf, 120, 134, FONT_DEFAULT_DATE);

  // Glass divider line (Y=146)
  tft.drawFastHLine(50, 146, 140, tft.color565(80, 80, 80));   // Highlight edge
  tft.drawFastHLine(50, 147, 140, tft.color565(30, 30, 30));   // Shadow edge

  // Cycle through metrics at the bottom every 15 seconds
  if (now - lastMetricSwitch > 15000) {
    metricIndex = (metricIndex + 1) % 5;
    lastMetricSwitch = now;
  }

  String metricText = "";
  switch (metricIndex) {
    case 0: {
      int pct = getDailyProductivityPct();
      metricText = "DAY: " + String(pct) + "%";
      break;
    }
    case 1:
      metricText = "SCORE: " + String(appStats.productivityScore) + "%";
      break;
    case 2:
      metricText = "AT DESK: " + formatTime(now - appState.continuousPresenceStart);
      break;
    case 3:
      metricText = "BREAKS: " + String(appStats.breakCount);
      break;
    case 4:
      metricText = "FOCUS: " + formatTime(appStats.totalFocusTime);
      break;
  }

  // Tint metrics text according to current active mood ring color
  uint8_t ringR = appState.currentRingColor.r;
  uint8_t ringG = appState.currentRingColor.g;
  uint8_t ringB = appState.currentRingColor.b;
  uint16_t metricColor = tft.color565(ringR * 75 / 100, ringG * 75 / 100, ringB * 75 / 100);

  if (metricText != lastMetricText || metricColor != lastMetricColor || forceRedraw || ringRedrawn) {
    tft.fillRect(40, 156, 160, 30, TFT_BLACK); // Clear text area safely
    tft.setTextColor(metricColor, TFT_BLACK);
    tft.drawString(metricText, 120, 172, FONT_DEFAULT_STATS);
    lastMetricText = metricText;
    lastMetricColor = metricColor;
  }
}


// ============================================================================
// SECTION 3: MINIMALIST FACEPLATE
// ============================================================================

// Helper to determine if coordinates are inside the offset minute box
bool inMinuteWindow(int x, int y) {
  return (x >= 160 && y >= 72 && y <= 150);
}

#define MSG_FONT_MINIMALIST "RamisArabic18"
#define FONT_MINIMALIST_TICK "RamisArabic18"
#define FONT_MINIMALIST_HOUR "RamisArabic64"
#define FONT_MINIMALIST_DATE "RamisArabic18"
#define FONT_MINIMALIST_MINUTE "RamisArabic36"
/**
 * SECTION 3: MINIMALIST FACEPLATE
 * Draws a clean, dial-based minimalist clock face.
 * Layout:
 * - Outer Circle: Custom minute dial (ticks and numbers rotated counter-clockwise).
 * - Center: Very large hour digit (loaded dynamically from LittleFS RamisArabic64).
 * - Center-Right: Antialiased capsule enclosing two-digit minute numbers (loaded dynamically from RamisArabic36).
 * - Below Center: Date string (loaded dynamically from RamisArabic18).
 * - Top-Center: Status icons (WiFi, Internet connection, Mail envelope indicator).
 */
void drawMinimalistClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;
  tft.setTextDatum(MC_DATUM); // Ensure all text draws and erases with the same coordinate system

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/minimalist_msg.rle", message, TFT_WHITE, MSG_FONT_MINIMALIST, isAi, TFT_LIGHTGREY, TFT_BLACK);
    }
    wasEvent = true;
    return;
  }

  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
  }

  // Animation state for non-blocking counter-clockwise tick wipe
  static bool  tickAnimating  = false;
  static int   tickAnimNext   = 59;   // Next tick index to draw (counts down 59 -> 0)
  static int   tickTargetM    = 0;    // Minute the animation is drawing toward

  static unsigned long lastMinimalistFaceUpdate = 0;
  // Bypass throttle while the tick wipe animation is running
  if (!forceRedraw && !tickAnimating && (now - lastMinimalistFaceUpdate < 500)) {
    return;
  }
  if (!tickAnimating) {
    lastMinimalistFaceUpdate = now;
  }

  static int last_m = -1;
  static int last_h = -1;
  static String last_date = "";
  static int last_wifi_status = -1;
  static int last_internet_online = -1;

  int m = timeClient.getMinutes();
  int h = timeClient.getHours();

  bool minuteChanged = (last_m == -1 || m != last_m);
  bool hourChanged = (last_h == -1 || h != last_h);
  bool dateChanged = (last_date == "" || last_date != String(buf));

  // If forceRedraw is true, reset statics to force full draw without erasing
  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
    last_m = -1;
    last_h = -1;
    last_date = "";
    last_wifi_status = -1;
    last_internet_online = -1;
    minuteChanged = true;
    hourChanged = true;
    dateChanged = true;
  }

  // Draw / transition minutes ring
  if (minuteChanged) {
    appStats.fsReadCount++;
    tft.loadFont(FONT_MINIMALIST_TICK, LittleFS);

    if (last_m != -1) {
      // 1. Erase all old ticks and labels immediately (no delay needed here)
      for (int i = 0; i < 60; i++) {
        float rad_old = ((i - last_m) * 6 - 6) * 3.14159265f / 180.0f;
        float c_old = cosf(rad_old);
        float s_old = sinf(rad_old);
        
        int x_out_old = 120 + (int)(120 * c_old);
        int y_out_old = 120 + (int)(120 * s_old);
        int x_in_old = 120 + (int)(112 * c_old);
        int y_in_old = 120 + (int)(112 * s_old);
        
        if (!inMinuteWindow(x_in_old, y_in_old) && !inMinuteWindow(x_out_old, y_out_old)) {
          tft.drawWideLine((float)x_in_old, (float)y_in_old, (float)x_out_old, (float)y_out_old, 2.0f, TFT_BLACK, TFT_BLACK);
        }
        
        if (i % 5 == 0) {
          int x_text_old = 120 + (int)(100 * c_old)+1;
          int y_text_old = 120 + (int)(100 * s_old) - 11;
          if (!inMinuteWindow(x_text_old, y_text_old)) {
            tft.setTextColor(TFT_BLACK, TFT_BLACK);
            tft.drawString(String(i), x_text_old, y_text_old);
          }
        }
      }

      tft.unloadFont();

      // 2. Queue the counter-clockwise draw as a non-blocking multi-frame animation
      tickAnimating = true;
      tickAnimNext  = 59;
      tickTargetM   = m;
    } else {
      // First draw on startup or page switch: render all ticks instantly (no animation)
      for (int i = 0; i < 60; i++) {
        float rad_new = ((i - m) * 6 - 6) * 3.14159265f / 180.0f;
        float c_new = cosf(rad_new);
        float s_new = sinf(rad_new);
        
        int x_out_new = 120 + (int)(120 * c_new);
        int y_out_new = 120 + (int)(120 * s_new);
        int x_in_new = 120 + (int)(112 * c_new);
        int y_in_new = 120 + (int)(112 * s_new);
        
        if (!inMinuteWindow(x_in_new, y_in_new) && !inMinuteWindow(x_out_new, y_out_new)) {
          uint16_t color = (i % 5 == 0) ? TFT_WHITE : tft.color565(100, 100, 100);
          float wd = (i % 5 == 0) ? 1.5f : 1.0f;
          tft.drawWideLine((float)x_in_new, (float)y_in_new, (float)x_out_new, (float)y_out_new, wd, color, TFT_BLACK);
        }
        
        if (i % 5 == 0) {
          int x_text_new = 120 + (int)(100 * c_new)+1;
          int y_text_new = 120 + (int)(100 * s_new) - 11;
          if (!inMinuteWindow(x_text_new, y_text_new)) {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(String(i), x_text_new, y_text_new);
          }
        }
      }
      tft.unloadFont();
    }
  }

  // Non-blocking tick wipe animation: draw one batch of ticks per call
  // Each call draws TICK_BATCH ticks and bypasses the 500ms throttle until done.
  // With the main loop running every ~10ms, 60 ticks at 6/frame = 10 frames = ~100ms non-blocking.
  if (tickAnimating) {
    const int TICK_BATCH = 6;
    appStats.fsReadCount++;
    tft.loadFont(FONT_MINIMALIST_TICK, LittleFS);

    for (int b = 0; b < TICK_BATCH && tickAnimNext >= 0; b++, tickAnimNext--) {
      int i = tickAnimNext;
      float rad_new = ((i - tickTargetM) * 6 - 6) * 3.14159265f / 180.0f;
      float c_new = cosf(rad_new);
      float s_new = sinf(rad_new);

      int x_out_new = 120 + (int)(120 * c_new);
      int y_out_new = 120 + (int)(120 * s_new);
      int x_in_new  = 120 + (int)(112 * c_new);
      int y_in_new  = 120 + (int)(112 * s_new);

      if (!inMinuteWindow(x_in_new, y_in_new) && !inMinuteWindow(x_out_new, y_out_new)) {
        uint16_t color = (i % 5 == 0) ? TFT_WHITE : tft.color565(100, 100, 100);
        float wd = (i % 5 == 0) ? 1.5f : 1.0f;
        tft.drawWideLine((float)x_in_new, (float)y_in_new, (float)x_out_new, (float)y_out_new, wd, color, TFT_BLACK);
      }

      if (i % 5 == 0) {
        int x_text_new = 120 + (int)(100 * c_new) + 1;
        int y_text_new = 120 + (int)(100 * s_new) - 11;
        if (!inMinuteWindow(x_text_new, y_text_new)) {
          tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
          tft.drawString(String(i), x_text_new, y_text_new);
        }
      }
    }

    tft.unloadFont();

    if (tickAnimNext < 0) {
      tickAnimating = false; // Animation complete
    }
    // Force the next call through immediately so animation frames are continuous
    lastMinimalistFaceUpdate = 0;
  }

  // Draw capsule outline if forced redraw or if minutes changed (repairs ticks erase)
  if (forceRedraw || minuteChanged) {
    tft.drawSmoothRoundRect(175, 86, 25, 24, 105, 50, TFT_WHITE, TFT_BLACK);
  }

  // Draw Hour (only when changed)
  if (hourChanged) {
    if (last_h != -1) {
      tft.fillRect(66, 85, 108, 62, TFT_BLACK); // Clear Hour area safely
    }
    int display_h = get12hDisplayHour(h);
    char hourStrBuf[3];
    snprintf(hourStrBuf, sizeof(hourStrBuf), "%02d", display_h);
    appStats.fsReadCount++;
    tft.loadFont(FONT_MINIMALIST_HOUR, LittleFS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(hourStrBuf), 120, 70);
    tft.unloadFont();
  }

  // Draw Date (only when changed)
  if (dateChanged) {
    if (last_date != "") {
      tft.fillRect(40, 149, 160, 18, TFT_RED); // Clear Date area safely (doesn't overlap Hour)
    }
    appStats.fsReadCount++;
    tft.loadFont(FONT_MINIMALIST_DATE, LittleFS);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(buf, 120, 158);
    tft.unloadFont();
    last_date = String(buf);
  }

  // Draw Minute (only when changed)
  if (minuteChanged) {
    if (last_m != -1) {
      tft.fillRect(185, 94, 45, 34, TFT_BLACK); // Clear only the large minute digits area inside the capsule
    }
    appStats.fsReadCount++;
    tft.loadFont(FONT_MINIMALIST_MINUTE, LittleFS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char minStr[3];
    snprintf(minStr, sizeof(minStr), "%02d", m);
    tft.drawString(String(minStr), 208, 87);
    tft.unloadFont();
  }

  // Draw Status Icons at Y=60 (Wifi, Internet)
  bool wifi_connected = wifiAvailable;
  bool internet_online = internetAvailable;
  static int last_mail_status = -1;

  if (forceRedraw || 
      (wifi_connected != (last_wifi_status == 1)) || 
      (internet_online != (last_internet_online == 1)) ||
      (appConfig.hasMail != (last_mail_status == 1))) {
    
    // Clear old checkmark icon area if it was drawn (starts at X=142, width 16, Y=52 to 68)
    tft.fillRect(142, 52, 30, 16, TFT_BLACK); // Expanded clear area for mail icon too

    // Draw WiFi icon centered at X=105 -> drawn at X=97, Y=53 (width 17)
    int wifiX = 97;
    int wifiY = 53;
    uint16_t wifiCol = wifi_connected ? TFT_WHITE : tft.color565(80, 80, 80);
    tft.fillRect(wifiX, wifiY, 17, 15, TFT_BLACK); // Clear area safely
    drawRLEImage("/wifi.rle", wifiX, wifiY, wifiCol);

    // Draw Internet (Globe) icon centered at X=135 -> drawn at X=128, Y=53 (width 14)
    int netX = 128;
    int netY = 53;
    uint16_t netCol = internet_online ? TFT_WHITE : tft.color565(80, 80, 80);
    tft.fillRect(netX, netY, 14, 15, TFT_BLACK); // Clear area safely
    drawRLEImage("/internet.rle", netX, netY, netCol);

    // Draw Mail envelope icon at X=152, Y=53 (width 15)
    if (appConfig.hasMail) {
      drawMailIcon(152, 53, TFT_BLACK, TFT_WHITE);
    } else {
      tft.fillRect(152, 53, 16, 13, TFT_BLACK);
    }

    last_wifi_status = wifi_connected ? 1 : 0;
    last_internet_online = internet_online ? 1 : 0;
    last_mail_status = appConfig.hasMail ? 1 : 0;
  }

  // Update statics at the end
  last_m = m;
  last_h = h;
}


// ============================================================================
// SECTION 4: HITECH FACEPLATE
// ============================================================================

// Colors for HiTech Faceplate
#define HITECH_CYAN      tft.color565(87, 175, 174)   // Glow cyan
#define HITECH_MUTED     tft.color565(43, 95, 97)     // Muted cyan/gray
#define HITECH_DARKTEAL  tft.color565(23, 34, 36)     // Cyberpunk dark background

#define HITECH_BG_TIME   tft.color565(23, 35, 36)     // Color under Time and Date
#define HITECH_BOX_BG    tft.color565(36, 54, 59)     // Color under Left and Right Box metrics
#define HITECH_BG_STATUS tft.color565(24, 35, 37)     // Color under Status icons and weather



#define MSG_FONT_HITECH "GoodTiming20"
#define FONT_HITECH_TIME "GoodTiming46"
#define FONT_HITECH_TEMP "GoodTiming15"
#define FONT_HITECH_DAY "GoodTiming15"
#define FONT_HITECH_DATE "GoodTiming15"
#define FONT_HITECH_STATS "GoodTiming20"
/**
 * SECTION 4: HITECH FACEPLATE
 * Draws a futuristic cyberpunk HUD style clock face using a custom background bitmap.
 * Layout:
 * - Background: Pre-compiled RLE bitmap image ("/hitech.rle") loaded from LittleFS.
 * - Center-Left: Time display in HH:MM format (loaded dynamically from GoodTiming46).
 * - Center-Right: Day of week and date in DD MM format (loaded dynamically from GoodTiming15).
 * - Top-Right: Weather temperature display (loaded dynamically from GoodTiming15).
 * - Top-Left: Cyberpunk style WiFi, Internet, and Mail status indicators.
 * - Bottom slots: Daily accumulated desk and break hours (loaded dynamically from GoodTiming20).
 */
void drawHiTechClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/hitech_msg.rle", message, HITECH_CYAN, MSG_FONT_HITECH, isAi, HITECH_MUTED, HITECH_DARKTEAL);
    }
    wasEvent = true;
    return;
  }

  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
  }

  static unsigned long lastHiTechFaceUpdate = 0;
  if (!forceRedraw && (now - lastHiTechFaceUpdate < 500)) {
    return;
  }
  lastHiTechFaceUpdate = now;
  static int last_wifi = -1;
  static int last_internet = -1;
  static int last_hour = -1;
  static int last_min = -1;
  static int last_temp = -999;
  static int last_mday = -1;
  static int last_mon = -1;
  static int last_mail = -1;
  static int last_desk_hours = -1;
  static int last_break_hours = -1;

  if (forceRedraw) {
    drawRLEImage("/hitech.rle", 0, 0);
    last_wifi = -1;
    last_internet = -1;
    last_hour = -1;
    last_min = -1;
    last_temp = -999;
    last_mday = -1;
    last_mon = -1;
    last_mail = -1;
    last_desk_hours = -1;
    last_break_hours = -1;
  }

  bool wifi_connected = wifiAvailable;
  bool internet_online = internetAvailable;

  int wifi_status = wifi_connected ? 1 : 0;
  int internet_status = internet_online ? 1 : 0;
  int mail_status = appConfig.hasMail ? 1 : 0;

  if (wifi_status != last_wifi) {
    if (wifi_connected) {
      drawRLEImage("/wifi.rle", 68, 18);
    } else {
      // Clear WiFi icon area with HITECH_BG_STATUS (width = 17, height = 15)
      tft.fillRect(68, 18, 17, 15, HITECH_BG_STATUS);
    }
    last_wifi = wifi_status;
  }

  if (internet_status != last_internet) {
    if (internet_online) {
      drawRLEImage("/internet.rle", 90, 18);
    } else {
      // Clear Internet icon area with HITECH_BG_STATUS (width = 14, height = 15)
      tft.fillRect(90, 18, 14, 15, HITECH_BG_STATUS);
    }
    last_internet = internet_status;
  }

  if (mail_status != last_mail || forceRedraw) {
    if (appConfig.hasMail) {
      drawMailIcon(112, 18, HITECH_BG_STATUS, HITECH_CYAN);
    } else {
      tft.fillRect(112, 18, 16, 15, HITECH_BG_STATUS);
    }
    last_mail = mail_status;
  }

  // Draw time in HH:MM format using GoodTiming46 font
  // Box: x = 60, y = 58, width = 120, height = 39. Center: X = 120, Y = 77
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  if (h != last_hour || m != last_min) {
    // Clear time window (aligned to time box Y=38 to Y=76, widened to 128px)
    tft.fillRect(45, 67, 146, 32, HITECH_BG_TIME);
    

    // Draw centered time
    appStats.fsReadCount++;
    tft.loadFont(FONT_HITECH_TIME, LittleFS);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_TIME);
    tft.setTextDatum(MC_DATUM);

    int display_h = get12hDisplayHour(h);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
    tft.drawString(String(timeStr), 117, 56);
    tft.unloadFont();

    last_hour = h;
    last_min = m;
  }

  // Draw temperature in format 00 and celsius upper o
  // Window: lower-left (147, 208) -> standard (147, 31); top-right (174, 220) -> standard (174, 19)
  // Box: x = 147, y = 19, width = 28, height = 13. Center: X = 160.5, Y = 25
  if (appState.temp != last_temp) {
    // Clear box area (aligned to Y=19 center with 13px height)
    tft.fillRect(138, 21, 40, 13, HITECH_BG_STATUS);

    appStats.fsReadCount++;
    tft.loadFont(FONT_HITECH_TEMP, LittleFS);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_STATUS);
    tft.setTextDatum(MC_DATUM);

    char tempValStr[3];
    snprintf(tempValStr, sizeof(tempValStr), "%02d", appState.temp);

    // Draw 2-digit temperature centered at X=153, Y=19
    tft.drawString(String(tempValStr), 150, 19);

    // Draw degree circle (radius 1) centered at X=163, Y=15
    tft.drawCircle(163, 24, 1, HITECH_CYAN);

    // Draw letter C centered at X=169, Y=19
    tft.drawString("C", 170, 19);

    tft.unloadFont();
    last_temp = appState.temp;
  }

  // Draw 3-letter day of week abbreviation and date in DD MM format
  // Day Window: lower-left (69, 118) -> standard (69, 121); top-right (100, 133) -> standard (100, 106)
  // Day Box: x = 69, y = 106, width = 31, height = 15. Center: X = 84, Y = 113
  // Date Window: lower-left (116, 118) -> standard (116, 121); top-right (170, 133) -> standard (170, 106)
  // Date Box: x = 116, y = 106, width = 54, height = 15. Center: X = 143, Y = 113
  if (ts.tm_mday != last_mday || ts.tm_mon != last_mon) {
    // 1. Day of the Week Box (restored to original y=106, height=15)
    tft.fillRect(65, 109, 38, 13, HITECH_BG_TIME);
    appStats.fsReadCount++;
    tft.loadFont(FONT_HITECH_DAY, LittleFS);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_TIME);
    tft.setTextDatum(MC_DATUM);
#if DESKBUDDY_LANG_PTBR
    const char* daysOfWeek[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
#else
    const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
#endif
    int wday = ts.tm_wday;
    if (wday < 0 || wday > 6) wday = 0;
    tft.drawString(daysOfWeek[wday], 84, 106);
    tft.unloadFont();

    // 2. Date Box (restored to original y=106, height=15)
    tft.fillRect(113, 109, 54, 13, HITECH_BG_TIME);
    appStats.fsReadCount++;
    tft.loadFont(FONT_HITECH_DATE, LittleFS);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_TIME);
    tft.setTextDatum(MC_DATUM);
    char dateStr[6];
    snprintf(dateStr, sizeof(dateStr), "%02d %02d", ts.tm_mday, ts.tm_mon + 1);
    tft.drawString(dateStr, 138, 106);
    tft.unloadFont();

    last_mday = ts.tm_mday;
    last_mon = ts.tm_mon;
  }

  // Draw sitting and away hours (updated dynamically when values change or forceRedraw)
  int current_desk_hours = appStats.totalDeskTime / 3600000UL;
  int current_break_hours = appStats.totalBreakTime / 3600000UL;

  if (current_desk_hours != last_desk_hours || current_break_hours != last_break_hours || forceRedraw) {
    // Clear boxes with HITECH_BG_STATUS (aligned to Y=134 to Y=150 inside slot borders)
    tft.fillRect(61, 147, 28, 15, HITECH_BOX_BG);
    tft.fillRect(126, 147, 28, 16, HITECH_BOX_BG);

    appStats.fsReadCount++;
    tft.loadFont(FONT_HITECH_STATS, LittleFS);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_STATUS);
    tft.setTextDatum(MC_DATUM);
    char deskHoursStr[8];
    char breakHoursStr[8];
    snprintf(deskHoursStr, sizeof(deskHoursStr), "%dh", current_desk_hours);
    snprintf(breakHoursStr, sizeof(breakHoursStr), "%dh", current_break_hours);
    tft.drawString(String(deskHoursStr), 89, 142);
    tft.drawString(String(breakHoursStr), 154, 142);
    tft.unloadFont();

    last_desk_hours = current_desk_hours;
    last_break_hours = current_break_hours;
  }
}

// Helper function to format duration as H:mm:ss
void formatHMS(unsigned long ms, char* outStr, size_t maxLen) {
  unsigned long seconds = ms / 1000UL;
  unsigned long h = seconds / 3600UL;
  unsigned long m = (seconds % 3600UL) / 60UL;
  unsigned long s = seconds % 60UL;
  snprintf(outStr, maxLen, "%lu:%02lu:%02lu", h, m, s);
}

// ============================================================================
// SECTION 5: DEV FACEPLATE
// ============================================================================

#define MSG_FONT_DEV nullptr
#define FONT_DEV_DATA 2

/**
 * SECTION 5: DEV FACEPLATE
 * Draws a high-density, real-time developer debug screen.
 * Uses cheap, fast, built-in non-antialiased fonts to optimize rendering speed and prevent flicker.
 * Telemetry elements:
 * - NTP Clock (shows seconds)
 * - Network Details (IP address and WiFi signal RSSI)
 * - Device Presence States (Away, Focus, Busy, etc.)
 * - Radar Telemetry (Raw vs Filtered target distance, target motion/static status)
 * - Session Metrics (Sitting time H:mm:ss)
 * - Daily Statistics (Total desk/break hours H:mm:ss, breaks count)
 * - RAM Health (Free heap memory vs historical min free heap since boot)
 */
void drawDevClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage(nullptr, message, TFT_GREEN, MSG_FONT_DEV, isAi, TFT_DARKGREY, TFT_BLACK);
    }
    wasEvent = true;
    return;
  }

  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
  }

  static unsigned long lastDevUpdate = 0;
  if (!forceRedraw && (now - lastDevUpdate < 200)) {
    return;
  }
  lastDevUpdate = now;

  static char prevLines[12][32];
  if (forceRedraw) {
    memset(prevLines, 0, sizeof(prevLines));
    tft.fillScreen(TFT_BLACK);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  auto drawDevLine = [&](int lineIndex, const char* newStr, int y) {
    if (strcmp(prevLines[lineIndex], newStr) != 0) {
      tft.setTextPadding(230);
      tft.drawString(newStr, 120, y, FONT_DEV_DATA);
      tft.setTextPadding(0);
      strncpy(prevLines[lineIndex], newStr, sizeof(prevLines[lineIndex]) - 1);
    }
  };

  char line[32];

  // Line 1: Header
  drawDevLine(0, "--- DEV MODE ---", 22);

  // Line 2: NTP Time
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();
  snprintf(line, sizeof(line), "TIME: %02d:%02d:%02d", hours, minutes, seconds);
  drawDevLine(1, line, 38);

  // Line 3: IP Address
  snprintf(line, sizeof(line), "IP: %s", wifiAvailable ? WiFi.localIP().toString().c_str() : "DISCONNECTED");
  drawDevLine(2, line, 54);

  // Line 4: RSSI
  int rssi = wifiAvailable ? WiFi.RSSI() : 0;
  snprintf(line, sizeof(line), "RSSI: %d dBm", rssi);
  drawDevLine(3, line, 70);

  // Line 5: Presence State
  const char* stateNames[] = {"AWAY", "FOCUS", "BUSY", "DISTRACTED", "REGULAR"};
  const char* stateStr = (appState.currentPresenceState >= 0 && appState.currentPresenceState < 5) ? stateNames[appState.currentPresenceState] : "UNKNOWN";
  snprintf(line, sizeof(line), "STATE: %s", stateStr);
  drawDevLine(4, line, 86);

  // Line 6: Radar targets (Motion and Static)
  int isPresent = radar.presenceDetected() ? 1 : 0;
  int isMoving = radar.movingTargetDetected() ? 1 : 0;
  int isStatic = radar.stationaryTargetDetected() ? 1 : 0;
  snprintf(line, sizeof(line), "RADAR: P:%d M:%d S:%d", isPresent, isMoving, isStatic);
  drawDevLine(5, line, 102);

  // Line 7: Raw & Filtered distance
  snprintf(line, sizeof(line), "DIST: R:%d F:%d", appState.rawDetectionDist, (int)appState.filteredDetectionDist);
  drawDevLine(6, line, 118);

  // Line 8: Session Sitting Timer
  unsigned long sessSitMs = 0;
  if (appState.currentPresenceState != STATE_AWAY) {
    sessSitMs = now - appState.continuousPresenceStart;
  }
  char sitStr[12];
  formatHMS(sessSitMs, sitStr, sizeof(sitStr));
  snprintf(line, sizeof(line), "SESS: %s", sitStr);
  drawDevLine(7, line, 134);

  // Line 9: Workday stats
  char dailyDeskStr[12];
  char dailyAwayStr[12];
  formatHMS(appStats.totalDeskTime, dailyDeskStr, sizeof(dailyDeskStr));
  formatHMS(appStats.totalBreakTime, dailyAwayStr, sizeof(dailyAwayStr));
  snprintf(line, sizeof(line), "DAY: S:%s A:%s", dailyDeskStr, dailyAwayStr);
  drawDevLine(8, line, 150);

  // Line 10: Break Count & Latest Break Duration
  unsigned long latestBreakMins = appStats.latestBreakDuration / 60000UL;
  snprintf(line, sizeof(line), "BREAKS: %d L:%lum", appStats.breakCount, latestBreakMins);
  drawDevLine(9, line, 166);

  // Line 11: File System Reads & Writes
  snprintf(line, sizeof(line), "FS: R:%u W:%u", appStats.fsReadCount, appStats.fsWriteCount);
  drawDevLine(10, line, 182);

  // Line 12: Heap & AI Requests Count
    uint32_t freeHeapK = ESP.getFreeHeap() / 1024;
  snprintf(line, sizeof(line), "HEAP:%uK AI:%d", freeHeapK, appStats.dailyAiRequestCount);
  drawDevLine(11, line, 198);
}

// ============================================================================
// SECTION 6: AVIATOR FACEPLATE
// ============================================================================

#define COLOR_TRANSPARENT       TFT_BLACK // Black pixels will be transparent
#define COLOR_AVIATOR_TIME_FG     tft.color565(240, 240, 240)
#define COLOR_AVIATOR_TIME_BG     tft.color565(65, 65, 65)
#define COLOR_AVIATOR_DATE_FG     tft.color565(240, 240, 240)
#define COLOR_AVIATOR_DATE_BG     tft.color565(240, 240, 240)
#define COLOR_AVIATOR_WEATHER_FG  tft.color565(240, 240, 240)
#define COLOR_AVIATOR_WEATHER_BG  tft.color565(240, 240, 240)
#define COLOR_AVIATOR_FOCUS_FG    tft.color565(235, 94, 40)
#define COLOR_AVIATOR_FOCUS_BG    tft.color565(235, 94, 40)
#define MSG_FONT_AVIATOR        "GoodTiming15"

static bool aviatorSpritesFailed = false;

void initWatchHandSprites() {
#if DESKBUDDY_DEBUG
  Serial.println("[SPRITES] Allocating Aviator watch hands and center canvas sprite...");
#endif
  aviatorSpritesFailed = false;

  // 1. Hour Hand Sprite (27x100)
  if (hourHandSprite.created()) hourHandSprite.deleteSprite();
  hourHandSprite.setColorDepth(16);
  if (hourHandSprite.createSprite(27, 100) == nullptr) {
    aviatorSpritesFailed = true;
  } else {
    hourHandSprite.fillSprite(COLOR_TRANSPARENT);
    drawFullRLEToSprite(hourHandSprite, "/aviator_hour.rle");
    hourHandSprite.setPivot(13, 85);
  }

  // 2. Minute Hand Sprite (21x120)
  if (minuteHandSprite.created()) minuteHandSprite.deleteSprite();
  minuteHandSprite.setColorDepth(16);
  if (minuteHandSprite.createSprite(21, 120) == nullptr) {
    aviatorSpritesFailed = true;
  } else {
    minuteHandSprite.fillSprite(COLOR_TRANSPARENT);
    drawFullRLEToSprite(minuteHandSprite, "/aviator_minute.rle");
    minuteHandSprite.setPivot(10, 109);
  }

  // 3. Second Hand Sprite (9x127)
  if (secondHandSprite.created()) secondHandSprite.deleteSprite();
  secondHandSprite.setColorDepth(16);
  if (secondHandSprite.createSprite(9, 127) == nullptr) {
    aviatorSpritesFailed = true;
  } else {
    secondHandSprite.fillSprite(COLOR_TRANSPARENT);
    drawFullRLEToSprite(secondHandSprite, "/aviator_second.rle");
    secondHandSprite.setPivot(4, 110);
  }

  // 4. Center Canvas Patch Sprite (220x220)
  if (centerBgSprite.created() && (centerBgSprite.width() != 220 || centerBgSprite.height() != 220)) {
    centerBgSprite.deleteSprite();
  }
  if (!centerBgSprite.created()) {
    centerBgSprite.setColorDepth(16);
    if (centerBgSprite.createSprite(220, 220) == nullptr) {
      aviatorSpritesFailed = true;
    }
  }
  if (centerBgSprite.created()) {
    centerBgSprite.fillSprite(TFT_BLACK);
  }

  if (aviatorSpritesFailed) {
    if (hourHandSprite.created()) hourHandSprite.deleteSprite();
    if (minuteHandSprite.created()) minuteHandSprite.deleteSprite();
    if (secondHandSprite.created()) secondHandSprite.deleteSprite();
    if (centerBgSprite.created()) centerBgSprite.deleteSprite();
#if DESKBUDDY_DEBUG
    Serial.println("[SPRITES] Aviator sprite alloc failed - analog rendering disabled until heap recovers.");
#endif
  }
}

void drawAviatorClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/aviator_msg.rle", message, COLOR_AVIATOR_FOCUS_FG, MSG_FONT_AVIATOR, isAi, TFT_LIGHTGREY, TFT_BLACK);
    }
    wasEvent = true;
    return;
  }

  // If returning from an event/message screen, force a full background redraw
  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
  }

  // Lazy loading of sprites from flash on faceplate select
  if ((!hourHandSprite.created() || !centerBgSprite.created()) && !aviatorSpritesFailed) {
    initWatchHandSprites();
  }

  // Read current time
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  int s = timeClient.getSeconds();

  static int last_min = -1;
  static int last_sec = -1;
  static int last_temp = -999;
  static int last_mday = -1;
  static int last_pct = -1;

  bool timeChanged = (s != last_sec);
  if (!forceRedraw && !timeChanged) {
    return;
  }
  last_sec = s;

  // 1. Draw full 240x240 background image ONLY on forceRedraw (faceplate load / mode switch)
  if (forceRedraw) {
    drawRLEImage("/aviator_bg.rle", 0, 0);
    last_min = -1;
    last_temp = -999;
    last_mday = -1;
    last_pct = -1;
  }

  // 2. Focus progress percentage calculation
  int pct = getDailyProductivityPct();

  // 3. Render and cache background + all text overlays + hour/minute hands in RAM canvas on change
  bool updateCanvas = forceRedraw || (m != last_min) || (appState.temp != last_temp) || (ts.tm_mday != last_mday) || (pct != last_pct);

  if (updateCanvas && centerBgSprite.created()) {
    // Refresh center background slice in RAM (220x220 centered at 10,10)
    drawRLEImageToSprite(centerBgSprite, "/aviator_bg.rle", 10, 10, 220, 220);
    
    // Draw weather (TFT Y=54 -> relative Y=44)
    centerBgSprite.setTextDatum(MC_DATUM);
    centerBgSprite.setTextColor(COLOR_AVIATOR_WEATHER_FG, COLOR_AVIATOR_WEATHER_BG);
    centerBgSprite.drawString(String(appState.temp) + "C", 162, 110, 2);

    // Draw Top Month/Day (TFT Y=74 -> relative Y=64)
    char monthDayStr[12];
#if DESKBUDDY_LANG_PTBR
    const char* months[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
#else
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
#endif
    snprintf(monthDayStr, sizeof(monthDayStr), "%02d %s", ts.tm_mday, months[ts.tm_mon]);
    centerBgSprite.setTextColor(COLOR_AVIATOR_DATE_FG, COLOR_AVIATOR_DATE_BG);
    centerBgSprite.drawString(monthDayStr, 86, 61, 2);

    // Draw Digital Time (HH:MM) centered at Y=93 (relative Y=83)
    int display_h = get12hDisplayHour(h);
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
    centerBgSprite.setTextColor(COLOR_AVIATOR_TIME_FG, COLOR_AVIATOR_TIME_BG);
    centerBgSprite.drawString(timeStr, 88, 83, 4);

    // Draw Focus Progress (TFT Y=182 -> relative Y=172)
    char stepsGoalStr[32];
    snprintf(stepsGoalStr, sizeof(stepsGoalStr), "FOCUS: %d%%", pct);
    centerBgSprite.setTextColor(COLOR_AVIATOR_FOCUS_FG, COLOR_AVIATOR_FOCUS_BG);
    centerBgSprite.drawString(stepsGoalStr, 110, 172, 2);

    // Set pivot of centerBgSprite relative to its center (110, 110)
    centerBgSprite.setPivot(110, 110);

    // Pre-rotate and draw Hour and Minute hands directly onto centerBgSprite in RAM
    float hourAngle = ((h % 12) * 30.0f) + (m * 0.5f);
    float minAngle = m * 6.0f;
    if (hourHandSprite.created()) hourHandSprite.pushRotated(&centerBgSprite, hourAngle, COLOR_TRANSPARENT);
    if (minuteHandSprite.created()) minuteHandSprite.pushRotated(&centerBgSprite, minAngle, COLOR_TRANSPARENT);

    // Update cached states
    last_min = m;
    last_temp = appState.temp;
    last_mday = ts.tm_mday;
    last_pct = pct;
  }

  // 4. Every second: Push center background RAM patch (<1ms, contains dial, text, and Hour+Minute hands!)
  if (centerBgSprite.created()) {
    centerBgSprite.pushSprite(10, 10);

    // 5. Draw ONLY the second hand on top of TFT
    tft.setPivot(120, 120);

    float secAngle = s * 6.0f;
    if (secondHandSprite.created()) {
      secondHandSprite.pushRotated(secAngle, COLOR_TRANSPARENT);
    }

    // Center hub pin
    tft.drawSmoothCircle(120, 120, 5, COLOR_AVIATOR_FOCUS_FG, COLOR_AVIATOR_FOCUS_FG);
    tft.drawSmoothCircle(120, 120, 2, TFT_BLACK, TFT_BLACK);
  }
}

// ============================================================================
// SECTION 7: DESKBUDDY FACEPLATE
// ============================================================================
// Animated companion face: 100x100 glowing cyan halo ring eye bitmaps mapped to
// presence states (Regular=Open, Focus=Closed, Busy=Squint, Away=Half-lidded),
// blinking ONLY in Open mode, dynamic eye convergence on close proximity (-6px),
// 30px Y gaze amplitude with resting offset above 0, 100ms digital glitch effects (~2/min),
// and 2-second spontaneous mode cameos (~2-3/min, excluded during Focus/Closed).
// Pure TFT_BLACK canvas background (no dark container box behind eyes).
//
// Layout:
//   Top 50%    (Y=0-120):  Visor canvas sprite (220x115) — Left and Right eyes
//                           stamped from 100x100 LittleFS RLE files on pure TFT_BLACK.
//   Bottom 50% (Y=120-240): Time (built-in Font 6), date, rotating stat.
//
// Eye Bitmaps (LittleFS RLE, 100x100 px each):
//   /buddy_eye_o.rle  — Open ring eye with cyan bloom (default render mode)
//   /buddy_eye_s.rle  — Happy / squint crescent (cameo mode)
//
// Refined Rules & Physics:
//   - Paired Eye States: Eyes are always stamped in pairs.
//   - Pure Black Background: Visor sprite fills TFT_BLACK (no background box).
//   - Render Mode: The primary eye mode is fixed at EYE_MODE_OPEN, so the open sprite
//     is always rendered. EYE_MODE_CLOSED / EYE_MODE_HALFED / EYE_MODE_SQUINT are
//     reserved enum modes; CLOSED/HALFED currently fall back to the open sprite.
//   - Resting Elevation: Resting gaze sits above 0 (-6px) when far/idle.
//   - Blinking: Top-down masking blink occurs ONLY when in OPEN mode!
//   - Eye Convergence: Eyes move 6px closer together as radar distance decreases
//                       (LX=69->75, RX=151->145 when <=45cm).
//   - Vertical Gaze Amplitude: 30px max (responsive to radar distance):
//       Distance <= 45cm  -> Look all the way to top of visor (-30px)
//       Distance >= 120cm -> Resting elevation (-6px)
// ============================================================================

// -- Eye bitmap dimensions ----------------------------------------------------
#define BUDDY_EYE_SPR_W   75   // 75x75 RLE sprite width (px)
#define BUDDY_EYE_SPR_H   75   // 75x75 RLE sprite height (px)

// Base eye center positions in the 130x98 visor sprite coordinate space
#define BUDDY_EYE_CY      76    // Vertical center of eyes in visor sprite (screen Y = 22 + 76 = 98)
#define BUDDY_EYE_LX_BASE 21    // Base Left eye center X (screen X = 55 + 21 = 76)
#define BUDDY_EYE_RX_BASE 109   // Base Right eye center X (screen X = 55 + 109 = 164)

// -- Visor canvas geometry ----------------------------------------------------
#define BUDDY_SPR_W       130   // 130px width (down 30px from 160)
#define BUDDY_SPR_H       98    // 98px height
#define BUDDY_SPR_X       55    // Centered at screen X = 55 (starts at 55, ends at 185)
#define BUDDY_SPR_Y       22    // Starts at screen Y=22

// -- Motion Physics Limits ---------------------------------------------------
#define BUDDY_EYE_GAZE_X_LIMIT   3   // Halved horizontal gaze drift (±3px)
#define BUDDY_EYE_GAZE_Y_LIMIT  30   // 30px vertical gaze amplitude

// -- Brow Sprite Geometry ----------------------------------------------------
#define BUDDY_BROW_W  40    // brow RLE sprite width  (px)
#define BUDDY_BROW_H  12    // brow RLE sprite height (px)

// -- Bottom-half font aliases --------------------------------------------------
#define FONT_BUDDY_TIME   7   // 7-segment digital font
#define FONT_BUDDY_DATE   2   // Small crisp text font
#define MSG_FONT_BUDDY    "RobotoCondensed26"

// Eye State Modes
enum BuddyEyeMode {
  EYE_MODE_OPEN = 0,
  EYE_MODE_HALFED,
  EYE_MODE_CLOSED,
  EYE_MODE_SQUINT
};

struct DeskbuddyThemeConfig {
  const char* bgRle;
  const char* browRle;
  const char* eyeOpenRle;
  const char* eyeSquintRle;
  
  const char* timeFont;    // Font filename for the digital clock time (e.g. "7Segment50")
  const char* dateFont;    // Font filename for the calendar date string (e.g. "Unicode.impact17")
  const char* metricFont;  // Font filename for telemetry metrics carousel (e.g. "GoodTiming20")
  const char* weatherFont; // Font filename for the weather display (e.g. "GoodTiming20")
  
  uint16_t timeColor;      // Color used for rendering the digital clock time digits
  uint16_t dateColor;      // Color used for rendering the calendar date string
  uint16_t metricColor;    // Color used for rendering metrics carousel text
  uint16_t weatherColor;   // Color used for rendering weather text
  bool useRingColorForMetric;
  
  int gazeXLimit;
  int gazeYLimit;
  unsigned long minBlinkInterval;
  unsigned long maxBlinkInterval;
  unsigned long minLookInterval;
  unsigned long maxLookInterval;

  // --- Layout Positioning (X/Y) & Redraw Clearing boundaries ---
  // Note on VLW Font coordinates vs Clearing boundaries:
  // Custom smooth (.vlw) fonts have large baseline offsets. The Y drawing coordinate passed to
  // tft.drawString() uses MC_DATUM (middle center) which aligns to the font's internal glyph center.
  // The clearing box Y coordinate (timeClearY, etc.) passed to tft.fillRect() represents the absolute top-left
  // of the physical screen clearing area. Hence, drawY is often lower (e.g. Y=101) than clearY (e.g. Y=127).
  
  int timeX; int timeY;                                             // Time draw X/Y center (MC_DATUM)
  int timeClearX; int timeClearY; int timeClearW; int timeClearH;   // Time clear box (top-left X/Y, Width, Height)
  
  int dateX; int dateY;                                             // Date draw X/Y center (MC_DATUM)
  int dateClearX; int dateClearY; int dateClearW; int dateClearH;   // Date clear box (top-left X/Y, Width, Height)
  
  int weatherX; int weatherY;                                       // Weather draw X/Y center (MC_DATUM)
  int weatherClearX; int weatherClearY; int weatherClearW; int weatherClearH; // Weather clear box
  
  int metricX; int metricY;                                         // Carousel metric draw X/Y center (MC_DATUM)
  int metricClearX; int metricClearY; int metricClearW; int metricClearH; // Carousel metric clear box
};

static const char* getRlePathOrFallback(const char* themedPath, const char* defaultPath) {
  if (LittleFS.exists(themedPath)) {
    return themedPath;
  }
  return defaultPath;
}

static const DeskbuddyThemeConfig deskbuddyThemes[5] = {
  // Theme 0: Deskbuddy (Original, ID 5)
  {
    "/buddy_bg.rle",
    "/buddy_brow.rle",
    "/buddy_eye_o.rle",
    "/buddy_eye_s.rle",
    "7Segment50",        // timeFont
    "7Segment50",  // dateFont
    "RobotoCondensed20",      // metricFont
    "RobotoCondensed20",      // weatherFont
    rgb565(245, 207, 142), // timeColor
    rgb565(142, 174, 245), // dateColor
    rgb565(50, 220, 255),   // metricColor
    rgb565(250, 220, 255),   // weatherColor
    false,  // useRingColorForMetric
    3,      // gazeXLimit
    30,     // gazeYLimit
    3500,   // minBlinkInterval
    8500,   // maxBlinkInterval
    4000,   // minLookInterval
    10000,  // maxLookInterval
    // Time: drawX, drawY, clearX, clearY, clearW, clearH
    71, 101, 33, 127, 85, 26,
    // Date: drawX, drawY, clearX, clearY, clearW, clearH
    167, 131, 128, 128, 78, 24,
    // Weather: drawX, drawY, clearX, clearY, clearW, clearH
    120, 187, 45, 165, 150, 24,
    // Carousel Metric (uncommented helper): drawX, drawY, clearX, clearY, clearW, clearH
    120, 168, 58, 165, 129, 20
  },
  // Theme 1: DeskAura (ID 6)
  {
    "/buddy_aura_bg.rle",
    "/buddy_aura_brow.rle",
    "/buddy_aura_eye_o.rle",
    "/buddy_aura_eye_s.rle",
    "RobotoCondensed26",          // timeFont
    "RobotoCondensed20",          // dateFont
    "RobotoCondensed20",      // metricFont
    "RobotoCondensed20",      // weatherFont
    rgb565(0, 255, 255),   // timeColor
    rgb565(255, 0, 255),   // dateColor
    rgb565(0, 255, 255),   // metricColor
    rgb565(0, 255, 255),   // weatherColor
    false,  // useRingColorForMetric
    5,      // gazeXLimit
    35,     // gazeYLimit
    2000,   // minBlinkInterval
    6000,   // maxBlinkInterval
    3000,   // minLookInterval
    7000,   // maxLookInterval
    // Time: drawX, drawY, clearX, clearY, clearW, clearH
    71, 125, 33, 127, 85, 26,
    // Date: drawX, drawY, clearX, clearY, clearW, clearH
    167, 131, 128, 128, 78, 24,
    // Weather: drawX, drawY, clearX, clearY, clearW, clearH
    120, 197, 45, 165, 150, 24,
    // Carousel Metric: drawX, drawY, clearX, clearY, clearW, clearH
    120, 163, 58, 165, 129, 20
  },
  // Theme 2: DeskCat (ID 7)
  {
    "/buddy_who_bg.rle",
    "/buddy_cat_brow.rle",
    "/buddy_cat_eye_o.rle",
    "/buddy_cat_eye_s.rle",
    "7Segment50",        // timeFont
    "SevenSegment20",    // dateFont
    "GoodTiming15",      // metricFont
    "GoodTiming15",      // weatherFont
    rgb565(255, 160, 0),   // timeColor
    rgb565(229, 160, 0),   // dateColor
    rgb565(255, 160, 0),   // metricColor
    rgb565(255, 160, 0),   // weatherColor
    false,  // useRingColorForMetric
    2,      // gazeXLimit
    25,     // gazeYLimit
    4000,   // minBlinkInterval
    10000,  // maxBlinkInterval
    5000,   // minLookInterval
    12000,  // maxLookInterval
    // Time: drawX, drawY, clearX, clearY, clearW, clearH
    71, 101, 33, 127, 85, 26,
    // Date: drawX, drawY, clearX, clearY, clearW, clearH
    167, 131, 128, 128, 78, 24,
    // Weather: drawX, drawY, clearX, clearY, clearW, clearH
    120, 167, 45, 165, 150, 24,
    // Carousel Metric: drawX, drawY, clearX, clearY, clearW, clearH
    120, 188, 58, 165, 129, 20
  },
  // Theme 3: DeskWho (ID 8)
  {
    "/buddy_who_bg.rle",
    "/buddy_who_brow.rle",
    "/buddy_who_eye_o.rle",
    "/buddy_who_eye_s.rle",
    "RobotoCondensed20",     // timeFont
    "RobotoCondensed20",     // dateFont
    "RobotoCondensed20",     // metricFont
    "RobotoCondensed20",     // weatherFont
    rgb565(46, 139, 87),  // timeColor
    rgb565(46, 139, 87),   // dateColor
    rgb565(46, 139, 87),  // metricColor
    rgb565(46, 139, 87),  // weatherColor
    false,  // useRingColorForMetric
    3,      // gazeXLimit
    30,     // gazeYLimit
    3500,   // minBlinkInterval
    8500,   // maxBlinkInterval
    4000,   // minLookInterval
    10000,  // maxLookInterval
    // Time: drawX, drawY, clearX, clearY, clearW, clearH
    71, 131, 33, 127, 85, 26,
    // Date: drawX, drawY, clearX, clearY, clearW, clearH
    167, 131, 128, 128, 78, 24,
    // Weather: drawX, drawY, clearX, clearY, clearW, clearH
    120, 187, 45, 165, 150, 24,
    // Carousel Metric: drawX, drawY, clearX, clearY, clearW, clearH
    120, 168, 58, 165, 129, 20
  },
  // Theme 4: DeskBit (ID 9)
  {
    "/buddy_bit_bg.rle",
    "/buddy_bit_brow.rle",
    "/buddy_bit_eye_o.rle",
    "/buddy_bit_eye_s.rle",
    "Unicode.impact20",   // timeFont
    "Unicode.impact17",   // dateFont
    "GoodTiming20",       // metricFont
    "GoodTiming20",       // weatherFont
    rgb565(255, 255, 255), // timeColor
    rgb565(170, 170, 170), // dateColor
    rgb565(255, 255, 255), // metricColor
    rgb565(255, 255, 255), // weatherColor
    true,   // useRingColorForMetric
    4,      // gazeXLimit
    20,     // gazeYLimit
    3000,   // minBlinkInterval
    7000,   // maxBlinkInterval
    3500,   // minLookInterval
    9000,   // maxLookInterval
    // Time: drawX, drawY, clearX, clearY, clearW, clearH
    71, 101, 33, 127, 85, 26,
    // Date: drawX, drawY, clearX, clearY, clearW, clearH
    167, 131, 128, 128, 78, 24,
    // Weather: drawX, drawY, clearX, clearY, clearW, clearH
    120, 167, 45, 165, 150, 24,
    // Carousel Metric: drawX, drawY, clearX, clearY, clearW, clearH
    120, 188, 58, 165, 129, 20
  }
};


/**
 * Ensure an eye sprite is allocated once in RAM with strict null checking.
 */
static bool ensureEyeSprite(TFT_eSprite &spr) {
  if (!spr.created()) {
    if (spr.createSprite(BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H) == nullptr) {
#if DESKBUDDY_DEBUG
      Serial.printf("[BUDDY] ERROR: Out of heap memory for eye sprite %dx%d!\n", BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H);
#endif
      return false;
    }
  }
  return true;
}

/**
 * Decode 100x100 RLE eye file from LittleFS into a TFT_eSprite.
 * Uses a single fast buffered read and direct RAM pointer writing to achieve ~150x decoding speedup,
 * preventing SPI bus locking and web server unresponsiveness.
 */
static bool loadEyeSprite100(TFT_eSprite &spr, const char *rleFile) {
  if (!ensureEyeSprite(spr)) return false;
  spr.fillSprite(TFT_BLACK);

  fs::File f = LittleFS.open(rleFile, "r");
  if (!f) {
    static char lastWarnedFile[64] = "";
    if (strcmp(lastWarnedFile, rleFile) != 0) {
#if DESKBUDDY_DEBUG
      Serial.printf("[BUDDY] Missing RLE: %s (falling back to default/black)\n", rleFile);
#endif
      strncpy(lastWarnedFile, rleFile, sizeof(lastWarnedFile) - 1);
      lastWarnedFile[sizeof(lastWarnedFile) - 1] = '\0';
    }
    return false;
  }

  uint8_t header[4];
  if (f.read(header, 4) < 4) {
    f.close();
    return false;
  }

  uint16_t w = header[0] | ((uint16_t)header[1] << 8);
  uint16_t h = header[2] | ((uint16_t)header[3] << 8);

  uint8_t chunk[256];
  size_t chunkPos = 0;
  size_t chunkSize = 0;

  int curX = 0, curY = 0;

  while (f.available() || chunkPos < chunkSize) {
    if (chunkPos >= chunkSize) {
      if (!f.available()) break;
      chunkSize = f.read(chunk, sizeof(chunk));
      chunkPos = 0;
      if (chunkSize == 0) break;
    }
    uint8_t hdr = chunk[chunkPos++];
    int count = (hdr & 0x7F) + 1;

    if (hdr & 0x80) {
      if (chunkPos + 1 >= chunkSize && f.available()) {
        // refill buffer if run color bytes cross buffer boundary
        size_t rem = chunkSize - chunkPos;
        if (rem > 0) memmove(chunk, chunk + chunkPos, rem);
        size_t readIn = f.read(chunk + rem, sizeof(chunk) - rem);
        chunkSize = rem + readIn;
        chunkPos = 0;
      }
      if (chunkPos + 1 >= chunkSize) break;
      uint8_t lo = chunk[chunkPos++];
      uint8_t hi = chunk[chunkPos++];
      uint16_t color = lo | ((uint16_t)hi << 8);
      for (int k = 0; k < count; k++) {
        spr.drawPixel(curX, curY, color);
        if (++curX >= (int)w) { curX = 0; curY++; }
      }
    } else {
      for (int k = 0; k < count; k++) {
        if (chunkPos + 1 >= chunkSize && f.available()) {
          size_t rem = chunkSize - chunkPos;
          if (rem > 0) memmove(chunk, chunk + chunkPos, rem);
          size_t readIn = f.read(chunk + rem, sizeof(chunk) - rem);
          chunkSize = rem + readIn;
          chunkPos = 0;
        }
        if (chunkPos + 1 >= chunkSize) break;
        uint8_t lo = chunk[chunkPos++];
        uint8_t hi = chunk[chunkPos++];
        uint16_t color = lo | ((uint16_t)hi << 8);
        spr.drawPixel(curX, curY, color);
        if (++curX >= (int)w) { curX = 0; curY++; }
      }
    }
  }

  f.close();
  appStats.fsReadCount++;
  return true;
}

/**
 * Stamp eye sprite onto the visor canvas at (stampX, stampY).
 * Pure black pixels in the eye sprite are treated as transparent.
 * topSkipRows > 0: blink eyelid — skip drawing that many rows from the top of the eye.
 * The skipped rows remain as the background (from the per-frame visorBgCache memcpy),
 * correctly preserving the background image without any fillRect over it.
 */
static void stampEye100(TFT_eSprite &visor, TFT_eSprite &eyeSpr,
                        int stampX, int stampY, int topSkipRows = 0, bool flipH = false) {
  if (!eyeSpr.created()) return;
  
  const int sw = BUDDY_EYE_SPR_W;
  const int sh = BUDDY_EYE_SPR_H;

  if (topSkipRows <= 0 && !flipH) {
    eyeSpr.pushToSprite(&visor, stampX, stampY, TFT_BLACK);
    return;
  }
  // Partial blink stamp: rows [topSkipRows .. SPR_H) only.
  // Rows above topSkipRows are skipped — background shows through from memcpy.
  int startY = (topSkipRows > 0) ? topSkipRows : 0;
  if (startY >= sh) return;
  for (int ey = startY; ey < sh; ey++) {
    int ty = stampY + ey;
    if (ty < 0 || ty >= BUDDY_SPR_H) continue;
    for (int ex = 0; ex < sw; ex++) {
      int tx = stampX + ex;
      if (tx < 0 || tx >= BUDDY_SPR_W) continue;
      // If flipH is true, map source coordinate to mirror column
      int srcX = flipH ? ((sw - 1) - ex) : ex;
      uint16_t c = eyeSpr.readPixel(srcX, ey);
      if (c != TFT_BLACK) visor.drawPixel(tx, ty, c);
    }
  }
}

/**
 * Push browSprite rotated by angle degrees (TFT CW convention) onto dst at its
 * current setPivot() landing position using NEAREST-NEIGHBOUR sampling.
 *
 * Unlike TFT_eSPI pushRotated() which uses bilinear interpolation, this function
 * uses a direct reverse-transform per output pixel + exact transparent colour
 * check. This prevents the anti-aliased edge pixels that are not exactly TFT_BLACK
 * from showing as dark halos around the brow stroke.
 *
 * dpx, dpy: canvas pixel coordinate where the sprite CENTER should land
 *           (equivalent to what you would pass to dst.setPivot() before pushRotated).
 */
static void pushBrowRotated(TFT_eSprite &dst, TFT_eSprite &src,
                            int dpx, int dpy,
                            float angle, uint16_t transp, bool flipH = false) {
  if (!src.created() || !dst.created()) return;

  float rad  = angle * (PI / 180.0f);
  float cosA = cosf(rad);
  float sinA = sinf(rad);

  int sw = src.width();
  int sh = src.height();
  float scx = (float)sw / 2.0f;
  float scy = (float)sh / 2.0f;

  int radius = (int)sqrtf((float)(sw * sw + sh * sh) / 4.0f) + 1;

  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      // Find source coordinate (fractional float)
      float sxf = cosA * (float)dx + sinA * (float)dy + scx;
      float syf = -sinA * (float)dx + cosA * (float)dy + scy;

      if (flipH) {
        sxf = (float)(sw - 1) - sxf;
      }

      // Check bounds
      if (sxf < 0.0f || sxf >= (float)sw || syf < 0.0f || syf >= (float)sh) continue;

      // Bilinear interpolation
      int x0 = (int)floorf(sxf);
      int y0 = (int)floorf(syf);
      int x1 = x0 + 1;
      int y1 = y0 + 1;

      float wx1 = sxf - (float)x0;
      float wy1 = syf - (float)y0;
      float wx0 = 1.0f - wx1;
      float wy0 = 1.0f - wy1;

      // Clamp coordinates to safe range
      if (x0 < 0) x0 = 0; if (x0 >= sw) x0 = sw - 1;
      if (x1 < 0) x1 = 0; if (x1 >= sw) x1 = sw - 1;
      if (y0 < 0) y0 = 0; if (y0 >= sh) y0 = sh - 1;
      if (y1 < 0) y1 = 0; if (y1 >= sh) y1 = sh - 1;

      // Read 4 neighboring pixels
      uint16_t c00 = src.readPixel(x0, y0);
      uint16_t c10 = src.readPixel(x1, y0);
      uint16_t c01 = src.readPixel(x0, y1);
      uint16_t c11 = src.readPixel(x1, y1);

      // Extract RGB565 channels
      float r00 = (float)((c00 >> 11) & 0x1F);
      float g00 = (float)((c00 >> 5)  & 0x3F);
      float b00 = (float)(c00         & 0x1F);

      float r10 = (float)((c10 >> 11) & 0x1F);
      float g10 = (float)((c10 >> 5)  & 0x3F);
      float b10 = (float)(c10         & 0x1F);

      float r01 = (float)((c01 >> 11) & 0x1F);
      float g01 = (float)((c01 >> 5)  & 0x3F);
      float b01 = (float)(c01         & 0x1F);

      float r11 = (float)((c11 >> 11) & 0x1F);
      float g11 = (float)((c11 >> 5)  & 0x3F);
      float b11 = (float)(c11         & 0x1F);

      // Bilinear blend weights
      float w00 = wx0 * wy0;
      float w10 = wx1 * wy0;
      float w01 = wx0 * wy1;
      float w11 = wx1 * wy1;

      // Interpolate channels
      uint16_t r = (uint16_t)(r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11);
      uint16_t g = (uint16_t)(g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11);
      uint16_t b = (uint16_t)(b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11);

      // Clamp values
      if (r > 31) r = 31;
      if (g > 63) g = 63;
      if (b > 31) b = 31;

      // Calculate pixel intensity (0.0 = fully transparent, 1.0 = fully opaque)
      // Since the background of the sprite is black, the brightness of the pixel
      // represents its opacity. We normalize the 6-bit green channel.
      float alpha = (float)g / 63.0f;
      if (alpha < 0.05f) continue; // Skip fully/nearly transparent pixels

      if (alpha >= 0.95f) {
        // Fully opaque: write directly without reading background
        uint16_t color = (r << 11) | (g << 5) | b;
        dst.drawPixel(dpx + dx, dpy + dy, color);
      } else {
        // Semi-transparent edge: blend with the background pixel
        uint16_t bg = dst.readPixel(dpx + dx, dpy + dy);
        
        float bg_r = (float)((bg >> 11) & 0x1F);
        float bg_g = (float)((bg >> 5)  & 0x3F);
        float bg_b = (float)(bg         & 0x1F);

        // Premultiplied alpha blend channels (preserves original sprite color brightness)
        uint16_t out_r = (uint16_t)((float)r + bg_r * (1.0f - alpha));
        uint16_t out_g = (uint16_t)((float)g + bg_g * (1.0f - alpha));
        uint16_t out_b = (uint16_t)((float)b + bg_b * (1.0f - alpha));

        // Clamp blended values
        if (out_r > 31) out_r = 31;
        if (out_g > 63) out_g = 63;
        if (out_b > 31) out_b = 31;

        uint16_t blended_color = (out_r << 11) | (out_g << 5) | out_b;
        dst.drawPixel(dpx + dx, dpy + dy, blended_color);
      }
    }
  }
}



static TFT_eSprite buddyEyeSpr(&tft);
static TFT_eSprite visorBgCache(&tft);    // Visor background cache (160x98 @ 16bpp = 31.3KB)
static TFT_eSprite browSprite(&tft);       // Brow RLE sprite (BUDDY_BROW_W x BUDDY_BROW_H, <1KB RAM)
static bool        browSpriteLoaded = false;
static bool        deskbuddyFirstRun = true;
static int         deskbuddyLastFontIdx = -1;

/**
 * Deallocate DeskBuddy eye sprite, visor canvas and background cache to free RAM when switching clock faces.
 */
void cleanupDeskbuddySprites() {
  deskbuddyFirstRun = true; // Reset the entrance animation flag for next load
  tft.unloadFont();         // Unload loaded font to release RAM
  deskbuddyLastFontIdx = -1; // Force font reload next time faceplate runs
  if (buddyEyeSpr.created())    buddyEyeSpr.deleteSprite();
  if (centerBgSprite.created()) centerBgSprite.deleteSprite();
  if (visorBgCache.created())   visorBgCache.deleteSprite();
  if (browSprite.created())     browSprite.deleteSprite();
  browSpriteLoaded = false;
  Serial.println("[SPRITES] Deskbuddy sprites (eye+visor+bgCache+brow) deallocated.");
}

/**
 * Release the current faceplate's RAM sprites so a large transient allocation
 * (e.g. the AI TLS handshake) has contiguous heap. The display self-heals by
 * re-initializing the sprites on the next frame after aiTlsInProgress clears.
 */
void releaseFaceplateSprites() {
  tft.unloadFont();
  if (appConfig.clockFace == 4) {
    if (hourHandSprite.created())    hourHandSprite.deleteSprite();
    if (minuteHandSprite.created())  minuteHandSprite.deleteSprite();
    if (secondHandSprite.created())  secondHandSprite.deleteSprite();
    if (centerBgSprite.created())    centerBgSprite.deleteSprite();
  } else if (appConfig.clockFace >= 5 && appConfig.clockFace <= 9) {
    cleanupDeskbuddySprites();
  }
}

/**
 * Allocate the visor canvas sprite (220x105), eye sprite (90x90) and split background cache strips.
 */
void initDeskbuddySprite(const DeskbuddyThemeConfig &cfg) {
  if (centerBgSprite.created() && (centerBgSprite.width() != BUDDY_SPR_W || centerBgSprite.height() != BUDDY_SPR_H)) {
    centerBgSprite.deleteSprite();
  }
  if (!centerBgSprite.created()) {
    centerBgSprite.setColorDepth(16);
#if DESKBUDDY_DEBUG
    Serial.printf("[BUDDY] Allocating visor canvas %dx%d @ 16bpp...\n", BUDDY_SPR_W, BUDDY_SPR_H);
#endif
    if (centerBgSprite.createSprite(BUDDY_SPR_W, BUDDY_SPR_H) == nullptr)
      Serial.println("[BUDDY] ERROR: centerBgSprite alloc failed!");
    else centerBgSprite.fillSprite(TFT_BLACK);
  }

  if (buddyEyeSpr.created() && (buddyEyeSpr.width() != BUDDY_EYE_SPR_W || buddyEyeSpr.height() != BUDDY_EYE_SPR_H)) {
    buddyEyeSpr.deleteSprite();
  }
  if (!buddyEyeSpr.created()) {
    buddyEyeSpr.setColorDepth(16);
#if DESKBUDDY_DEBUG
    Serial.printf("[BUDDY] Allocating eye sprite %dx%d @ 16bpp...\n", BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H);
#endif
    if (buddyEyeSpr.createSprite(BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H) == nullptr)
      Serial.println("[BUDDY] ERROR: buddyEyeSpr alloc failed!");
    else buddyEyeSpr.fillSprite(TFT_BLACK);
  }

  if (visorBgCache.created() && (visorBgCache.width() != BUDDY_SPR_W || visorBgCache.height() != BUDDY_SPR_H)) {
    visorBgCache.deleteSprite();
  }
  if (!visorBgCache.created()) {
    visorBgCache.setColorDepth(16);
#if DESKBUDDY_DEBUG
    Serial.printf("[BUDDY] Allocating visorBgCache (%dx%d @ 16bpp)...\n", BUDDY_SPR_W, BUDDY_SPR_H);
#endif
    if (visorBgCache.createSprite(BUDDY_SPR_W, BUDDY_SPR_H) == nullptr)
      Serial.println("[BUDDY] ERROR: visorBgCache alloc failed!");
    else visorBgCache.fillSprite(TFT_BLACK);
  }
  // Load brow RLE sprite once from LittleFS into RAM.
  if (!browSprite.created()) {
    browSprite.setColorDepth(16);
    if (browSprite.createSprite(BUDDY_BROW_W, BUDDY_BROW_H) != nullptr) {
      browSprite.fillSprite(TFT_BLACK);
      const char* browPath = getRlePathOrFallback(cfg.browRle, "/buddy_brow.rle");
      browSpriteLoaded = drawFullRLEToSprite(browSprite, browPath);
#if DESKBUDDY_DEBUG
      Serial.printf(browSpriteLoaded
        ? "[BUDDY] %s loaded into browSprite.\n"
        : "[BUDDY] %s missing — brow rendering disabled until file is added.\n", browPath);
#endif
    } else {
      Serial.println("[BUDDY] ERROR: browSprite alloc failed!");
    }
  }
}


/**
 * SECTION 7: DESKBUDDY FACEPLATE
 */
void drawDeskbuddyFaceplate(unsigned long now, bool forceRedraw,
                             bool showEvent, const String &message,
                             bool isAi, bool wifiAvailable,
                             bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;

  // -- Resolve Active Theme Config -------------------------------------------
  int themeIdx = appConfig.clockFace - 5;
  if (themeIdx < 0 || themeIdx > 4) themeIdx = 0;
  const DeskbuddyThemeConfig &cfg = deskbuddyThemes[themeIdx];

  // -- Event / message overlay -----------------------------------------------
  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage(nullptr, message, TFT_WHITE, MSG_FONT_BUDDY,
                           isAi, TFT_LIGHTGREY, TFT_BLACK);
    }
    wasEvent = true;
    return;
  }

  if (wasEvent) {
    forceRedraw = true;
    wasEvent = false;
    deskbuddyFirstRun = true; // Trigger slide-up animation when returning from message popup
  }

  // -- Lazy visor canvas & eye sprite init (Bug 3 fix: guard prevents per-frame re-init cost) ---
  if (forceRedraw || !centerBgSprite.created()) {
    initDeskbuddySprite(cfg);
  }

  // -- RAM Eye Sprite & Background tracking ------------------------------------
  static int  loadedEyeMode        = -1;
  static int  loadedRightEyeMode   = -1;  // Bug 4 fix: track right eye separately to skip redundant flash reads
  static bool hasDeskbuddyBgImage  = false;

  // -- Eye Cache Invalidation on Faceplate Change -----------------------------
  static int lastFaceplateIndex = -1;
  int currentFaceIndex = appConfig.clockFace;
  if (currentFaceIndex != lastFaceplateIndex) {
    loadedEyeMode = -1;
    loadedRightEyeMode = -1;
    lastFaceplateIndex = currentFaceIndex;
    deskbuddyFirstRun = true;
  }

  // -- Resolved Paths & Font existence caches (avoids continuous LittleFS.exists checks on every frame) --
  static const char* resolvedEyeOpen   = "/buddy_eye_o.rle";
  static const char* resolvedEyeSquint = "/buddy_eye_s.rle";
  static bool hasTimeFont      = false;
  static bool hasDateFont      = false;
  static bool hasMetricFont    = false;
  static bool hasWeatherFont   = false;

  if (forceRedraw || currentFaceIndex != lastFaceplateIndex) {
    resolvedEyeOpen   = getRlePathOrFallback(cfg.eyeOpenRle, "/buddy_eye_o.rle");
    resolvedEyeSquint = getRlePathOrFallback(cfg.eyeSquintRle, "/buddy_eye_s.rle");
    hasTimeFont      = LittleFS.exists("/" + String(cfg.timeFont) + ".vlw");
    hasDateFont      = LittleFS.exists("/" + String(cfg.dateFont) + ".vlw");
    hasMetricFont    = LittleFS.exists("/" + String(cfg.metricFont) + ".vlw");
    hasWeatherFont   = LittleFS.exists("/" + String(cfg.weatherFont) + ".vlw");
  }

  // -- Animation State & Timers ----------------------------------------------
  static unsigned long nextBlink        = 0;
  static unsigned long blinkStart       = 0;
  static unsigned long rightBlinkOffset = 0;
  static unsigned long nextLook         = 0;

  // 100ms Digital Glitch State (~2 times per minute)
  static unsigned long nextGlitchCheck  = 0;
  static unsigned long glitchStart      = 0;
  static bool          isGlitching      = false;
  static int           glitchJitterX    = 0;
  static int           glitchJitterY    = 0;

  // 2-Second Spontaneous Mode Cameos (Triggers every 20-25s for 2.0 sec)
  static unsigned long nextCameoCheck   = 0;
  static unsigned long cameoStart       = 0;
  static bool          isCameoActive    = false;
  static uint8_t       cameoMode        = EYE_MODE_SQUINT;

  static float         lookX            = 0.0f;
  static float         lookY            = 100.0f; // Starts down at screen Y ~200 and rises on load
  static float         lookTargetX      = 0.0f;
  static float         convergenceX     = 0.0f;  // Eye closeness shift (0px..6px)

  // Autonomous Eyebrow Mood
  static float         browLiftL       = 0.0f;
  static float         browLiftR       = 0.0f;
  static float         browTiltL       = 0.0f;
  static float         browTiltR       = 0.0f;
  static float         browInset       = 0.0f;
  static float         browLiftLTarget = 0.0f;
  static float         browLiftRTarget = 0.0f;
  static float         browTiltLTarget = 0.0f;
  static float         browTiltRTarget = 0.0f;
  static float         browInsetTarget = 0.0f;
  static unsigned long nextBrowCheck   = 0;
  static unsigned long browHoldEnd     = 0;
  static bool          browHolding     = false;

  if (deskbuddyFirstRun) {
    nextBlink       = now + cfg.minBlinkInterval + (unsigned long)random(cfg.maxBlinkInterval - cfg.minBlinkInterval);
    nextLook        = now + cfg.minLookInterval + (unsigned long)random(cfg.maxLookInterval - cfg.minLookInterval);
    nextGlitchCheck = now + 15000UL + (unsigned long)random(15000);
    nextCameoCheck  = now + 18000UL + (unsigned long)random(10000); // First cameo in 18-28s
    nextBrowCheck   = now + 10000UL + (unsigned long)random(8000);  // First brow mood in 10-18s
    lookY           = 100.0f;                                       // Force eye slide-up trigger
    deskbuddyFirstRun = false;
  }

  if (forceRedraw) {
    const char* bgPath = getRlePathOrFallback(cfg.bgRle, "/buddy_bg.rle");
    hasDeskbuddyBgImage = drawRLEImage(bgPath, 0, 0);
    if (hasDeskbuddyBgImage) {
      if (visorBgCache.created()) {
        drawRLEImageToSprite(visorBgCache, bgPath, BUDDY_SPR_X, BUDDY_SPR_Y, BUDDY_SPR_W, BUDDY_SPR_H);
      }
#if DESKBUDDY_DEBUG
      Serial.printf("[BUDDY] Visor background cache (%dx%d) decoded.\n", BUDDY_SPR_W, BUDDY_SPR_H);
#endif
    }
    loadedEyeMode = -1;
  }

  // ── Determine Primary Eye Mode from Presence State ───────────────────────
  uint8_t primaryEyeMode = EYE_MODE_OPEN;

  // ── Exclude Focus / Closed Mode from Glitches & Cameos ───────────────────
  bool allowCameosAndGlitches = (primaryEyeMode != EYE_MODE_CLOSED);

  // Active mode is primaryEyeMode (cameos disabled)
  uint8_t activeEyeMode = primaryEyeMode;

  // ── Synchronized Blink (ONLY HAPPENS IN OPEN MODE!) ──────────────────────
  bool canBlink = (activeEyeMode == EYE_MODE_OPEN);

  if (canBlink && blinkStart == 0 && now >= nextBlink) {
    blinkStart       = now;
    nextBlink        = now + cfg.minBlinkInterval + (unsigned long)random(cfg.maxBlinkInterval - cfg.minBlinkInterval);
    rightBlinkOffset = (random(100) < 20) ? (10UL + random(15)) : 0UL;
  }
  if (blinkStart != 0 && now - blinkStart >= (220UL + rightBlinkOffset)) {
    blinkStart = 0;
  }

  float leftBlinkPct = 0.0f;
  float rightBlinkPct = 0.0f;

  if (canBlink && blinkStart != 0) {
    unsigned long elapsedL = now - blinkStart;
    if (elapsedL < 60)       leftBlinkPct = (float)elapsedL / 60.0f;
    else if (elapsedL < 140) leftBlinkPct = 1.0f;
    else if (elapsedL < 220) leftBlinkPct = 1.0f - ((float)(elapsedL - 140) / 80.0f);

    if (now >= (blinkStart + rightBlinkOffset)) {
      unsigned long elapsedR = now - (blinkStart + rightBlinkOffset);
      if (elapsedR < 60)       rightBlinkPct = (float)elapsedR / 60.0f;
      else if (elapsedR < 140) rightBlinkPct = 1.0f;
      else if (elapsedR < 220) rightBlinkPct = 1.0f - ((float)(elapsedR - 140) / 80.0f);
    }
  }

  // Determine RLE file path for Left & Right eyes (paired by default)
  const char* leftRLE  = resolvedEyeOpen;
  const char* rightRLE = leftRLE;
  int leftModeID  = activeEyeMode;
  int rightModeID = activeEyeMode;

  {
    switch (activeEyeMode) {
      case EYE_MODE_CLOSED:
      case EYE_MODE_HALFED:
      case EYE_MODE_OPEN:
      default:
        leftRLE = resolvedEyeOpen;
        rightRLE = leftRLE;
        break;
      case EYE_MODE_SQUINT:
        leftRLE = resolvedEyeSquint;
        rightRLE = leftRLE;
        break;
    }
  }

  // ── Gaze / Radar Distance Physics ─────────────────────────────────────────
  // 1. Horizontal Gaze: Halved (±cfg.gazeXLimit drift)
  if (now >= nextLook) {
    lookTargetX = (float)random(-cfg.gazeXLimit, cfg.gazeXLimit + 1);
    nextLook    = now + cfg.minLookInterval + (unsigned long)random(cfg.maxLookInterval - cfg.minLookInterval);
  }

  // 2. Vertical Gaze: cfg.gazeYLimit amplitude with resting offset above 0 (-6px)
  int rawDist = appState.rawDetectionDist;
  float lookTargetY = -6.0f; // Default resting position above 0
  float targetConvergence = 0.0f;

  if (rawDist > 0) {
    if (rawDist <= 45) {
      lookTargetY       = -(float)cfg.gazeYLimit; // (top of visor)
      targetConvergence = 6.0f;                           // 6px closer together
    } else if (rawDist < 120) {
      float t = (float)(rawDist - 45) / 75.0f;             // 0.0 at 45cm -> 1.0 at 120cm
      lookTargetY       = -(float)cfg.gazeYLimit * (1.0f - t) + (-6.0f * t);
      targetConvergence = 6.0f * (1.0f - t);
    } else {
      lookTargetY       = -6.0f;                           // Resting position above 0
      targetConvergence = 0.0f;
    }
  } else {
    lookTargetY       = -6.0f;                             // Idle resting position above 0
    targetConvergence = 0.0f;
  }

  lookX        += (lookTargetX - lookX) * 0.12f;
  lookY        += (lookTargetY - lookY) * 0.15f;
  convergenceX += (targetConvergence - convergenceX) * 0.12f;

  // ── Render Visor Frame at ~30 fps (Pure TFT_BLACK canvas, no box) ─────────
  static unsigned long lastEyeFrame = 0;
  bool eyeUpdate = forceRedraw || isGlitching || isCameoActive || (now - lastEyeFrame >= 33UL);

  if (eyeUpdate) {
    lastEyeFrame = now;
    uint16_t visorBg = TFT_BLACK;

    if (centerBgSprite.created()) {
      centerBgSprite.fillSprite(TFT_BLACK);

      int gx = (int)lookX + glitchJitterX;
      int gy = (int)lookY + glitchJitterY;
      int cx = (int)convergenceX;

      int leftX  = (BUDDY_EYE_LX_BASE + cx) - BUDDY_EYE_SPR_W / 2 + gx;
      int leftY  = BUDDY_EYE_CY - BUDDY_EYE_SPR_H / 2 + gy;
      int rightX = (BUDDY_EYE_RX_BASE - cx) - BUDDY_EYE_SPR_W / 2 + gx;
      int rightY = BUDDY_EYE_CY - BUDDY_EYE_SPR_H / 2 + gy;

      // 2. Load & stamp Left Eye (Bug 4 fix: only reload on actual mode change)
      if (loadedEyeMode != leftModeID) {
        if (loadEyeSprite100(buddyEyeSpr, leftRLE)) {
          loadedEyeMode      = leftModeID;
          loadedRightEyeMode = -1;
        }
      }
      int leftTopLidH  = (activeEyeMode == EYE_MODE_OPEN) ? (int)(leftBlinkPct  * 65.0f) : 0;
      int rightTopLidH = (activeEyeMode == EYE_MODE_OPEN) ? (int)(rightBlinkPct * 65.0f) : 0;
      stampEye100(centerBgSprite, buddyEyeSpr, leftX, leftY, leftTopLidH);

      // 3. Stamp Right Eye (Bug 4 fix: separate mode tracker avoids reloading every glitch frame)
      if (strcmp(leftRLE, rightRLE) != 0) {
        if (loadedRightEyeMode != rightModeID) {
          if (loadEyeSprite100(buddyEyeSpr, rightRLE)) {
            loadedRightEyeMode = rightModeID;
          }
        }
        stampEye100(centerBgSprite, buddyEyeSpr, rightX, rightY, rightTopLidH, true);
        loadedEyeMode = -1;
      } else {
        stampEye100(centerBgSprite, buddyEyeSpr, rightX, rightY, rightTopLidH, true);
      }

      // 4. BG overlay: composite single visorBgCache ON TOP of eyes (to mask/clip them)
      if (hasDeskbuddyBgImage && visorBgCache.created()) {
        visorBgCache.pushToSprite(&centerBgSprite, 0, 0, TFT_BLACK);
      }

      // 5. Draw expressive eyebrows over eyes and the visor frame (ONLY IN OPEN MODE!)
      if (activeEyeMode == EYE_MODE_OPEN) {
        bool browAtRest = (fabsf(browLiftL) < 0.01f && fabsf(browLiftR) < 0.01f &&
                           fabsf(browTiltL) < 0.01f && fabsf(browTiltR) < 0.01f &&
                           fabsf(browInset) < 0.01f);
        if (!browHolding && browAtRest && now >= nextBrowCheck) {
          int roll = (int)random(100);
          if (roll < 35) {
            int subRoll = (int)random(100);
            if (subRoll < 40) {
              browLiftLTarget = 0.35f; browLiftRTarget = 0.35f;
              browTiltLTarget = 1.0f;  browTiltRTarget = 1.0f;
              browInsetTarget = 0.3f;
            } else if (subRoll < 75) {
              browLiftLTarget = 0.5f;  browLiftRTarget = 0.5f;
              browTiltLTarget = -0.7f; browTiltRTarget = -0.7f;
              browInsetTarget = -0.3f;
            } else {
              bool leftUp     = (random(2) == 0);
              browLiftLTarget = leftUp ? 0.6f : 0.2f;
              browLiftRTarget = leftUp ? 0.2f : 0.6f;
              browTiltLTarget = leftUp ? -0.8f : 0.8f;
              browTiltRTarget = leftUp ? 0.8f : -0.8f;
              browInsetTarget = 0.0f;
            }
            browHoldEnd     = now + 2000UL + (unsigned long)random(58000);
            browHolding     = true;
            nextBrowCheck   = now + 12000UL + (unsigned long)random(10000);
          } else if (roll < 55) {
            browLiftLTarget = 1.0f;  browLiftRTarget = 1.0f;
            browTiltLTarget = 0.0f;  browTiltRTarget = 0.0f;
            browInsetTarget = 0.0f;
            browHoldEnd     = now + 1000UL + (unsigned long)random(29000);
            browHolding     = true;
            nextBrowCheck   = now + 25000UL + (unsigned long)random(20000);
          } else if (roll < 75) {
            bool leftSide   = (random(2) == 0);
            browLiftLTarget = leftSide ? 1.0f : 0.15f;
            browLiftRTarget = leftSide ? 0.15f : 1.0f;
            browTiltLTarget = leftSide ? -0.6f : 0.0f;
            browTiltRTarget = leftSide ? 0.0f : -0.6f;
            browInsetTarget = 0.0f;
            browHoldEnd     = now + 2000UL + (unsigned long)random(58000);
            browHolding     = true;
            nextBrowCheck   = now + 18000UL + (unsigned long)random(15000);
          } else {
            nextBrowCheck   = now + 8000UL + (unsigned long)random(8000);
          }
        }

        if (browHolding && now >= browHoldEnd) {
          browLiftLTarget = 0.0f;  browLiftRTarget = 0.0f;
          browTiltLTarget = 0.0f;  browTiltRTarget = 0.0f;
          browInsetTarget = 0.0f;
          browHolding     = false;
        }

        browLiftL += (browLiftLTarget - browLiftL) * 0.28f;
        browLiftR += (browLiftRTarget - browLiftR) * 0.28f;
        browTiltL += (browTiltLTarget - browTiltL) * 0.28f;
        browTiltR += (browTiltRTarget - browTiltR) * 0.28f;
        browInset += (browInsetTarget - browInset) * 0.28f;

        if (browLiftLTarget == 0.0f && fabsf(browLiftL) < 0.005f) browLiftL = 0.0f;
        if (browLiftRTarget == 0.0f && fabsf(browLiftR) < 0.005f) browLiftR = 0.0f;
        if (browTiltLTarget == 0.0f && fabsf(browTiltL) < 0.005f) browTiltL = 0.0f;
        if (browTiltRTarget == 0.0f && fabsf(browTiltR) < 0.005f) browTiltR = 0.0f;
        if (browInsetTarget == 0.0f && fabsf(browInset) < 0.005f) browInset = 0.0f;

        if (browSpriteLoaded && browSprite.created()) {
          const float HS        = 18.0f;
          const float LIFT_MAX  = 10.0f;
          const float TILT_MAX  =  5.0f;
          const float INSET_MAX = 10.0f;

          float insetPx  = browInset * INSET_MAX;
          float neutralY = (float)(BUDDY_EYE_CY - 31);
          int   dipL     = (int)(leftBlinkPct  * 3.0f);
          int   dipR     = (int)(rightBlinkPct * 3.0f);

          float leftAngle  =  atan2f(browTiltL * TILT_MAX, (float)(BUDDY_BROW_W / 2)) * (180.0f / PI);
          float rightAngle = -atan2f(browTiltR * TILT_MAX, (float)(BUDDY_BROW_W / 2)) * (180.0f / PI);

          int leftPivotX  = (int)((float)(BUDDY_EYE_LX_BASE + cx + gx) + insetPx + 4.0f);
          int leftPivotY  = (int)(neutralY + dipL - browLiftL * LIFT_MAX) + gy;
          pushBrowRotated(centerBgSprite, browSprite, leftPivotX, leftPivotY, leftAngle, TFT_BLACK, false);

          int rightPivotX = (int)((float)(BUDDY_EYE_RX_BASE - cx + gx) - insetPx - 4.0f);
          int rightPivotY = (int)(neutralY + dipR - browLiftR * LIFT_MAX) + gy;
          pushBrowRotated(centerBgSprite, browSprite, rightPivotX, rightPivotY, rightAngle, TFT_BLACK, true);
        }
      }

      // 6. Push completed visor patch to TFT (<1ms).
      centerBgSprite.pushSprite(BUDDY_SPR_X, BUDDY_SPR_Y);
    }
  }

  // ── Bottom half: clock / date / rotating stat ────────────────────────────
  static unsigned long lastBottomUpdate = 0;
  static int     last_h       = -1;
  static int     last_m       = -1;
  static String  last_date    = "";
  static String  last_metric  = "";
  static uint16_t lastMetricColor = 0;
  static int     metricIdx    = 0;
  static unsigned long lastMetricSwitch = 0;
  static int     last_temp    = -999;
  static String  last_weatherDesc = "";

  bool bottomUpdate = forceRedraw || (now - lastBottomUpdate >= 500UL);

  if (bottomUpdate) {
    lastBottomUpdate = now;

    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    int display_h = get12hDisplayHour(h);

    tft.setTextDatum(MC_DATUM);

    // 3D glowing divider bar
    uint8_t mr = appState.currentRingColor.r;
    uint8_t mg = appState.currentRingColor.g;
    uint8_t mb = appState.currentRingColor.b;

    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
    String currentDate = String(buf);
    currentDate.toUpperCase();

    // Rotating metric: advance index every 12 seconds
    if (now - lastMetricSwitch > 12000UL) {
      metricIdx = (metricIdx + 1) % 3;
      lastMetricSwitch = now;
    }
    String metricText = "";
    switch (metricIdx) {
      case 0: {
        int pct = getDailyProductivityPct();
        metricText = "DAY: " + String(pct) + "%";
        break;
      }
      case 1:
        metricText = "SCORE: " + String(appStats.productivityScore) + "%";
        break;
      case 2:
        metricText = "FOCUS: " + formatTime(appStats.totalFocusTime);
        break;
    }
    uint16_t metricColor = tft.color565(mr * 70 / 100, mg * 70 / 100, mb * 70 / 100);

    bool timeChanged   = forceRedraw || h != last_h   || m != last_m;
    bool dateChanged   = forceRedraw || currentDate   != last_date;
    bool metricChanged = forceRedraw || metricText    != last_metric || metricColor != lastMetricColor;
    bool weatherChanged = forceRedraw || (appState.temp != last_temp) || (last_weatherDesc != appState.weatherDesc);

    if (hasDeskbuddyBgImage && (timeChanged || dateChanged || metricChanged || weatherChanged)) {

      if (timeChanged) {
        tft.fillRect(cfg.timeClearX, cfg.timeClearY, cfg.timeClearW, cfg.timeClearH, TFT_BLACK);
        tft.setTextColor(cfg.timeColor, TFT_BLACK);
        if (hasTimeFont) {
          tft.loadFont(cfg.timeFont, LittleFS);
          tft.drawString(String(timeStr), cfg.timeX, cfg.timeY);
          tft.unloadFont();
        } else {
          tft.drawString(String(timeStr), cfg.timeX, cfg.timeY, FONT_BUDDY_TIME);
        }
        last_h = h; last_m = m;
      }
      if (dateChanged) {
        tft.fillRect(cfg.dateClearX, cfg.dateClearY, cfg.dateClearW, cfg.dateClearH, TFT_BLACK);
        tft.setTextColor(cfg.dateColor, TFT_BLACK);
        if (hasDateFont) {
          tft.loadFont(cfg.dateFont, LittleFS);
          tft.drawString(currentDate, cfg.dateX, cfg.dateY);
          tft.unloadFont();
        } else {
          tft.drawString(currentDate, cfg.dateX, cfg.dateY, FONT_BUDDY_DATE);
        }
        last_date = currentDate;
      }
      if (metricChanged) {
        tft.fillRect(cfg.metricClearX, cfg.metricClearY, cfg.metricClearW, cfg.metricClearH, TFT_BLACK);
        tft.setTextColor(cfg.useRingColorForMetric ? metricColor : cfg.metricColor, TFT_BLACK);
        if (hasMetricFont) {
          tft.loadFont(cfg.metricFont, LittleFS);
          tft.drawString(metricText, cfg.metricX, cfg.metricY);
          tft.unloadFont();
        } else {
          tft.drawString(metricText, cfg.metricX, cfg.metricY, FONT_BUDDY_DATE);
        }
        last_metric     = metricText;
        lastMetricColor = metricColor;
      }
      if (weatherChanged) {
        tft.fillRect(cfg.weatherClearX, cfg.weatherClearY, cfg.weatherClearW, cfg.weatherClearH, TFT_BLACK);
        tft.setTextColor(cfg.useRingColorForMetric ? metricColor : cfg.weatherColor, TFT_BLACK);
        String weatherStr = String(appState.temp) + "C | " + appState.weatherDesc;
        weatherStr.toUpperCase();
        if (hasWeatherFont) {
          tft.loadFont(cfg.weatherFont, LittleFS);
          tft.drawString(weatherStr, cfg.weatherX, cfg.weatherY);
          tft.unloadFont();
        } else {
          tft.drawString(weatherStr, cfg.weatherX, cfg.weatherY, FONT_BUDDY_DATE);
        }
        last_temp = appState.temp;
        last_weatherDesc = appState.weatherDesc;
      }

    } else if (!hasDeskbuddyBgImage) {
      // Fallback: no background image — built-in fonts, no heap allocation needed
      if (timeChanged) {
        tft.fillRect(28, 128, 184, 46, TFT_BLACK);
        tft.setTextColor(cfg.timeColor, TFT_BLACK);
        tft.drawString(String(timeStr), 120, 152, FONT_BUDDY_TIME);
        last_h = h; last_m = m;
      }
      if (dateChanged) {
        tft.fillRect(28, 177, 184, 18, TFT_BLACK);
        tft.setTextColor(cfg.dateColor, TFT_BLACK);
        tft.drawString(currentDate, 120, 186, FONT_BUDDY_DATE);
        last_date = currentDate;
      }
      if (metricChanged) {
        tft.fillRect(28, 200, 184, 20, TFT_BLACK);
        tft.setTextColor(cfg.useRingColorForMetric ? metricColor : cfg.metricColor, TFT_BLACK);
        tft.drawString(metricText, 120, 210, FONT_BUDDY_DATE);
        last_metric     = metricText;
        lastMetricColor = metricColor;
      }
      if (weatherChanged) {
        tft.fillRect(28, 40, 184, 20, TFT_BLACK);
        tft.setTextColor(cfg.useRingColorForMetric ? metricColor : cfg.weatherColor, TFT_BLACK);
        String weatherStr = String(appState.temp) + "C | " + appState.weatherDesc;
        weatherStr.toUpperCase();
        tft.drawString(weatherStr, 120, 50, FONT_BUDDY_DATE);
        last_temp = appState.temp;
        last_weatherDesc = appState.weatherDesc;
      }
    }
  }
}

inline bool loadFontExact(const char* name) {
  if (name == nullptr || strlen(name) == 0) return false;
  String path = "/" + String(name) + ".vlw";
  if (LittleFS.exists(path)) {
    tft.loadFont(name, LittleFS);
    return tft.fontLoaded;
  }
  return false;
}

inline int drawDynamicMultiLineText(
    const String& text, 
    int startY, 
    int lineHeight = 16, 
    int maxLines = 12, 
    const char* fontName = nullptr, 
    uint16_t defaultColor = TFT_WHITE, 
    uint16_t bgColor = TFT_BLACK,
    bool truncatePerLine = false,
    const char* subtitleFontName = nullptr,
    int dateRightX = 0,
    int contentLeftX = 0
) {
  tft.setTextDatum(MC_DATUM);

  int y = startY;
  int startIdx = 0;
  int linesDrawn = 0;

  while (startIdx < text.length() && linesDrawn < maxLines) {
    if (y + lineHeight > 225) break; // Circular screen bottom clipping safety

    int yc = y + (lineHeight / 2);
    int dy = yc - 120;
    int maxR = 105;
    int charsForThisLine = DISPLAY_CHARS_PER_LINE;
    if (abs(dy) < maxR) {
      float halfWidth = sqrtf((float)(maxR * maxR - dy * dy));
      charsForThisLine = (int)((halfWidth * 2.0f) / 6.0f);
    }
    if (charsForThisLine < 8) charsForThisLine = 8;

    String line = "";
    int nextNewline = text.indexOf('\n', startIdx);
    if (nextNewline != -1) {
      line = text.substring(startIdx, nextNewline);
      startIdx = nextNewline + 1;
    } else {
      line = text.substring(startIdx);
      startIdx = text.length();
    }

    uint16_t lineCol = defaultColor;
    line.replace("\r", "");
    if (line.indexOf("[RED]") != -1)         { lineCol = tft.color565(239,  68,  68); line.replace("[RED]", ""); }
    else if (line.indexOf("[GREEN]") != -1)  { lineCol = tft.color565( 34, 197,  94); line.replace("[GREEN]", ""); }
    else if (line.indexOf("[YELLOW]") != -1) { lineCol = tft.color565(245, 158,  11); line.replace("[YELLOW]", ""); }
    else if (line.indexOf("[BLUE]") != -1)   { lineCol = tft.color565( 56, 189, 248); line.replace("[BLUE]", ""); }
    else if (line.indexOf("[ORANGE]") != -1) { lineCol = tft.color565(249, 115,  22); line.replace("[ORANGE]", ""); }
    else if (line.indexOf("[GREY]") != -1)   { lineCol = tft.color565(148, 163, 184); line.replace("[GREY]", ""); }
    else if (line.indexOf("[GRAY]") != -1)   { lineCol = tft.color565(148, 163, 184); line.replace("[GRAY]", ""); }
    else if (line.indexOf("[WHITE]") != -1)  { lineCol = TFT_WHITE; line.replace("[WHITE]", ""); }

    line.trim();
    if (line.length() == 0) continue; // Skip blank lines

    bool isSubtitle = (line.indexOf("--") != -1);
    const char* activeFontName = (isSubtitle && subtitleFontName != nullptr && strlen(subtitleFontName) > 0)
                                 ? subtitleFontName 
                                 : fontName;

    if (truncatePerLine) {
      if (line.length() > charsForThisLine) {
        line = line.substring(0, charsForThisLine);
      }
    }

    bool fontLoaded = loadFontExact(activeFontName);
    tft.setTextColor(lineCol, bgColor);

    // Blocked Two-Column Layout (Date Right-Aligned, Content Left-Aligned)
    if (!isSubtitle && dateRightX > 0 && contentLeftX > 0) {
      int pipeIdx = line.indexOf('|');
      if (pipeIdx != -1) {
        String datePart = line.substring(0, pipeIdx);
        String contentPart = line.substring(pipeIdx + 1);
        datePart.trim();
        contentPart.trim();

        String ampmStr = "";
        if (datePart.endsWith("PM")) {
          ampmStr = "P";
          datePart = datePart.substring(0, datePart.length() - 2);
          datePart.trim();
        } else if (datePart.endsWith("P")) {
          ampmStr = "P";
          datePart = datePart.substring(0, datePart.length() - 1);
          datePart.trim();
        } else if (datePart.endsWith("AM")) {
          ampmStr = "A";
          datePart = datePart.substring(0, datePart.length() - 2);
          datePart.trim();
        } else if (datePart.endsWith("A")) {
          ampmStr = "A";
          datePart = datePart.substring(0, datePart.length() - 1);
          datePart.trim();
        }

        // 1. Draw Date / Time Digits (Right Aligned at dateRightX)
        tft.setTextDatum(MR_DATUM);
        if (fontLoaded) tft.drawString(datePart, dateRightX, y);
        else tft.drawString(datePart, dateRightX, y, 1);

        // 2. Draw A/P Suffix (Small RobotoCondensed10, Left Aligned at Column 61, matching time color)
        if (ampmStr.length() > 0) {
          if (fontLoaded) tft.unloadFont();
          bool loadAmpmFont = loadFontExact("RobotoCondensed10");
          tft.setTextDatum(ML_DATUM);
          tft.setTextColor(lineCol, bgColor); // Matched to time color (lineCol)
          tft.drawString(ampmStr, 61, y + 5, loadAmpmFont ? 0 : 1);
          if (loadAmpmFont) tft.unloadFont();
          fontLoaded = loadFontExact(activeFontName);
          tft.setTextColor(lineCol, bgColor);
        }

        // 3. Draw Entry Content (Left Aligned at contentLeftX)
        tft.setTextDatum(ML_DATUM);
        if (fontLoaded) tft.drawString(contentPart, contentLeftX, y);
        else tft.drawString(contentPart, contentLeftX, y, 1);
      } else {
        // Items without date (e.g. Daily tasks): Left Aligned at contentLeftX
        tft.setTextDatum(ML_DATUM);
        if (fontLoaded) tft.drawString(line, contentLeftX, y);
        else tft.drawString(line, contentLeftX, y, 1);
      }
    } else {
      // Standard Centered Line (Subtitles & default text)
      tft.setTextDatum(MC_DATUM);
      if (fontLoaded) tft.drawString(line, 120, y);
      else tft.drawString(line, 120, y, 1);
    }

    if (fontLoaded) {
      tft.unloadFont();
    }

    tft.setTextDatum(MC_DATUM);
    y += lineHeight;
    linesDrawn++;
  }

  return linesDrawn;
}

// ============================================================================
// NON-AI EVENT FACEPLATES (Task Due, Journal Dashboard, Journal Tasks)
// Standardized fonts decoupled from clock faceplates.
// ============================================================================

inline void drawTaskDueFaceplate(const TaskDueViewData& data, uint16_t bgColor, const char* fontNameOverride) {
  // [FONT SELECTOR]: Body Task Name & Time Badge Font
  // Controls: data.taskText and data.dueTimeStr
  const char* fontName = "RobotoCondensed32"; //DUE NOW Body
  uint8_t systemFontIdx = 1;

  tft.fillScreen(bgColor);

  // 1. Header (DUE NOW / OVERDUE - Localized)
  tft.setTextDatum(MC_DATUM);
  uint16_t headerColor = data.isOverdue ? tft.color565(239, 68, 68) : tft.color565(245, 158, 11);
  tft.setTextColor(headerColor, bgColor);

  String headerStr = data.headerText;
  if (headerStr.length() == 0 || headerStr == "DUE NOW" || headerStr == "OVERDUE") {
#if DESKBUDDY_LANG_PTBR
    headerStr = data.isOverdue ? "ATRASADO" : "VENCENDO AGORA";
#else
    headerStr = data.isOverdue ? "OVERDUE" : "DUE NOW";
#endif
  }
  // [FONT SELECTOR]: Top Header Banner Font
  // Controls: "ATRASADO", "VENCENDO AGORA", "OVERDUE", "DUE NOW"
  bool loadHeaderFont = loadFontExact("RobotoCondensed20"); //DUE NOW Title
  tft.drawString(headerStr, 120, 30, loadHeaderFont ? 0 : 1);
  if (loadHeaderFont) tft.unloadFont();

  // 2. Task Name
  bool fontLoaded = loadFontExact(fontName);
  tft.setTextColor(tft.color565(34, 197, 94), bgColor); // GREEN (diagnostic test)
  tft.drawString(data.taskText, 120, 105, fontLoaded ? 0 : systemFontIdx);

  // 3. Time Badge with AM/PM Support
  tft.setTextColor(tft.color565(245, 158, 11), bgColor);
  String timeStr = data.dueTimeStr;
  String ampmStr = "";

  int amIdx = timeStr.indexOf("AM");
  int pmIdx = timeStr.indexOf("PM");
  if (amIdx != -1) {
    ampmStr = "AM";
    timeStr = timeStr.substring(0, amIdx);
    timeStr.trim();
  } else if (pmIdx != -1) {
    ampmStr = "PM";
    timeStr = timeStr.substring(0, pmIdx);
    timeStr.trim();
  }

  // Respect appConfig.time24h setting dynamically
  if (!appConfig.time24h && ampmStr.length() == 0) {
    int colonIdx = timeStr.indexOf(':');
    if (colonIdx != -1) {
      int h = timeStr.substring(0, colonIdx).toInt();
      int m = timeStr.substring(colonIdx + 1).toInt();
      int h12 = h % 12;
      if (h12 == 0) h12 = 12;
      ampmStr = (h >= 12) ? "PM" : "AM";
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", h12, m);
      timeStr = String(buf);
    }
  } else if (appConfig.time24h && ampmStr.length() > 0) {
    int colonIdx = timeStr.indexOf(':');
    if (colonIdx != -1) {
      int h = timeStr.substring(0, colonIdx).toInt();
      int m = timeStr.substring(colonIdx + 1).toInt();
      if (ampmStr == "PM" && h < 12) h += 12;
      if (ampmStr == "AM" && h == 12) h = 0;
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
      timeStr = String(buf);
      ampmStr = "";
    }
  }

  if (ampmStr.length() > 0) {
    tft.setTextDatum(MR_DATUM);
    tft.drawString(timeStr, 135, 195, fontLoaded ? 0 : systemFontIdx);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(tft.color565(148, 163, 184), bgColor);
    tft.drawString(ampmStr, 142, 195, systemFontIdx);
    tft.setTextDatum(MC_DATUM);
  } else {
    tft.drawString(timeStr, 120, 195, fontLoaded ? 0 : systemFontIdx);
  }

  if (fontLoaded) tft.unloadFont();
}

inline void drawJournalDashboardFaceplate(
    const JournalDashboardViewData& data, 
    uint16_t bgColor,
    const char* titleFontOverride,
    const char* labelFontOverride
) {
  // [FONT SELECTORS]: Journal Dashboard Fonts
  // titleFont: Controls top header ("RESUMO TODO" / "TODO SUMMARY")
  // labelFont: Controls all dashboard labels & count values ("ParaHoje:", "Diarias:", etc.)
  const char* titleFont = "RobotoCondensed20"; // Summary Title
  const char* labelFont = "RobotoCondensed20"; // Summary body
  uint8_t systemFontIdx = 1;

  tft.fillScreen(bgColor);

#if DESKBUDDY_LANG_PTBR
  const char* titleText  = "RESUMO TO-DO";
  const char* lblDue     = "PARA HOJE:";
  const char* lblDaily   = "DIÁRIAS:";
  const char* lblMonth   = "MENSAIS:";
  const char* lblScore   = "DILIGÊNCIA:";
#else
  const char* titleText  = "TO-DO SUMMARY";
  const char* lblDue     = "DUE TODAY:";
  const char* lblDaily   = "DAILY:";
  const char* lblMonth   = "MONTHLY:";
  const char* lblScore   = "DILIGENCE:";
#endif

  String displayTitle = (data.titleStr.length() > 0) ? data.titleStr : String(titleText);

  // 1. Header Title
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(245, 158, 11), bgColor);
  bool loadTitleFont = loadFontExact(titleFont);
  tft.drawString(displayTitle, 120, 32, loadTitleFont ? 0 : 1);
  if (loadTitleFont) tft.unloadFont();

  bool loadLabelFont = loadFontExact(labelFont);

  // 2. Labels & Values - Forced Green for diagnostic test
  tft.setTextColor(tft.color565(34, 197, 94), bgColor);

  tft.setTextDatum(ML_DATUM);
  tft.drawString(lblDue,   50,  82, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(lblDaily, 50, 112, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(lblMonth, 50, 142, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(lblScore, 50, 172, loadLabelFont ? 0 : systemFontIdx);

  tft.setTextDatum(ML_DATUM);
  tft.drawString(String(data.dueTodayCount), 180, 82, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(String(data.dailyCount), 180, 112, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(String(data.monthlyCount), 180, 142, loadLabelFont ? 0 : systemFontIdx);
  tft.drawString(String(data.diligenceScore) + "%", 180, 172, loadLabelFont ? 0 : systemFontIdx);

  if (loadLabelFont) tft.unloadFont();
  tft.setTextDatum(MC_DATUM);
}

inline void drawJournalTasksFaceplate(
    const String& rawTaskList, 
    uint16_t bgColor, 
    const char* fontNameOverride, 
    int maxLines,
    int lineHeight
) {
  // ==========================================================================
  // JOURNAL TASK LIST UNIFIED SETTINGS & CONFIGURATION BLOCK
  // ==========================================================================
  
  // 1. [FONT SELECTOR]: Task Item Body Font (e.g. "05/08 | Rent", "Arrumar a cama")
  const char* fontName = "RobotoCondensed17";

  // 2. [FONT SELECTOR]: Section Subtitle Font ("-- DIARIAS --" / "-- MENSAIS --")
  const char* subtitleFont = "RobotoCondensed18";

  // 3. [FONT SELECTOR]: Page Header Title Font ("TAREFAS (1)")
  const char* titleFontName = "RobotoCondensed20";

  // 4. [LINE SPACING SELECTOR]: Vertical pixel step between task list lines
  int taskLineHeight = 22;

  // 5. [LAYOUT SELECTOR]: Top Y position of page title ("TAREFAS (1)")
  int titleTopY = 17;

  // 6. [LAYOUT SELECTOR]: Top Y position of first task list entry (distance below title)
  int bodyStartY = 45;

  // 7. [PAGE BREAK SELECTOR]: Maximum items per page before breaking to a new screen
  //    Note: To change page break limit, edit TASK_LIST_MAX_PAGE_LINES in Constants.h
  //    (Formula: 1 title line + N body items. E.g. 7 = 1 title + 6 items per page)
  //    Current Value: TASK_LIST_MAX_PAGE_LINES = 7

  // ==========================================================================

  tft.fillScreen(bgColor);

  String cleanList = rawTaskList;
  String pageSuffix = "";
  int firstNL = cleanList.indexOf('\n');
  if (firstNL != -1) {
    String firstLine = cleanList.substring(0, firstNL);
    if (firstLine.indexOf("LISTA DE TAREFAS") != -1 || firstLine.indexOf("TASK LIST") != -1) {
      int openParen = firstLine.indexOf('(');
      int closeParen = firstLine.indexOf(')', openParen);
      if (openParen != -1 && closeParen != -1) {
        pageSuffix = " " + firstLine.substring(openParen + 1, closeParen);
      }
      cleanList = cleanList.substring(firstNL + 1);
    }
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(245, 158, 11), bgColor);
  bool loadTitleFont = loadFontExact(titleFontName);
#if DESKBUDDY_LANG_PTBR
  tft.drawString("TAREFAS" + pageSuffix, 120, titleTopY, loadTitleFont ? 0 : 1);
#else
  tft.drawString("TASKS" + pageSuffix, 120, titleTopY, loadTitleFont ? 0 : 1);
#endif
  if (loadTitleFont) tft.unloadFont();

  // 7. [LAYOUT SELECTOR]: Date column right-alignment X position (Column 60)
  int dateRightX = 60;

  // 8. [LAYOUT SELECTOR]: Task content left-alignment X position (Column 80)
  int contentLeftX = 80;

  drawDynamicMultiLineText(cleanList, bodyStartY, taskLineHeight, maxLines, fontName, tft.color565(34, 197, 94), bgColor, true, subtitleFont, dateRightX, contentLeftX);
}

#endif // FACEPLATES_H


