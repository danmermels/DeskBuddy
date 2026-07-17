#ifndef STATS_H
#define STATS_H

#include <Arduino.h>

#include "State.h"

extern void saveDailyStats();

// Resets session-level metrics
inline void resetSessionStats() {
  appState.sessionDeskTime = 0;
  appState.sessionMotionTime = 0;
  appState.sessionDistanceSum = 0;
  appState.sessionDistanceCount = 0;
  appState.sessionDistanceAverage = 0.0;
}

// Resets daily counts on day session rollover
inline void resetDailyStats(uint32_t tempLastAway, int currentDay) {
  appStats.firstSitToday = true;
  appStats.firstSitEpoch = 0;
  appStats.breakCount = 0;
  appStats.totalDeskTime = 0;
  appStats.totalFocusTime = 0;
  appStats.totalBreakTime = 0;
  appStats.overnightBreakDuration = 0;
  appStats.lastAwayEpoch = tempLastAway; // Preserve for calculation
  appStats.dailyAiRequestCount = 0;
  appStats.longestSittingStreak = 0;
  appStats.latestBreakDuration = 0;
  appStats.totalMotionTime = 0;
  appStats.motionCount = 0;
  
  resetSessionStats();
  
  appState.isStopByTracking = false;
  appState.originalLastAwayEpoch = 0;
  appState.totalStopByTimeMs = 0;
  appStats.previousLatestBreakDuration = 0;
  
  appStats.lunchReminderTriggered = false;
  appStats.lastNtpDay = currentDay;
  appStats.fsWritesToday = 0;
  
  saveDailyStats();
}

#endif // STATS_H
