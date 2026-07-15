#ifndef LEARNING_H
#define LEARNING_H

#include <Arduino.h>

// Learning and Day Session Rollover Variables (extern declarations, defined in main.cpp)
extern uint8_t hourlyPresenceHistory[24];
extern uint32_t presenceMsCurrentDay[24];
extern int historyDaysCount;
extern bool lunchReminderTriggered;

// Predefined occupancy pattern (simulating a standard 9-to-6 workday with lunch break)
const uint8_t PREDEFINED_PATTERN[24] = {
  0, 0, 0, 0, 0, 0, 0, 0,  // 12 AM - 7 AM
  10,                      // 8 AM
  60, 75, 80,              // 9 AM - 11 AM
  15,                      // 12 PM (lunch)
  45, 70, 75, 65,          // 1 PM - 4 PM
  50,                      // 5 PM
  20,                      // 6 PM (wrap-up / overtime)
  5,                       // 7 PM
  0, 0, 0, 0               // 8 PM - 11 PM
};

inline uint8_t getEffectivePresence(int h) {
  if (h < 0 || h >= 24) return 0;
  if (historyDaysCount <= 0) {
    return PREDEFINED_PATTERN[h];
  } else if (historyDaysCount == 1) {
    return (uint8_t)((2 * (uint16_t)PREDEFINED_PATTERN[h] + hourlyPresenceHistory[h]) / 3);
  } else if (historyDaysCount == 2) {
    return (uint8_t)(((uint16_t)PREDEFINED_PATTERN[h] + 2 * hourlyPresenceHistory[h]) / 3);
  } else {
    return hourlyPresenceHistory[h];
  }
}

// Forward declarations
inline int getLearnedWorkdayStart();
inline int getLearnedWorkdayEnd();
inline int getLearnedLunchHour();

// Scans for the typical start of the workday (first hour >= 15% presence between 4 AM and 12 PM)
inline int getLearnedWorkdayStart(const uint8_t* history) {
  for (int h = 4; h <= 12; h++) {
    if (history[h] >= 15) {
      return h;
    }
  }
  return 8; // Fallback to 8 AM
}

inline int getLearnedWorkdayStart() {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(h);
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

inline int getLearnedWorkdayEnd() {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(h);
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

inline int getLearnedLunchHour() {
  uint8_t eff[24];
  for (int h = 0; h < 24; h++) {
    eff[h] = getEffectivePresence(h);
  }
  return getLearnedLunchHour(eff);
}

#endif // LEARNING_H
