#ifndef LEARNING_H
#define LEARNING_H

#include <Arduino.h>

#include "State.h"

inline uint8_t getEffectivePresence(int dayIndex, int h) {
  if (dayIndex < 0 || dayIndex >= 7) dayIndex = 1; // Default to Monday
  if (h < 0 || h >= 24) return 0;
  
  if (appStats.historyDaysCountWeekly[dayIndex] == 0) {
    return 0; // No data yet — no assumed pattern
  }
  return appStats.hourlyPresenceHistoryWeekly[dayIndex][h];
}

// Scans for the typical start of the workday (first hour >= 15% presence between 4 AM and 12 PM)
inline int getLearnedWorkdayStart(const uint8_t* history) {
  for (int h = 4; h <= 12; h++) {
    if (history[h] >= 15) {
      return h;
    }
  }
  return 8; // Fallback to 8 AM
}

inline int getLearnedWorkdayStart(int dayIndex) {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(dayIndex, h);
  }
  return getLearnedWorkdayStart(eff);
}

// Scans for the typical end of the workday (last hour >= 15% presence checked from 4 PM onwards)
inline int getLearnedWorkdayEnd(const uint8_t* history) {
  int lastActive = 18; // Fallback to 6 PM (18:00)
  for (int i = 16; i < 28; i++) {
    int h = i % 24;
    if (history[h] >= 15) {
      lastActive = h;
    }
  }
  return lastActive;
}

inline int getLearnedWorkdayEnd(int dayIndex) {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(dayIndex, h);
  }
  return getLearnedWorkdayEnd(eff);
}

// Finds the hour between 11 AM and 2 PM with the lowest presence (representing lunch)
inline int getLearnedLunchHour(const uint8_t* history) {
  int bestHour = 12;
  int minVal = 100;
  for (int h = 11; h <= 14; h++) {
    int val = history[h];
    if (val < minVal) {
      minVal = val;
      bestHour = h;
    }
  }
  return bestHour;
}

inline int getLearnedLunchHour(int dayIndex) {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(dayIndex, h);
  }
  return getLearnedLunchHour(eff);
}

#endif // LEARNING_H
