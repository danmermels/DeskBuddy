#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>
#include "State.h"
#include "MessageManager.h"
#include "Logger.h"

// Timer display font: any VLW file in data/ (e.g. "7Segment50", "RamisArabic64",
// "GoodTiming20"). Swap the name here to change the TFT readout - the .vlw must
// exist on LittleFS, otherwise the built-in 7-segment font is used as a fallback.
#ifndef TIMER_FONT
#define TIMER_FONT "Unicode.impact20"
#endif
#define TIMER_RESET_HOLD_MS 3000UL
#define TIMER_PAUSE_HOLD_MS 10000UL

// Global state (instantiated in main.cpp) and externs
extern TimerState timerState;
extern TFT_eSPI tft;
extern MessageManager messageManager;

// Current metered time (ms) for the active timer mode
inline unsigned long timerCurrentMs(unsigned long now) {
  if (timerState.mode == 1) {
    return timerState.running ? (timerState.accumMs + (now - timerState.refMillis)) : timerState.accumMs;
  }
  if (timerState.mode == 2) {
    if (timerState.running) {
      unsigned long elapsed = now - timerState.refMillis;
      return (elapsed >= timerState.targetMs) ? 0 : (timerState.targetMs - elapsed);
    }
    return timerState.accumMs;
  }
  return 0;
}

// Called every display loop: fires the one-shot completion notification via MessageManager.
inline void timerTick(unsigned long now) {
  if (timerState.mode == 2 && timerState.running) {
    if (now - timerState.refMillis >= timerState.targetMs) {
      timerState.running = false;
      timerState.accumMs = 0;
      if (!timerState.completionFired) {
        timerState.completionFired = true;
        timerState.holdUntil = now + TIMER_PAUSE_HOLD_MS; // show 00:00 briefly, then release the screen
        messageManager.scheduleMessageWithPriority(
          EVENT_PAGE,
          "Timer done!",
          MessageManager::P_HIGH, 0, MessageManager::R_NORMAL
        );
      }
    }
  }
}

// Apply a command from the web/MQTT control channel.
// action: START | PAUSE | RESUME | RESET   mode: "sw" | "cd"
inline void timerCommand(const String& action, const String& mode, unsigned long totalMs, unsigned long now) {
  if (action == "RESET") {
    timerState.mode = 0;
    timerState.running = false;
    timerState.accumMs = 0;
    timerState.targetMs = 0;
    timerState.completionFired = false;
    timerState.holdUntil = now + TIMER_RESET_HOLD_MS;
    return;
  }
  if (action == "PAUSE") {
    if (timerState.mode != 0 && timerState.running) {
      timerState.accumMs = timerCurrentMs(now);
      timerState.running = false;
      timerState.holdUntil = now + TIMER_PAUSE_HOLD_MS; // keep the frozen readout 10 s, then return to the faceplate
    }
    return;
  }
  if (action == "RESUME") {
    if (timerState.mode != 0 && !timerState.running) {
      if (timerState.mode == 2 && timerState.completionFired) return; // a finished countdown must be started fresh
      timerState.refMillis = now;
      timerState.running = true;
    }
    return;
  }
  if (action == "START") {
    if (mode == "cd") {
      if (totalMs == 0) return;
      timerState.mode = 2;
      timerState.targetMs = totalMs;
      timerState.accumMs = totalMs;
      timerState.completionFired = false;
    } else {
      if (timerState.mode != 1) timerState.accumMs = 0;
      timerState.mode = 1;
      timerState.completionFired = false;
    }
    timerState.refMillis = now;
    timerState.running = true;
  }
}

// True while the metered number should own the screen: a running timer always
// does; a paused, finished, or reset timer only during its hold window, so the
// overlay can never trap the display (paused timers free the screen after 10 s,
// resets after 3 s) and resume/start brings it right back.
inline bool timerShouldDraw(unsigned long now) {
  if (timerState.running) return true;
  return now < timerState.holdUntil;
}

// Reused 48KB overlay sprite: created once while the timer owns the screen and
// freed when it releases it (was a create/delete on every redraw, up to 20 Hz
// on the hundredths strip = the worst alloc/free churn in the firmware).
static TFT_eSprite timerOverlaySpr(&tft);
static bool timerOverlayReady = false;
static bool timerOverlayFontLoaded = false;

// Free the timer overlay sprite once the timer no longer owns the screen.
inline void freeTimerOverlaySprite() {
  if (timerOverlayReady) {
    if (timerOverlayFontLoaded) timerOverlaySpr.unloadFont();
    timerOverlaySpr.deleteSprite();
    timerOverlayReady = false;
    timerOverlayFontLoaded = false;
  }
}

// Full-screen metered overlay: centered readout with no title. Stopwatch (mode 1)
// shows M:SS:hh (hundredths), countdown (mode 2) and reset-hold show M:SS. The
// number is rendered into an off-screen sprite and pushed in atomic window writes:
// the whole number once per second, plus just the hundredths strip at 20 Hz while
// the stopwatch runs (the previous per-second direct region-clear + font reload
// from LittleFS visibly blinked). The sprite covers screen y=44..144 so
// GoodTiming46's anchor (TFT_eSPI draws glyphs ~56px below its datum, see
// Faceplates.h HiTech time) sits inside the buffer; digits land centered on
// y=120. Font status is logged once (Font6 is the compiled-in fallback).
inline void drawTimerOverlay(unsigned long now, bool force = false) {
  static int fontReady = -1; // -1 = unchecked, 0 = VLW missing, 1 = loaded
  static unsigned long lastPush = 0;
  static String lastSig = "";
  static String lastMain = "";

  // Timer released the screen (done/paused past hold): release the sprite.
  if (!timerShouldDraw(now)) {
    freeTimerOverlaySprite();
    return;
  }

  unsigned long ms = timerCurrentMs(now);
  bool sw = (timerState.mode == 1);

  char buf[16];
  String main = "";
  if (sw) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", ms / 60000UL, (ms / 1000UL) % 60UL, (ms / 10UL) % 100UL);
    main = String(buf).substring(0, String(buf).lastIndexOf(':'));
  } else {
    snprintf(buf, sizeof(buf), "%lu:%02lu", ms / 60000UL, (ms / 1000UL) % 60UL);
    main = String(buf);
  }
  String sig = String(buf);

  // Full redraw when the M:SS part changes (1 Hz). For the stopwatch, also
  // redraw the hundredths strip at ~20 Hz without touching the rest of the screen.
  bool fullRedraw = force || (main != lastMain);
  bool hhRedraw = sw && !fullRedraw && (sig != lastSig) && (now - lastPush >= 50);
  if (!fullRedraw && !hhRedraw) return;
  if (fullRedraw) lastMain = main;
  lastSig = sig;
  lastPush = now;

  if (fontReady == -1) {
    fontReady = LittleFS.exists("/" + String(TIMER_FONT) + ".vlw") ? 1 : 0;
    Logger::log("DISPLAY", "Timer font %s.vlw -> %s", TIMER_FONT, fontReady ? "found" : "MISSING (Font6 fallback)");
  }

  if (force) tft.fillScreen(TFT_BLACK); // blank the round screen once on entry

  // Sprite is full-screen wide so the widest stopwatch string "999:59:99" fits.
  // Created once and reused (font loaded once) while the timer owns the screen.
  if (!timerOverlayReady) {
    timerOverlayReady = (timerOverlaySpr.createSprite(240, 100) != nullptr);
    if (timerOverlayReady) {
      if (fontReady == 1) { timerOverlaySpr.loadFont(TIMER_FONT, LittleFS); timerOverlayFontLoaded = true; }
      else timerOverlaySpr.setTextFont(6);
    }
  }
  if (timerOverlayReady) {
    timerOverlaySpr.fillSprite(TFT_BLACK);
    timerOverlaySpr.setTextDatum(MC_DATUM);
    timerOverlaySpr.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    timerOverlaySpr.drawString(sig, 120, (fontReady == 1) ? 33 : 76);
    if (fullRedraw || hhRedraw) {
      if (sw) {
        int fullW = timerOverlaySpr.textWidth(sig);
        String hh = sig.substring(sig.lastIndexOf(':'));
        int hhW = timerOverlaySpr.textWidth(hh);
        int hhX = 120 + fullW / 2 - hhW;
        if (fullRedraw) {
          timerOverlaySpr.pushSprite(0, 44);
        } else {
          timerOverlaySpr.pushSprite(hhX, 44, hhX, 0, hhW, 100); // hundredths strip only
        }
      } else {
        timerOverlaySpr.pushSprite(0, 44);
      }
    }
  } else {
    // Sprite buffer unavailable (low heap) - fall back to a direct draw
    tft.fillRect(0, 100, 240, 40, TFT_BLACK);
    if (fontReady == 1) tft.loadFont(TIMER_FONT, LittleFS);
    else tft.setTextFont(6);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    tft.drawString(sig, 120, (fontReady == 1) ? 77 : 120);
    if (fontReady == 1) tft.unloadFont();
  }
}

#endif // TIMER_H
