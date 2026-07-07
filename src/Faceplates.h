#ifndef FACEPLATES_H
#define FACEPLATES_H

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include "FuturaFont.h"
#include "GoodTimingFont.h"
#include "Display.h"

// Extern references for global state variables from main.cpp
extern const RGBColor stateColors[];
extern int currentPresenceState;
extern bool hasMail;
extern RGBColor currentRingColor;
extern RGBColor startRingColor;
extern RGBColor targetRingColor;
extern unsigned long ringTransitionStart;
extern const unsigned long ringTransitionDuration;
extern int temp;
extern String weatherDesc;
extern int productivityScore;
extern float targetHours;
extern unsigned long totalDeskTime;
extern unsigned long totalFocusTime;
extern unsigned long totalBreakTime;
extern int breakCount;
extern unsigned long continuousPresenceStart;
extern char buf[];
extern NTPClient timeClient;
extern String formatTime(unsigned long ms);

// ============================================================================
// SECTION 1: COMMON CLIPPING & DRAWING HELPERS
// ============================================================================

// Helper function to draw a single pixel with circular clipping (only within radius < 115)
void drawClippedPixel(int32_t x, int32_t y, uint16_t color) {
  if (x >= 0 && x < 240 && y >= 0 && y < 240) {
    int32_t dx = x - 120;
    int32_t dy = y - 120;
    if (dx * dx + dy * dy < 115 * 115) {
      tft.drawPixel(x, y, color);
    }
  }
}

// Helper to draw a horizontal line with circular clipping
void drawClippedHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
  for (int32_t i = 0; i < w; i++) {
    drawClippedPixel(x + i, y, color);
  }
}

// Helper to draw a filled rectangle with circular clipping
void drawClippedFillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
  for (int32_t i = 0; i < h; i++) {
    drawClippedHLine(x, y + i, w, color);
  }
}

// Compute the fixed point square root of an integer and return the 8 MS bits of fractional part
static inline uint8_t local_sqrt_fraction(uint32_t num) {
  if (num > (0x40000000)) return 0;
  uint32_t bsh = 0x00004000;
  uint32_t fpr = 0;
  uint32_t osh = 0;
  while (num > bsh) { bsh <<= 2; osh++; }
  do {
    uint32_t bod = bsh + fpr;
    if (num >= bod) {
      num -= bod;
      fpr = bsh + bod;
    }
    num <<= 1;
  } while (bsh >>= 1);
  return fpr >> osh;
}

// Draw a smooth rounded corner rectangle with circular clipping (radius < 115)
void drawClippedSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir, int32_t w, int32_t h, uint32_t fg_color, uint32_t bg_color, uint8_t quadrants = 0xF) {
  if (r < ir) {
    int32_t temp = r;
    r = ir;
    ir = temp;
  }
  if (r <= 0 || ir < 0) return;

  w -= 2 * r;
  h -= 2 * r;

  if (w < 0) w = 0;
  if (h < 0) h = 0;

  x += r;
  y += r;

  uint16_t t = r - ir + 1;
  int32_t xs = 0;
  int32_t cx = 0;

  int32_t r2 = r * r;   // Outer arc radius^2
  r++;
  int32_t r1 = r * r;   // Outer AA zone radius^2

  int32_t r3 = ir * ir; // Inner arc radius^2
  ir--;
  int32_t r4 = ir * ir; // Inner AA zone radius^2

  uint8_t alpha = 0;

  // Scan top left quadrant
  for (int32_t cy = r - 1; cy > 0; cy--) {
    int32_t len = 0;  // Pixel run length
    int32_t lxst = 0; // Left side run x start
    int32_t rxst = 0; // Right side run x start
    int32_t dy2 = (r - cy) * (r - cy);

    // Find and track arc zone start point
    while ((r - xs) * (r - xs) + dy2 >= r1) xs++;

    for (cx = xs; cx < r; cx++) {
      int32_t hyp = (r - cx) * (r - cx) + dy2;

      if (hyp > r2) {
        alpha = ~local_sqrt_fraction(hyp); // Outer AA zone
      } else if (hyp >= r3) {
        rxst = cx;
        len++;
        continue;
      } else {
        if (hyp <= r4) break;
        alpha = local_sqrt_fraction(hyp); // Inner AA zone
      }

      if (alpha < 16) continue;

      uint16_t pcol = fastBlend(alpha, fg_color, bg_color);
      if (quadrants & 0x8) drawClippedPixel(x + cx - r, y - cy + r + h, pcol);     // BL
      if (quadrants & 0x1) drawClippedPixel(x + cx - r, y + cy - r, pcol);         // TL
      if (quadrants & 0x2) drawClippedPixel(x - cx + r + w, y + cy - r, pcol);     // TR
      if (quadrants & 0x4) drawClippedPixel(x - cx + r + w, y - cy + r + h, pcol); // BR
    }
    
    lxst = rxst - len + 1;
    if (quadrants & 0x8) drawClippedHLine(x + lxst - r, y - cy + r + h, len, fg_color);
    if (quadrants & 0x1) drawClippedHLine(x + lxst - r, y + cy - r, len, fg_color);
    if (quadrants & 0x2) drawClippedHLine(x - rxst + r + w, y + cy - r, len, fg_color);
    if (quadrants & 0x4) drawClippedHLine(x - rxst + r + w, y - cy + r + h, len, fg_color);
  }

  // Draw sides
  if ((quadrants & 0xC) == 0xC) drawClippedFillRect(x, y + r - t + h, w + 1, t, fg_color); // Bottom
  if ((quadrants & 0x9) == 0x9) drawClippedFillRect(x - r + 1, y, t, h + 1, fg_color);     // Left
  if ((quadrants & 0x3) == 0x3) drawClippedFillRect(x, y - r + 1, w + 1, t, fg_color);     // Top
  if ((quadrants & 0x6) == 0x6) drawClippedFillRect(x + r - t + w, y, t, h + 1, fg_color); // Right
}


// ============================================================================
// SECTION 2: DEFAULT FACEPLATE
// ============================================================================

// Helper function to draw default digital clock face
void drawDefaultClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  // 1. Mood Ring Animation & Drawing
  RGBColor targetColor = stateColors[currentPresenceState];
  if (targetColor != targetRingColor) {
    startRingColor = currentRingColor;
    targetRingColor = targetColor;
    ringTransitionStart = now;
  }

  static unsigned long lastRingUpdate = 0;
  bool isTransitioning = (currentRingColor != targetRingColor);
  bool ringRedrawn = false;

  if (forceRedraw || (isTransitioning && (now - lastRingUpdate > 50))) {
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
    uint16_t color565 = tft.color565(currentRingColor.r, currentRingColor.g, currentRingColor.b);
    tft.drawSmoothRoundRect(2, 2, 118, 116, 0, 0, color565, TFT_BLACK);
    lastRingUpdate = now;
    ringRedrawn = true;
  }

  // 2. Alert/Event Message Mode
  if (showEvent) {
    if (forceRedraw || ringRedrawn) {
      drawFaceplateMessage("/msg_default.rle", message, TFT_SKYBLUE, isAi, TFT_LIGHTGREY);
      // Redraw bezel ring on top of the alert background
      uint16_t color565 = tft.color565(currentRingColor.r, currentRingColor.g, currentRingColor.b);
      tft.drawSmoothRoundRect(2, 2, 118, 116, 0, 0, color565, TFT_BLACK);
    }
    return;
  }

  // 3. Normal Clock Drawing (Throttled to 500ms unless redraw or ring updated)
  static unsigned long lastDefaultFaceUpdate = 0;
  if (!forceRedraw && !ringRedrawn && (now - lastDefaultFaceUpdate < 500)) {
    return;
  }
  lastDefaultFaceUpdate = now;

  static String lastMetricText = "";
  static uint16_t lastMetricColor = 0;

  static unsigned long lastMetricSwitch = 0;
  static int metricIndex = 0;

  tft.setTextDatum(MC_DATUM);

  // Weather section (top)
  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  tft.drawString(String(temp) + "C | " + weatherDesc, 120, 50, 4);

  // Draw Mail Indicator on Default Clock Face
  static bool lastHasMailDefault = false;
  if (forceRedraw || (hasMail != lastHasMailDefault)) {
    if (hasMail) {
      tft.fillRect(200, 46, 17, 12, TFT_BLACK); // Clear first
      tft.drawRect(200, 46, 17, 12, TFT_YELLOW);
      tft.drawLine(200, 46, 208, 52, TFT_YELLOW);
      tft.drawLine(208, 52, 216, 46, TFT_YELLOW);
    } else {
      tft.fillRect(200, 46, 17, 12, TFT_BLACK);
    }
    lastHasMailDefault = hasMail;
  }

  // Time section (center)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String timeStr = timeClient.getFormattedTime().substring(0, 5);
  tft.drawString(timeStr, 120, 105, 7); // Large digital font

  // Date section (below time)
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(buf, 120, 150, 2);

  // Cycle through metrics at the bottom (Y=190) every 15 seconds
  if (now - lastMetricSwitch > 15000) {
    metricIndex = (metricIndex + 1) % 5;
    lastMetricSwitch = now;
  }

  String metricText = "";
  uint16_t metricColor = tft.color565(100, 100, 100);

  switch (metricIndex) {
    case 0: {
      int pct = 0;
      if (targetHours > 0.0f) {
        pct = (int)((totalDeskTime * 100.0f) / (targetHours * 3600.0f * 1000.0f));
      }
      if (pct > 100) pct = 100;
      metricText = "Day: " + String(pct) + "%";
      break;
    }
    case 1:
      metricText = "Score: " + String(productivityScore) + "%";
      break;
    case 2:
      metricText = "At Desk: " + formatTime(now - continuousPresenceStart);
      break;
    case 3:
      metricText = "Breaks: " + String(breakCount);
      break;
    case 4:
      metricText = "Focus: " + formatTime(totalFocusTime);
      break;
  }

  if (metricText != lastMetricText || metricColor != lastMetricColor || forceRedraw || ringRedrawn) {
    tft.fillRect(42, 176, 156, 28, TFT_BLACK); // Clear text area safely without clipping bezel ring
    tft.setTextColor(metricColor, TFT_BLACK);
    tft.drawString(metricText, 120, 190, 4);
    lastMetricText = metricText;
    lastMetricColor = metricColor;
  }
}


// ============================================================================
// SECTION 3: MINIMALIST FACEPLATE
// ============================================================================

// Helper to determine if coordinates are inside the offset minute box
bool inMinuteWindow(int x, int y) {
  return (x >= 170 && y >= 81 && y <= 141);
}

// Helper function to draw minimalist clock face
void drawMinimalistClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  tft.setTextDatum(MC_DATUM); // Ensure all text draws and erases with the same coordinate system

  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/msg_minimalist.rle", message, TFT_WHITE, isAi, TFT_LIGHTGREY);
    }
    return;
  }

  static unsigned long lastMinimalistFaceUpdate = 0;
  if (!forceRedraw && (now - lastMinimalistFaceUpdate < 500)) {
    return;
  }
  lastMinimalistFaceUpdate = now;

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
    tft.loadFont(Futura18); // Load smooth font for dial numbers
    
    if (last_m != -1) {
      // 1. Erase all old ticks and labels first (instant) using wide black lines
      for (int i = 0; i < 60; i++) {
        float rad_old = ((i - last_m) * 6 - 6) * 3.14159265f / 180.0f;
        float c_old = cosf(rad_old);
        float s_old = sinf(rad_old);
        
        int x_out_old = 120 + (int)(110 * c_old);
        int y_out_old = 120 + (int)(110 * s_old);
        int x_in_old = 120 + (int)(102 * c_old);
        int y_in_old = 120 + (int)(102 * s_old);
        
        if (!inMinuteWindow(x_in_old, y_in_old) && !inMinuteWindow(x_out_old, y_out_old)) {
          tft.drawWideLine((float)x_in_old, (float)y_in_old, (float)x_out_old, (float)y_out_old, 2.0f, TFT_BLACK, TFT_BLACK);
        }
        
        if (i % 5 == 0) {
          int x_text_old = 120 + (int)(92 * c_old);
          int y_text_old = 120 + (int)(92 * s_old);
          if (!inMinuteWindow(x_text_old, y_text_old)) {
            tft.setTextColor(TFT_BLACK, TFT_BLACK);
            tft.drawString(String(i), x_text_old, y_text_old);
          }
        }
      }
      
      // 2. Draw new ticks and labels in a counter-clockwise sweep with antialiasing
      for (int i = 59; i >= 0; i--) {
        float rad_new = ((i - m) * 6 - 6) * 3.14159265f / 180.0f;
        float c_new = cosf(rad_new);
        float s_new = sinf(rad_new);
        
        int x_out_new = 120 + (int)(110 * c_new);
        int y_out_new = 120 + (int)(110 * s_new);
        int x_in_new = 120 + (int)(102 * c_new);
        int y_in_new = 120 + (int)(102 * s_new);
        
        if (!inMinuteWindow(x_in_new, y_in_new) && !inMinuteWindow(x_out_new, y_out_new)) {
          uint16_t color = (i % 5 == 0) ? TFT_WHITE : tft.color565(100, 100, 100);
          float wd = (i % 5 == 0) ? 1.5f : 1.0f;
          tft.drawWideLine((float)x_in_new, (float)y_in_new, (float)x_out_new, (float)y_out_new, wd, color, TFT_BLACK);
        }
        
        if (i % 5 == 0) {
          int x_text_new = 120 + (int)(92 * c_new);
          int y_text_new = 120 + (int)(92 * s_new);
          if (!inMinuteWindow(x_text_new, y_text_new)) {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(String(i), x_text_new, y_text_new);
          }
        }
        
        delay(2); // Smooth counter-clockwise wipe transition
      }
    } else {
      // Instant draw on startup or page switch with antialiasing
      for (int i = 0; i < 60; i++) {
        float rad_new = ((i - m) * 6 - 6) * 3.14159265f / 180.0f;
        float c_new = cosf(rad_new);
        float s_new = sinf(rad_new);
        
        int x_out_new = 120 + (int)(110 * c_new);
        int y_out_new = 120 + (int)(110 * s_new);
        int x_in_new = 120 + (int)(102 * c_new);
        int y_in_new = 120 + (int)(102 * s_new);
        
        if (!inMinuteWindow(x_in_new, y_in_new) && !inMinuteWindow(x_out_new, y_out_new)) {
          uint16_t color = (i % 5 == 0) ? TFT_WHITE : tft.color565(100, 100, 100);
          float wd = (i % 5 == 0) ? 1.5f : 1.0f;
          tft.drawWideLine((float)x_in_new, (float)y_in_new, (float)x_out_new, (float)y_out_new, wd, color, TFT_BLACK);
        }
        
        if (i % 5 == 0) {
          int x_text_new = 120 + (int)(92 * c_new);
          int y_text_new = 120 + (int)(92 * s_new);
          if (!inMinuteWindow(x_text_new, y_text_new)) {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawString(String(i), x_text_new, y_text_new);
          }
        }
      }
    }
    tft.unloadFont();
  }

  // Draw capsule outline if forced redraw or if minutes changed (repairs ticks erase)
  if (forceRedraw || minuteChanged) {
    drawClippedSmoothRoundRect(175, 86, 25, 24, 105, 50, TFT_WHITE, TFT_BLACK);
  }

  // Draw Hour (only when changed)
  if (hourChanged) {
    if (last_h != -1) {
      tft.fillRect(66, 86, 108, 62, TFT_BLACK); // Clear Hour area safely
    }
    tft.loadFont(Futura64);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(h), 120, 121); // Moved 10px down (from 111 to 121)
    tft.unloadFont();
  }

  // Draw Date (only when changed)
  if (dateChanged) {
    if (last_date != "") {
      tft.fillRect(40, 149, 160, 18, TFT_BLACK); // Clear Date area safely (doesn't overlap Hour)
    }
    tft.loadFont(Futura18);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(buf, 120, 158); // Moved 10px down (from 148 to 158)
    tft.unloadFont();
    last_date = String(buf);
  }

  // Draw Minute (only when changed)
  if (minuteChanged) {
    if (last_m != -1) {
      tft.fillRect(185, 96, 45, 34, TFT_BLACK); // Clear only the large minute digits area inside the capsule
    }
    tft.loadFont(Futura36);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char minStr[3];
    snprintf(minStr, sizeof(minStr), "%02d", m);
    tft.drawString(String(minStr), 208, 113); // Centered at X=208, Y=113 (lowered 2px)
    tft.unloadFont();
  }

  // Draw Status Icons at Y=60 (Wifi, Internet)
  bool wifi_connected = wifiAvailable;
  bool internet_online = internetAvailable;
  static int last_mail_status = -1;

  if (forceRedraw || 
      (wifi_connected != (last_wifi_status == 1)) || 
      (internet_online != (last_internet_online == 1)) ||
      (hasMail != (last_mail_status == 1))) {
    
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
    if (hasMail) {
      tft.fillRect(152, 53, 15, 11, TFT_BLACK);
      tft.drawRect(152, 55, 15, 11, TFT_WHITE);
      tft.drawLine(152, 55, 159, 61, TFT_WHITE);
      tft.drawLine(159, 61, 166, 55, TFT_WHITE);
    } else {
      tft.fillRect(152, 53, 16, 13, TFT_BLACK);
    }

    last_wifi_status = wifi_connected ? 1 : 0;
    last_internet_online = internet_online ? 1 : 0;
    last_mail_status = hasMail ? 1 : 0;
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

// Draw a line with circular clipping (radius < 115)
void drawClippedLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
  int32_t dx = abs(x1 - x0);
  int32_t dy = abs(y1 - y0);
  int32_t sx = (x0 < x1) ? 1 : -1;
  int32_t sy = (y0 < y1) ? 1 : -1;
  int32_t err = dx - dy;

  while (true) {
    drawClippedPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int32_t e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Helper function to draw hitech cyberpunk style clock face using background bitmap
void drawHiTechClockFace(unsigned long now, bool forceRedraw, bool showEvent, const String &message, bool isAi, bool wifiAvailable, bool internetAvailable, bool hasMail) {
  if (showEvent) {
    if (forceRedraw) {
      drawFaceplateMessage("/msg_hitech.rle", message, HITECH_CYAN, isAi, HITECH_MUTED);
    }
    return;
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
  int mail_status = hasMail ? 1 : 0;

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
    if (hasMail) {
      tft.fillRect(112, 18, 16, 15, HITECH_BG_STATUS); // Clear area safely
      tft.drawRect(112, 20, 15, 11, HITECH_CYAN);
      tft.drawLine(112, 20, 119, 26, HITECH_CYAN);
      tft.drawLine(119, 26, 126, 20, HITECH_CYAN);
    } else {
      tft.fillRect(112, 18, 16, 15, HITECH_BG_STATUS);
    }
    last_mail = mail_status;
  }

  // Draw time in HH:MM format using Futura36 font
  // Window: lower-left (60, 142) -> standard (60, 97); top-right (180, 181) -> standard (180, 58)
  // Box: x = 60, y = 58, width = 120, height = 39. Center: X = 120, Y = 77
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  if (h != last_hour || m != last_min) {
    // Clear time window (aligned to time box Y=38 to Y=76, widened to 128px)
    tft.fillRect(45, 69, 146, 30, HITECH_BG_TIME);
    

    // Draw centered time
    tft.loadFont(GoodTiming46);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_TIME);
    tft.setTextDatum(MC_DATUM);
    
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);
    tft.drawString(String(timeStr), 117, 56);
    //tft.drawString("22:22", 120, 56);
    tft.unloadFont();

    last_hour = h;
    last_min = m;
  }

  // Draw temperature in format 00 and celsius upper o
  // Window: lower-left (147, 208) -> standard (147, 31); top-right (174, 220) -> standard (174, 19)
  // Box: x = 147, y = 19, width = 28, height = 13. Center: X = 160.5, Y = 25
  if (temp != last_temp) {
    // Clear box area (aligned to Y=19 center with 13px height)
    tft.fillRect(138, 21, 40, 13, HITECH_BG_STATUS);

    tft.loadFont(GoodTiming15);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_STATUS);
    tft.setTextDatum(MC_DATUM);

    char tempValStr[3];
    snprintf(tempValStr, sizeof(tempValStr), "%02d", temp);

    // Draw 2-digit temperature centered at X=153, Y=19
    tft.drawString(String(tempValStr), 150, 19);

    // Draw degree circle (radius 1) centered at X=163, Y=15
    tft.drawCircle(163, 24, 1, HITECH_CYAN);

    // Draw letter C centered at X=169, Y=19
    tft.drawString("C", 170, 19);

    tft.unloadFont();
    last_temp = temp;
  }

  // Draw 3-letter day of week abbreviation and date in DD MM format
  // Day Window: lower-left (69, 118) -> standard (69, 121); top-right (100, 133) -> standard (100, 106)
  // Day Box: x = 69, y = 106, width = 31, height = 15. Center: X = 84, Y = 113
  // Date Window: lower-left (116, 118) -> standard (116, 121); top-right (170, 133) -> standard (170, 106)
  // Date Box: x = 116, y = 106, width = 54, height = 15. Center: X = 143, Y = 113
  if (ts.tm_mday != last_mday || ts.tm_mon != last_mon) {
    // 1. Day of the Week Box (restored to original y=106, height=15)
    tft.fillRect(65, 109, 38, 13, HITECH_BG_TIME);
    tft.loadFont(GoodTiming15);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_TIME);
    tft.setTextDatum(MC_DATUM);
    const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    int wday = ts.tm_wday;
    if (wday < 0 || wday > 6) wday = 0;
    tft.drawString(daysOfWeek[wday], 84, 106);
    tft.unloadFont();

    // 2. Date Box (restored to original y=106, height=15)
    tft.fillRect(113, 109, 54, 13, HITECH_BG_TIME);
    tft.loadFont(GoodTiming15);
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
  int current_desk_hours = totalDeskTime / 3600000UL;
  int current_break_hours = totalBreakTime / 3600000UL;

  if (current_desk_hours != last_desk_hours || current_break_hours != last_break_hours || forceRedraw) {
    // Clear boxes with HITECH_BG_STATUS (aligned to Y=134 to Y=150 inside slot borders)
    tft.fillRect(61, 147, 28, 15, HITECH_BOX_BG);
    tft.fillRect(126, 147, 28, 16, HITECH_BOX_BG);

    tft.loadFont(GoodTiming20);
    tft.setTextColor(HITECH_CYAN, HITECH_BG_STATUS);
    tft.setTextDatum(MC_DATUM);
    char deskHoursStr[8];
    char breakHoursStr[8];
    snprintf(deskHoursStr, sizeof(deskHoursStr), "%dH", current_desk_hours);
    snprintf(breakHoursStr, sizeof(breakHoursStr), "%dH", current_break_hours);
    tft.drawString(String(deskHoursStr), 89, 142);
    tft.drawString(String(breakHoursStr), 154, 142);
    tft.unloadFont();

    last_desk_hours = current_desk_hours;
    last_break_hours = current_break_hours;
  }
}

#endif // FACEPLATES_H
