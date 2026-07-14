#ifndef STATS_H
#define STATS_H

#include <Arduino.h>

// Extern declarations for daily stats variables defined in main.cpp
extern bool firstSitToday;
extern uint32_t firstSitEpoch;
extern int breakCount;
extern unsigned long totalDeskTime;
extern unsigned long totalFocusTime;
extern unsigned long totalBreakTime;
extern unsigned long overnightBreakDuration;
extern uint32_t lastAwayEpoch;
extern int dailyAiRequestCount;
extern unsigned long longestSittingStreak;
extern unsigned long latestBreakDuration;
extern unsigned long totalMotionTime;
extern int motionCount;
extern unsigned long sessionDeskTime;
extern unsigned long sessionMotionTime;
extern unsigned long sessionDistanceSum;
extern unsigned long sessionDistanceCount;
extern float sessionDistanceAverage;
extern bool isStopByTracking;
extern uint32_t originalLastAwayEpoch;
extern unsigned long totalStopByTimeMs;
extern unsigned long previousLatestBreakDuration;
extern bool lunchReminderTriggered;
extern int lastNtpDay;

// Forward declaration of saveDailyStats defined in main.cpp
extern void saveDailyStats();

// Resets session-level metrics
inline void resetSessionStats() {
  sessionDeskTime = 0;
  sessionMotionTime = 0;
  sessionDistanceSum = 0;
  sessionDistanceCount = 0;
  sessionDistanceAverage = 0.0;
}

// Resets daily counts on day session rollover
inline void resetDailyStats(uint32_t tempLastAway, int currentDay) {
  firstSitToday = true;
  firstSitEpoch = 0;
  breakCount = 0;
  totalDeskTime = 0;
  totalFocusTime = 0;
  totalBreakTime = 0;
  overnightBreakDuration = 0;
  lastAwayEpoch = tempLastAway; // Preserve for calculation
  dailyAiRequestCount = 0;
  longestSittingStreak = 0;
  latestBreakDuration = 0;
  totalMotionTime = 0;
  motionCount = 0;
  
  resetSessionStats();
  
  isStopByTracking = false;
  originalLastAwayEpoch = 0;
  totalStopByTimeMs = 0;
  previousLatestBreakDuration = 0;
  
  lunchReminderTriggered = false;
  lastNtpDay = currentDay;
  
  saveDailyStats();
}

#endif // STATS_H
