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

// ============================================================================
// SECTION 1: GRAPHICS & ASSETS INTERFACE
// ============================================================================



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
      tft.fillRect(200, 46, 17, 12, TFT_BLACK); // Clear first
      tft.drawRect(200, 46, 17, 12, TFT_YELLOW);
      tft.drawLine(200, 46, 208, 52, TFT_YELLOW);
      tft.drawLine(208, 52, 216, 46, TFT_YELLOW);
    } else {
      tft.fillRect(200, 46, 17, 12, TFT_BLACK);
    }
    lastHasMailDefault = appConfig.hasMail;
  }

  // Time section (center)
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  int display_h = h;
  if (!appConfig.time24h) {
    display_h = h % 12;
    if (display_h == 0) display_h = 12;
  }
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
      int pct = 0;
      if (appConfig.targetHours > 0.0f) {
        pct = (int)((appStats.totalDeskTime * 100.0f) / (appConfig.targetHours * 3600.0f * 1000.0f));
      }
      if (pct > 100) pct = 100;
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
    int display_h = h;
    if (!appConfig.time24h) {
      display_h = h % 12;
      if (display_h == 0) display_h = 12;
    }
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
      tft.fillRect(152, 53, 15, 11, TFT_BLACK);
      tft.drawRect(152, 55, 15, 11, TFT_WHITE);
      tft.drawLine(152, 55, 159, 61, TFT_WHITE);
      tft.drawLine(159, 61, 166, 55, TFT_WHITE);
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
      tft.fillRect(112, 18, 16, 15, HITECH_BG_STATUS); // Clear area safely
      tft.drawRect(112, 20, 15, 11, HITECH_CYAN);
      tft.drawLine(112, 20, 119, 26, HITECH_CYAN);
      tft.drawLine(119, 26, 126, 20, HITECH_CYAN);
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
    
    int display_h = h;
    if (!appConfig.time24h) {
      display_h = h % 12;
      if (display_h == 0) display_h = 12;
    }
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
    tft.drawString(String(timeStr), 117, 56);
    //tft.drawString("22:22", 120, 56);
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
    const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
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
  snprintf(line, sizeof(line), "HEAP:%uK AI:%d/15", freeHeapK, appStats.dailyAiRequestCount);
  drawDevLine(11, line, 198);
}

// ============================================================================
// SECTION 6: AVIATOR FACEPLATE
// ============================================================================

#define COLOR_AVIATOR_ORANGE    tft.color565(235, 94, 40)
#define COLOR_AVIATOR_DARKGRAY  tft.color565(40, 40, 40)
#define COLOR_AVIATOR_OFFWHITE  tft.color565(240, 240, 240)
#define COLOR_TRANSPARENT       TFT_BLACK // Black pixels will be transparent
#define MSG_FONT_AVIATOR        "GoodTiming15"

void initWatchHandSprites() {
  Serial.println("[SPRITES] Allocating Aviator watch hands and center canvas sprite...");

  // 1. Hour Hand Sprite (27x100)
  hourHandSprite.createSprite(27, 100);
  hourHandSprite.fillSprite(COLOR_TRANSPARENT);
  drawFullRLEToSprite(hourHandSprite, "/aviator_hour.rle");
  hourHandSprite.setPivot(13, 85);

  // 2. Minute Hand Sprite (21x120)
  minuteHandSprite.createSprite(21, 120);
  minuteHandSprite.fillSprite(COLOR_TRANSPARENT);
  drawFullRLEToSprite(minuteHandSprite, "/aviator_minute.rle");
  minuteHandSprite.setPivot(10, 109);

  // 3. Second Hand Sprite (9x127)
  secondHandSprite.createSprite(9, 127);
  secondHandSprite.fillSprite(COLOR_TRANSPARENT);
  drawFullRLEToSprite(secondHandSprite, "/aviator_second.rle");
  secondHandSprite.setPivot(4, 110);

  // 4. Center Canvas Patch Sprite (220x220)
  centerBgSprite.createSprite(220, 220);
  centerBgSprite.fillSprite(TFT_BLACK);
}

void drawAviatorClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  static bool wasEvent = false;

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/aviator_msg.rle", message, COLOR_AVIATOR_ORANGE, MSG_FONT_AVIATOR, isAi, TFT_LIGHTGREY, TFT_BLACK);
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
  if (!hourHandSprite.created() || !centerBgSprite.created()) {
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
  int pct = 0;
  if (appConfig.targetHours > 0.0f) {
    pct = (int)((appStats.totalDeskTime * 100.0f) / (appConfig.targetHours * 3600.0f * 1000.0f));
  }
  if (pct > 100) pct = 100;

  // 3. Render and cache background + all text overlays + hour/minute hands in RAM canvas on change
  bool updateCanvas = forceRedraw || (m != last_min) || (appState.temp != last_temp) || (ts.tm_mday != last_mday) || (pct != last_pct);

  if (updateCanvas) {
    // Refresh center background slice in RAM (220x220 centered at 10,10)
    drawRLEImageToSprite(centerBgSprite, "/aviator_bg.rle", 10, 10, 220, 220);
    
    // Draw weather (TFT Y=54 -> relative Y=44)
    centerBgSprite.setTextColor(TFT_WHITE);
    centerBgSprite.setTextDatum(MC_DATUM);
    centerBgSprite.drawString(String(appState.temp) + "C", 110, 44, 2);

    // Draw Date Badge (TFT X=70, Y=68 -> relative X=60, Y=58)
    char dayStr[3];
    snprintf(dayStr, sizeof(dayStr), "%d", ts.tm_mday);
    centerBgSprite.drawString(dayStr, 60, 58, 2);

    // Draw Top Month/Day (TFT Y=74 -> relative Y=64)
    char monthDayStr[12];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    snprintf(monthDayStr, sizeof(monthDayStr), "%02d %s", ts.tm_mday, months[ts.tm_mon]);
    centerBgSprite.drawString(monthDayStr, 110, 64, 2);

    // Draw Digital Time (HH:MM) centered at Y=93 (relative Y=83)
    int display_h = h;
    if (!appConfig.time24h) {
      display_h = h % 12;
      if (display_h == 0) display_h = 12;
    }
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
    centerBgSprite.drawString(timeStr, 110, 83, 4);

    // Draw Focus Progress (TFT Y=182 -> relative Y=172)
    char stepsGoalStr[32];
    snprintf(stepsGoalStr, sizeof(stepsGoalStr), "FOCUS: %d%%", pct);
    centerBgSprite.setTextColor(TFT_LIGHTGREY);
    centerBgSprite.drawString(stepsGoalStr, 110, 172, 2);

    // Set pivot of centerBgSprite relative to its center (110, 110)
    centerBgSprite.setPivot(110, 110);

    // Pre-rotate and draw Hour and Minute hands directly onto centerBgSprite in RAM
    float hourAngle = ((h % 12) * 30.0f) + (m * 0.5f);
    float minAngle = m * 6.0f;
    hourHandSprite.pushRotated(&centerBgSprite, hourAngle, COLOR_TRANSPARENT);
    minuteHandSprite.pushRotated(&centerBgSprite, minAngle, COLOR_TRANSPARENT);

    // Update cached states
    last_min = m;
    last_temp = appState.temp;
    last_mday = ts.tm_mday;
    last_pct = pct;
  }

  // 4. Every second: Push center background RAM patch (<1ms, contains dial, text, and Hour+Minute hands!)
  centerBgSprite.pushSprite(10, 10);

  // 5. Draw ONLY the second hand on top of TFT
  tft.setPivot(120, 120);

  float secAngle = s * 6.0f;
  secondHandSprite.pushRotated(secAngle, COLOR_TRANSPARENT);

  // Center hub pin
  tft.drawSmoothCircle(120, 120, 5, COLOR_AVIATOR_ORANGE, COLOR_AVIATOR_ORANGE);
  tft.drawSmoothCircle(120, 120, 2, TFT_BLACK, TFT_BLACK);
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
// Eye Bitmaps (LittleFS RLE, 100x100 px each, 27 KB total):
//   /buddy_eye_o.rle  — Open ring eye with cyan bloom (STATE_REGULAR)
//   /buddy_eye_h.rle  — Half-lidded / relaxed halo (STATE_AWAY)
//   /buddy_eye_c.rle  — Closed glowing seam pair (STATE_FOCUS)
//   /buddy_eye_s.rle  — Happy / squint crescent (STATE_BUSY / DISTRACTED)
//
// Refined Rules & Physics:
//   - Paired Eye States: All 4 states present in PAIRS (including Closed pair!).
//   - Pure Black Background: Visor sprite fills TFT_BLACK (no background box).
//   - Primary Mood / Presence State Mapping:
//       STATE_REGULAR / default -> OPEN   (/buddy_eye_o.rle)
//       STATE_FOCUS              -> CLOSED (/buddy_eye_c.rle, excludes glitches & cameos)
//       STATE_BUSY / DISTRACTED  -> SQUINT (/buddy_eye_s.rle)
//       STATE_AWAY               -> HALFED (/buddy_eye_h.rle)
//   - Resting Elevation: Resting gaze sits above 0 (-6px) when far/idle.
//   - Focus Exclusion: Closed/Focus mode is excluded from glitches and cameos.
//   - Blinking: Top-down masking blink occurs ONLY when in OPEN mode!
//   - Eye Convergence: Eyes move 6px closer together as radar distance decreases
//                       (LX=69->75, RX=151->145 when <=45cm).
//   - Vertical Gaze Amplitude: 30px max (responsive to radar distance):
//       Distance <= 45cm  -> Look all the way to top of visor (-30px)
//       Distance >= 120cm -> Resting elevation (-6px)
// ============================================================================

// -- Eye bitmap dimensions ----------------------------------------------------
#define BUDDY_EYE_SPR_W   100   // 100x100 RLE sprite width  (px)
#define BUDDY_EYE_SPR_H   100   // 100x100 RLE sprite height (px)

// Base eye center positions in the 220x115 visor sprite coordinate space
// Lowered 29px total (CY = 86) for perfect vertical alignment
// Brought 5px closer together at normal distance (LX=71, RX=148)
#define BUDDY_EYE_CY      86    // Vertical center of eyes in visor sprite
#define BUDDY_EYE_LX_BASE 70    // Base Left eye center X (brought 1px right)
#define BUDDY_EYE_RX_BASE 151   // Base Right eye center X (brought 1px left)

// -- Visor canvas geometry ----------------------------------------------------
#define BUDDY_SPR_W       220
#define BUDDY_SPR_H       115
#define BUDDY_SPR_X        10
#define BUDDY_SPR_Y         5

// -- Motion Physics Limits ---------------------------------------------------
#define BUDDY_EYE_GAZE_X_LIMIT   3   // Halved horizontal gaze drift (±3px)
#define BUDDY_EYE_GAZE_Y_LIMIT  30   // 30px vertical gaze amplitude

// -- Bottom-half font aliases --------------------------------------------------
#define FONT_BUDDY_TIME   6
#define FONT_BUDDY_DATE   2
#define MSG_FONT_BUDDY    nullptr

// Eye State Modes
enum BuddyEyeMode {
  EYE_MODE_OPEN = 0,
  EYE_MODE_HALFED,
  EYE_MODE_CLOSED,
  EYE_MODE_SQUINT
};

/**
 * Ensure an eye sprite is allocated once in RAM with strict null checking.
 */
static bool ensureEyeSprite(TFT_eSprite &spr) {
  if (!spr.created()) {
    if (spr.createSprite(BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H) == nullptr) {
      Serial.printf("[BUDDY] ERROR: Out of heap memory for eye sprite %dx%d!\n", BUDDY_EYE_SPR_W, BUDDY_EYE_SPR_H);
      return false;
    }
  }
  return true;
}

/**
 * Decode 100x100 RLE eye file from LittleFS into a TFT_eSprite.
 * Reuses existing allocated RAM buffer to avoid heap fragmentation and allocation crashes.
 */
static bool loadEyeSprite100(TFT_eSprite &spr, const char *rleFile) {
  if (!ensureEyeSprite(spr)) return false;
  spr.fillSprite(TFT_BLACK);

  fs::File f = LittleFS.open(rleFile, "r");
  if (!f) {
    Serial.printf("[BUDDY] Missing RLE: %s\n", rleFile);
    return false;
  }

  uint16_t w = 0, h = 0;
  f.read((uint8_t*)&w, 2);
  f.read((uint8_t*)&h, 2);

  int curX = 0, curY = 0;
  while (f.available()) {
    uint8_t hdr = f.read();
    if (hdr & 0x80) {
      uint8_t lo = f.read();
      uint8_t hi = f.read();
      uint16_t color = lo | ((uint16_t)hi << 8);
      int count = (hdr & 0x7F) + 1;
      for (int k = 0; k < count; k++) {
        spr.drawPixel(curX, curY, color);
        if (++curX >= (int)w) { curX = 0; curY++; }
      }
    } else {
      int count = (hdr & 0x7F) + 1;
      for (int k = 0; k < count; k++) {
        uint8_t lo = f.read();
        uint8_t hi = f.read();
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
 */
static void stampEye100(TFT_eSprite &visor, TFT_eSprite &eyeSpr, int stampX, int stampY) {
  if (eyeSpr.created()) {
    eyeSpr.pushToSprite(&visor, stampX, stampY, TFT_BLACK);
  }
}

/**
 * Apply procedural top-down dark eyelid mask over an eye on the visor sprite.
 */
static void applyTopEyelidMask(TFT_eSprite &visor, int stampX, int stampY, int topLidH, uint16_t bgColor) {
  if (topLidH > 0) {
    if (topLidH > BUDDY_EYE_SPR_H) topLidH = BUDDY_EYE_SPR_H;
    visor.fillRect(stampX - 5, stampY - 5, BUDDY_EYE_SPR_W + 10, topLidH + 5, bgColor);
  }
}

/**
 * Render glowing cyan eyebrows — ONE segment per brow, no joints, no gaps.
 *
 * Each brow is a single drawWedgeLine from its outer to its inner end.
 * Three per-brow sculpted parameters drive all expressions:
 *
 *   browLiftL/R  [0..1]  height: brow moves upward (surprise, strangement)
 *   browTiltL/R  [-1..1] pitch:  +1 = inner end dips (interest/furrow)
 *                                -1 = inner end rises (one-side arch / strangement)
 *   browInset    [0..1]  shared: both brows slide inward toward nose bridge (interest)
 *
 * Left  brow: outer end = leftCenterX  - halfSpan,  inner end = leftCenterX  + halfSpan
 * Right brow: inner end = rightCenterX - halfSpan,  outer end = rightCenterX + halfSpan
 * A positive tilt on BOTH brows creates the symmetric "V" / interest frown.
 */
static void drawDeskbuddyEyebrows(
    TFT_eSprite &visor,
    int leftCenterX, int rightCenterX, int centerY,
    float leftBlinkPct, float rightBlinkPct,
    uint16_t color,
    float browLiftL, float browLiftR,   // [0..1]  per-brow vertical lift
    float browTiltL, float browTiltR,   // [-1..1] per-brow pitch
    float browInset)                     // [0..1]  shared inward slide
{
  const float HALF_SPAN  = 18.0f;   // half-length of each brow segment
  const float LIFT_MAX   = 15.0f;   // px of upward travel at lift=1
  const float TILT_MAX   =  8.0f;   // px of end-point displacement at |tilt|=1
  const float INSET_MAX  = 10.0f;   // px of inward slide at inset=1

  int dipL = (int)(leftBlinkPct  * 3.0f);
  int dipR = (int)(rightBlinkPct * 3.0f);

  float insetPx  = browInset * INSET_MAX;
  float neutralY = (float)(centerY - 42); // raised 5px higher than before

  // ── Left brow ─────────────────────────────────────────────────────────────
  //   lx_outer = leftward (away from nose), lx_inner = rightward (toward nose)
  float lBaseY   = neutralY + dipL - browLiftL * LIFT_MAX;
  float lTiltPx  = browTiltL * TILT_MAX;
  float lx_outer = (float)leftCenterX - HALF_SPAN + insetPx;  // slides right on inset
  float ly_outer = lBaseY - lTiltPx;                           // rises when tilt+
  float lx_inner = (float)leftCenterX + HALF_SPAN + insetPx;  // slides right on inset
  float ly_inner = lBaseY + lTiltPx;                           // dips  when tilt+

  // ── Right brow ────────────────────────────────────────────────────────────
  //   rx_inner = leftward (toward nose), rx_outer = rightward (away from nose)
  float rBaseY   = neutralY + dipR - browLiftR * LIFT_MAX;
  float rTiltPx  = browTiltR * TILT_MAX;
  float rx_inner = (float)rightCenterX - HALF_SPAN - insetPx; // slides left on inset
  float ry_inner = rBaseY + rTiltPx;                           // dips  when tilt+
  float rx_outer = (float)rightCenterX + HALF_SPAN - insetPx; // slides left on inset
  float ry_outer = rBaseY - rTiltPx;                           // rises when tilt+

  uint16_t glowColor = tft.color565(0, 100, 140);

  // Glow underlay (thinner, 1px lower)
  visor.drawWedgeLine(lx_outer, ly_outer + 1.0f, lx_inner, ly_inner + 1.0f, 1.2f, 1.2f, glowColor, TFT_BLACK);
  visor.drawWedgeLine(rx_inner, ry_inner + 1.0f, rx_outer, ry_outer + 1.0f, 1.2f, 1.2f, glowColor, TFT_BLACK);

  // Core bright stroke (~3.6px wide)
  visor.drawWedgeLine(lx_outer, ly_outer, lx_inner, ly_inner, 1.8f, 1.8f, color, TFT_BLACK);
  visor.drawWedgeLine(rx_inner, ry_inner, rx_outer, ry_outer, 1.8f, 1.8f, color, TFT_BLACK);
}



static TFT_eSprite leftEyeSpr(&tft);
static TFT_eSprite rightEyeSpr(&tft);

/**
 * Deallocate DeskBuddy eye sprites and visor canvas to free ~90KB RAM when switching clock faces.
 */
void cleanupDeskbuddySprites() {
  if (leftEyeSpr.created()) leftEyeSpr.deleteSprite();
  if (rightEyeSpr.created()) rightEyeSpr.deleteSprite();
  if (centerBgSprite.created()) centerBgSprite.deleteSprite();
  Serial.println("[SPRITES] Deskbuddy eye sprites (40KB) and visor canvas (50KB) deallocated from RAM.");
}

/**
 * Allocate the visor canvas sprite (220x115).
 */
void initDeskbuddySprite() {
  if (!centerBgSprite.created()) {
    Serial.println("[BUDDY] Allocating visor canvas sprite 220x115...");
    centerBgSprite.createSprite(BUDDY_SPR_W, BUDDY_SPR_H);
    centerBgSprite.fillSprite(TFT_BLACK);
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
  }

  // -- Lazy visor canvas init ------------------------------------------------
  initDeskbuddySprite();

  // -- RAM Eye Sprites tracking ----------------------------------------------
  static int loadedLeftMode  = -1;
  static int loadedRightMode = -1;


  // -- Animation State & Timers ----------------------------------------------
  static bool          firstRun         = true;
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
  static float         lookY            = -6.0f; // Initial resting position above 0
  static float         lookTargetX      = 0.0f;
  static float         convergenceX     = 0.0f;  // Eye closeness shift (0px..6px)

  // Autonomous Eyebrow Mood — physiological, timer-driven, NOT telemetry.
  // One segment per brow, sculpted by three independent parameters:
  //   browLiftL/R  [0..1]   vertical lift per brow
  //   browTiltL/R  [-1..1]  pitch: +1=inner dips (interest), -1=inner rises (strangement arch)
  //   browInset    [0..1]   shared inward slide toward nose bridge (interest)
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

  if (firstRun) {
    nextBlink       = now + 3000UL + (unsigned long)random(4000);
    nextLook        = now + 4000UL + (unsigned long)random(5000);
    nextGlitchCheck = now + 15000UL + (unsigned long)random(15000);
    nextCameoCheck  = now + 18000UL + (unsigned long)random(10000); // First cameo in 18-28s
    nextBrowCheck   = now + 10000UL + (unsigned long)random(8000);  // First brow mood in 10-18s
    firstRun        = false;
  }

  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
    loadedLeftMode  = -1;
    loadedRightMode = -1;
  }

  // ── Determine Primary Eye Mode from Presence State ───────────────────────
  //   STATE_REGULAR / default -> OPEN   (/buddy_eye_o.rle)
  //   STATE_FOCUS              -> CLOSED (/buddy_eye_c.rle, EXCLUDES glitches & cameos)
  //   STATE_BUSY / DISTRACTED  -> SQUINT (/buddy_eye_s.rle)
  //   STATE_AWAY               -> HALFED (/buddy_eye_h.rle)
  uint8_t primaryEyeMode = EYE_MODE_OPEN;
  switch (appState.currentPresenceState) {
    case STATE_FOCUS:
      primaryEyeMode = EYE_MODE_CLOSED;
      break;
    case STATE_BUSY:
    case STATE_DISTRACTED:
      primaryEyeMode = EYE_MODE_SQUINT;
      break;
    case STATE_AWAY:
      primaryEyeMode = EYE_MODE_HALFED;
      break;
    case STATE_REGULAR:
    default:
      primaryEyeMode = EYE_MODE_OPEN;
      break;
  }

  // ── Exclude Focus / Closed Mode from Glitches & Cameos ───────────────────
  bool allowCameosAndGlitches = (primaryEyeMode != EYE_MODE_CLOSED);

  // ── 2-Second Spontaneous Mode Cameo Switcher ──────────────────────────────
  if (allowCameosAndGlitches && !isCameoActive && !isGlitching && now >= nextCameoCheck) {
    isCameoActive  = true;
    cameoStart     = now;
    // Pick an alternate mode cameo for 2 seconds (excluding closed)
    uint8_t modes[] = { EYE_MODE_SQUINT, EYE_MODE_HALFED, EYE_MODE_OPEN };
    cameoMode      = modes[random(3)];
    if (cameoMode == primaryEyeMode) {
      cameoMode = (primaryEyeMode == EYE_MODE_OPEN) ? EYE_MODE_SQUINT : EYE_MODE_OPEN;
    }
    nextCameoCheck = now + 20000UL + (unsigned long)random(10000); // Cameo every 20-30s
  }

  if (isCameoActive && (!allowCameosAndGlitches || now - cameoStart >= 2000UL)) {
    isCameoActive = false;
  }

  // Active mode is cameoMode during cameo, otherwise primaryEyeMode
  uint8_t activeEyeMode = (isCameoActive && allowCameosAndGlitches) ? cameoMode : primaryEyeMode;

  // ── Synchronized Blink (ONLY HAPPENS IN OPEN MODE!) ──────────────────────
  bool canBlink = (activeEyeMode == EYE_MODE_OPEN);

  if (canBlink && blinkStart == 0 && now >= nextBlink) {
    blinkStart       = now;
    nextBlink        = now + 3500UL + (unsigned long)random(5000);
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

  // ── 100ms Digital Glitch Effect (~2/min, excluded during Focus/Closed) ────
  if (allowCameosAndGlitches && !isGlitching && now >= nextGlitchCheck) {
    isGlitching     = true;
    glitchStart     = now;
    glitchJitterX   = random(-3, 4);
    glitchJitterY   = random(-2, 3);
    nextGlitchCheck = now + 22000UL + (unsigned long)random(16000); // ~2 per minute
  }

  if (isGlitching && (!allowCameosAndGlitches || now - glitchStart >= 100UL)) {
    isGlitching   = false;
    glitchJitterX = 0;
    glitchJitterY = 0;
  }

  // Determine RLE file path for Left & Right eyes (paired by default)
  const char* leftRLE  = "/buddy_eye_o.rle";
  const char* rightRLE = "/buddy_eye_o.rle";
  int leftModeID  = activeEyeMode;
  int rightModeID = activeEyeMode;

  if (isGlitching && allowCameosAndGlitches) {
    // During 100ms glitch, swap eye images for digital glitch effect
    leftRLE     = (glitchJitterX > 0) ? "/buddy_eye_s.rle" : "/buddy_eye_h.rle";
    rightRLE    = (glitchJitterX > 0) ? "/buddy_eye_c.rle" : "/buddy_eye_s.rle";
    leftModeID  = 99;
    rightModeID = 98;
  } else {
    switch (activeEyeMode) {
      case EYE_MODE_CLOSED:
        leftRLE = "/buddy_eye_c.rle";  rightRLE = "/buddy_eye_c.rle"; // Paired closed!
        break;
      case EYE_MODE_SQUINT:
        leftRLE = "/buddy_eye_s.rle";  rightRLE = "/buddy_eye_s.rle"; // Paired squint!
        break;
      case EYE_MODE_HALFED:
        leftRLE = "/buddy_eye_h.rle";  rightRLE = "/buddy_eye_h.rle"; // Paired halfed!
        break;
      case EYE_MODE_OPEN:
      default:
        leftRLE = "/buddy_eye_o.rle";  rightRLE = "/buddy_eye_o.rle"; // Paired open!
        break;
    }
  }

  // Lazy-load eye bitmaps into RAM on mode change (with persistent RAM buffers)
  if (loadedLeftMode != leftModeID) {
    if (loadEyeSprite100(leftEyeSpr, leftRLE)) {
      loadedLeftMode = leftModeID;
    }
  }
  if (loadedRightMode != rightModeID) {
    if (strcmp(leftRLE, rightRLE) == 0 && leftEyeSpr.created()) {
      // Both eyes use identical texture: no need to decode RLE twice!
      loadedRightMode = rightModeID;
    } else {
      if (loadEyeSprite100(rightEyeSpr, rightRLE)) {
        loadedRightMode = rightModeID;
      }
    }
  }

  // ── Gaze / Radar Distance Physics ─────────────────────────────────────────
  // 1. Horizontal Gaze: Halved (±3px drift)
  if (now >= nextLook) {
    lookTargetX = (float)random(-BUDDY_EYE_GAZE_X_LIMIT, BUDDY_EYE_GAZE_X_LIMIT + 1);
    nextLook    = now + 4000UL + (unsigned long)random(6000);
  }

  // 2. Vertical Gaze: 30px amplitude with resting offset above 0 (-6px):
  //    Distance <= 45cm  -> Look all the way to top of visor (-30px)
  //    Distance >= 120cm -> Resting position above 0 (-6px)
  // 3. Eye Convergence: Eyes move 6px closer together as distance decreases!
  int rawDist = appState.rawDetectionDist;
  float lookTargetY = -6.0f; // Default resting position above 0
  float targetConvergence = 0.0f;

  if (rawDist > 0) {
    if (rawDist <= 45) {
      lookTargetY       = -(float)BUDDY_EYE_GAZE_Y_LIMIT; // -30px (top of visor)
      targetConvergence = 6.0f;                           // 6px closer together
    } else if (rawDist < 120) {
      float t = (float)(rawDist - 45) / 75.0f;             // 0.0 at 45cm -> 1.0 at 120cm
      lookTargetY       = -(float)BUDDY_EYE_GAZE_Y_LIMIT * (1.0f - t) + (-6.0f * t);
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

  // ── Mood Ring Color ───────────────────────────────────────────────────────
  uint8_t mr = appState.currentRingColor.r;
  uint8_t mg = appState.currentRingColor.g;
  uint8_t mb = appState.currentRingColor.b;

  // ── Render Visor Frame at ~30 fps (Pure TFT_BLACK canvas, no box) ─────────
  static unsigned long lastEyeFrame = 0;
  bool eyeUpdate = forceRedraw || isGlitching || isCameoActive || (now - lastEyeFrame >= 33UL);

  if (eyeUpdate) {
    lastEyeFrame = now;
    uint16_t visorBg = TFT_BLACK; // Pure TFT_BLACK background (no box behind eyes)

    // 1. Fill entire visor canvas with pure TFT_BLACK
    if (centerBgSprite.created()) {
      centerBgSprite.fillSprite(visorBg);

      int gx = (int)lookX + glitchJitterX;
      int gy = (int)lookY + glitchJitterY;
      int cx = (int)convergenceX;

      // Top-left stamp positions for 100x100 tiles (center - 50 + gaze)
      int leftX  = (BUDDY_EYE_LX_BASE + cx) - 50 + gx;
      int leftY  = BUDDY_EYE_CY - 50 + gy;
      int rightX = (BUDDY_EYE_RX_BASE - cx) - 50 + gx;
      int rightY = BUDDY_EYE_CY - 50 + gy;

      // 2. Stamp Left Eye & apply top-down mask blink (ONLY IN OPEN MODE!)
      stampEye100(centerBgSprite, leftEyeSpr, leftX, leftY);
      if (activeEyeMode == EYE_MODE_OPEN) {
        int leftTopLidH = (int)(leftBlinkPct * 65.0f);
        applyTopEyelidMask(centerBgSprite, leftX, leftY, leftTopLidH, visorBg);
      }

      // 3. Stamp Right Eye (use rightEyeSpr if created during glitch, otherwise leftEyeSpr)
      TFT_eSprite &rSpr = (rightEyeSpr.created() && strcmp(leftRLE, rightRLE) != 0) ? rightEyeSpr : leftEyeSpr;
      stampEye100(centerBgSprite, rSpr, rightX, rightY);
      if (activeEyeMode == EYE_MODE_OPEN) {
        int rightTopLidH = (int)(rightBlinkPct * 65.0f);
        applyTopEyelidMask(centerBgSprite, rightX, rightY, rightTopLidH, visorBg);
      }

      // 4. Draw expressive eyebrows over eyes (ONLY IN OPEN MODE!)
      if (activeEyeMode == EYE_MODE_OPEN) {
        // ── Autonomous Eyebrow Mood State Machine ───────────────────────────
        // Expressions mapped to single-segment brow geometry (lift + tilt + inset):
        //   40% → Interest    : tilt+1 both, inset 0.7   brows converge & inner ends dip
        //   15% → Surprise    : lift+1 both, tilt 0       brows rise flat
        //   15% → Strangement : lift+1 one side, tilt-0.6 that side  skeptical arch
        //   30% → Skip        : quiet reschedule
        bool browAtRest = (fabsf(browLiftL) < 0.01f && fabsf(browLiftR) < 0.01f &&
                           fabsf(browTiltL) < 0.01f && fabsf(browTiltR) < 0.01f &&
                           fabsf(browInset) < 0.01f);
        if (!browHolding && browAtRest && now >= nextBrowCheck) {
          int roll = (int)random(100);
          if (roll < 40) {
            // Interest: inner ends angle down + brows converge inward
            browLiftLTarget = 0.0f;  browLiftRTarget = 0.0f;
            browTiltLTarget = 1.0f;  browTiltRTarget = 1.0f;
            browInsetTarget = 0.7f;
            browHoldEnd     = now + 2000UL + (unsigned long)random(58000); // 2s–1min
            browHolding     = true;
            nextBrowCheck   = now + 12000UL + (unsigned long)random(10000);
          } else if (roll < 55) {
            // Surprise: both brows lift straight up
            browLiftLTarget = 1.0f;  browLiftRTarget = 1.0f;
            browTiltLTarget = 0.0f;  browTiltRTarget = 0.0f;
            browInsetTarget = 0.0f;
            browHoldEnd     = now + 1000UL + (unsigned long)random(29000); // 1s–30s
            browHolding     = true;
            nextBrowCheck   = now + 25000UL + (unsigned long)random(20000);
          } else if (roll < 70) {
            // Strangement: one brow lifts with slight skeptical arch inward tilt
            bool leftSide   = (random(2) == 0);
            browLiftLTarget = leftSide ? 1.0f : 0.0f;
            browLiftRTarget = leftSide ? 0.0f : 1.0f;
            browTiltLTarget = leftSide ? -0.6f : 0.0f; // raised brow tilts: inner end up
            browTiltRTarget = leftSide ? 0.0f : -0.6f;
            browInsetTarget = 0.0f;
            browHoldEnd     = now + 2000UL + (unsigned long)random(58000); // 2s–1min
            browHolding     = true;
            nextBrowCheck   = now + 18000UL + (unsigned long)random(15000);
          } else {
            nextBrowCheck   = now + 8000UL + (unsigned long)random(8000);
          }
        }

        // Release hold → ramp everything back to neutral
        if (browHolding && now >= browHoldEnd) {
          browLiftLTarget = 0.0f;  browLiftRTarget = 0.0f;
          browTiltLTarget = 0.0f;  browTiltRTarget = 0.0f;
          browInsetTarget = 0.0f;
          browHolding     = false;
        }

        // Fast lerp toward all targets (factor 0.28 ≈ ~150ms ramp at 30fps)
        browLiftL += (browLiftLTarget - browLiftL) * 0.28f;
        browLiftR += (browLiftRTarget - browLiftR) * 0.28f;
        browTiltL += (browTiltLTarget - browTiltL) * 0.28f;
        browTiltR += (browTiltRTarget - browTiltR) * 0.28f;
        browInset += (browInsetTarget - browInset) * 0.28f;

        // Snap to zero to kill micro-drift
        if (browLiftLTarget == 0.0f && fabsf(browLiftL) < 0.005f) browLiftL = 0.0f;
        if (browLiftRTarget == 0.0f && fabsf(browLiftR) < 0.005f) browLiftR = 0.0f;
        if (browTiltLTarget == 0.0f && fabsf(browTiltL) < 0.005f) browTiltL = 0.0f;
        if (browTiltRTarget == 0.0f && fabsf(browTiltR) < 0.005f) browTiltR = 0.0f;
        if (browInsetTarget == 0.0f && fabsf(browInset) < 0.005f) browInset = 0.0f;

        uint16_t eyebrowColor = tft.color565(0, 220, 255);
        drawDeskbuddyEyebrows(
            centerBgSprite,
            BUDDY_EYE_LX_BASE + cx + gx, BUDDY_EYE_RX_BASE - cx + gx, BUDDY_EYE_CY + gy,
            leftBlinkPct, rightBlinkPct, eyebrowColor,
            browLiftL, browLiftR, browTiltL, browTiltR, browInset);
      }

      // 5. Push completed visor patch to TFT (<1ms)
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

  bool bottomUpdate = forceRedraw || (now - lastBottomUpdate >= 500UL);

  if (bottomUpdate) {
    lastBottomUpdate = now;

    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    int display_h = h;
    if (!appConfig.time24h) {
      display_h = h % 12;
      if (display_h == 0) display_h = 12;
    }

    tft.setTextDatum(MC_DATUM);

    // 3D glowing divider bar
    uint16_t dividerGlow = tft.color565(mr * 70 / 100, mg * 70 / 100, mb * 70 / 100);
    tft.drawFastHLine(35, 120, 170, tft.color565(12, 12, 22));
    tft.drawFastHLine(42, 121, 156, dividerGlow);
    tft.drawFastHLine(46, 122, 148, tft.color565(80, 85, 115));
    tft.drawFastHLine(50, 123, 140, tft.color565(6, 6, 12));

    // Time (HH:MM)
    if (forceRedraw || h != last_h || m != last_m) {
      tft.fillRect(28, 128, 184, 46, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      char timeStr[6];
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d", display_h, m);
      tft.drawString(String(timeStr), 120, 152, FONT_BUDDY_TIME);
      last_h = h;
      last_m = m;
    }

    // Date
    String currentDate = String(buf);
    if (forceRedraw || currentDate != last_date) {
      tft.fillRect(28, 177, 184, 18, TFT_BLACK);
      tft.setTextColor(tft.color565(110, 110, 135), TFT_BLACK);
      tft.drawString(currentDate, 120, 186, FONT_BUDDY_DATE);
      last_date = currentDate;
    }

    // Rotating metric
    if (now - lastMetricSwitch > 12000UL) {
      metricIdx = (metricIdx + 1) % 3;
      lastMetricSwitch = now;
    }

    String metricText = "";
    switch (metricIdx) {
      case 0: {
        int pct = 0;
        if (appConfig.targetHours > 0.0f) {
          pct = (int)((appStats.totalDeskTime * 100.0f) /
                      (appConfig.targetHours * 3600.0f * 1000.0f));
          if (pct > 100) pct = 100;
        }
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

    if (forceRedraw || metricText != last_metric || metricColor != lastMetricColor) {
      tft.fillRect(28, 200, 184, 20, TFT_BLACK);
      tft.setTextColor(metricColor, TFT_BLACK);
      tft.drawString(metricText, 120, 210, FONT_BUDDY_DATE);
      last_metric     = metricText;
      lastMetricColor = metricColor;
    }
  }
}

#endif // FACEPLATES_H

