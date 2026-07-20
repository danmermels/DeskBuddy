#ifndef FACEPLATES_H
#define FACEPLATES_H

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <ld2410.h>
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
      drawFaceplateMessage("/default_msg.rle", message, TFT_SKYBLUE, MSG_FONT_DEFAULT, isAi, TFT_LIGHTGREY);
      
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
      drawFaceplateMessage("/minimalist_msg.rle", message, TFT_WHITE, MSG_FONT_MINIMALIST, isAi, TFT_LIGHTGREY);
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



#define MSG_FONT_HITECH "GoodTiming15"
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
      drawFaceplateMessage("/hitech_msg.rle", message, HITECH_CYAN, MSG_FONT_HITECH, isAi, HITECH_MUTED);
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
      drawFaceplateMessage(nullptr, message, TFT_GREEN, MSG_FONT_DEV, isAi, TFT_DARKGREY);
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
      drawFaceplateMessage("/aviator_msg.rle", message, COLOR_AVIATOR_ORANGE, MSG_FONT_AVIATOR, isAi, TFT_LIGHTGREY);
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

#endif // FACEPLATES_H
