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

// Add elapsed milliseconds to the current hour's presence accumulator
inline void accumulatePresence(int hour, unsigned long elapsedMs) {
  if (hour >= 0 && hour < 24) {
    presenceMsCurrentDay[hour] += elapsedMs;
  }
}

// Forward declarations
inline int getLearnedWorkdayStart();

// Determines if we should perform a day session rollover
inline bool shouldResetDaySession(uint32_t currentEpoch, uint32_t lastAwayEpoch, int currentDay, int lastNtpDay) {
  extern unsigned long totalDeskTime;

  if (lastAwayEpoch == 0) {
    return true; // Fresh startup / no prior departure, perform reset
  }

  if (currentEpoch < lastAwayEpoch) {
    return true; // Time sync discrepancy, perform safety reset
  }

  uint32_t absenceDuration = currentEpoch - lastAwayEpoch;

  // Calculate local departure hour using gmtime_r (since local offset is already shifted in NTPClient)
  time_t rawDeparture = (time_t)lastAwayEpoch;
  struct tm depTime;
  gmtime_r(&rawDeparture, &depTime);
  int departureHour = depTime.tm_hour;
  if (departureHour < 0 || departureHour >= 24) departureHour = 0;

  // Work hours: occupancy history probability >= 15%
  bool departedDuringWork = (getEffectivePresence(departureHour) >= 15);

  // Dynamic threshold: 7 hours if departing during workday, 3 hours if departing during sleep/off hours
  uint32_t threshold = departedDuringWork ? 25200UL : 10800UL;

  if (currentDay == lastNtpDay) {
    // If it is the same calendar day, we only rollover if:
    // 1. The previous session today was very short (less than 15 minutes of desk time)
    // 2. The absence duration is greater than or equal to the threshold
    if (totalDeskTime < 900000UL && absenceDuration >= threshold) {
      return true;
    }
    return false; // Same calendar day, continue current session
  }
  
  if (absenceDuration >= threshold) {
    // Check if this is a late-night/early-morning quick check (night interruption)
    time_t rawCurrent = (time_t)currentEpoch;
    struct tm curTime;
    gmtime_r(&rawCurrent, &curTime);
    int currentHour = curTime.tm_hour;
    if (currentHour < 0 || currentHour >= 24) currentHour = 0;

    int learnedStart = getLearnedWorkdayStart();
    int limit = learnedStart - 3;
    if (limit < 3) limit = 3;

    if (currentHour < limit && absenceDuration < 36000UL) { // < 10 hours absence
      return false; // Treat as night interruption, do not rollover yet
    }
    return true; // Valid rollover
  }
  return false;
}

// Integrates today's presence percentage into history using exponential moving average
inline void mergeCurrentDayPresence() {
  for (int h = 0; h < 24; h++) {
    uint32_t todayMs = presenceMsCurrentDay[h];
    // Maximum milliseconds in an hour is 3,600,000
    uint8_t todayPct = (uint8_t)constrain((todayMs * 100UL) / 3600000UL, 0UL, 100UL);

    if (historyDaysCount == 0) {
      hourlyPresenceHistory[h] = todayPct;
    } else {
      // Exponential moving average: 80% history weight, 20% today weight
      hourlyPresenceHistory[h] = (uint8_t)((hourlyPresenceHistory[h] * 4 + todayPct) / 5);
    }
    presenceMsCurrentDay[h] = 0; // Clear accumulator for the new session
  }
  
  if (historyDaysCount < 30) {
    historyDaysCount++;
  }
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

// Determines the dynamic validation buffer duration based on learned occupancy
inline uint32_t getDynamicValidationBufferMs(int hour) {
  if (hour < 0 || hour >= 24) return 180000UL;

  // If typical presence for this hour is >= 15% (active work hour): 45-second buffer
  // If typical presence is < 15% (off-work/sleep hour): 3-minute buffer
  return (getEffectivePresence(hour) >= 15) ? 45000UL : 180000UL;
}

#endif // LEARNING_H
