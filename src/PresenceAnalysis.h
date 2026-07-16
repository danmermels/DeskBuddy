#ifndef PRESENCE_ANALYSIS_H
#define PRESENCE_ANALYSIS_H

#include <Arduino.h>

// Presence Analysis Functions (formerly in Learning.h)

inline void accumulatePresence(int hour, unsigned long elapsedMs) {
  extern uint32_t presenceMsCurrentDay[24];
  if (hour >= 0 && hour < 24) {
    presenceMsCurrentDay[hour] += elapsedMs;
  }
}

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

  // Forward declarations (defined in Learning.h)
  extern uint8_t getEffectivePresence(int dayIndex, int h);
  int learnedStart = 8; // Fallback (actual implementation in Learning.h)

  int departureDay = depTime.tm_wday;
  if (departureDay < 0 || departureDay >= 7) departureDay = 1;

  // Work hours: occupancy history probability >= 15%
  bool departedDuringWork = (getEffectivePresence(departureDay, departureHour) >= 15);

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

    int limit = learnedStart - 3;
    if (limit < 3) limit = 3;

    if (currentHour < limit && absenceDuration < 36000UL) { // < 10 hours absence
      return false; // Treat as night interruption, do not rollover yet
    }
    return true; // Valid rollover
  }
  return false;
}

inline void mergeCurrentDayPresence(int dayIndex) {
  extern uint8_t hourlyPresenceHistoryWeekly[7][24];
  extern uint32_t presenceMsCurrentDay[24];
  extern int historyDaysCountWeekly[7];
  
  if (dayIndex < 0 || dayIndex >= 7) dayIndex = 1; // Default to Monday
  
  int count = historyDaysCountWeekly[dayIndex];
  for (int h = 0; h < 24; h++) {
    uint32_t todayMs = presenceMsCurrentDay[h];
    // Maximum milliseconds in an hour is 3,600,000
    uint8_t todayPct = (uint8_t)constrain((todayMs * 100UL) / 3600000UL, 0UL, 100UL);

    if (count == 0) {
      hourlyPresenceHistoryWeekly[dayIndex][h] = todayPct;
    } else {
      // Exponential moving average: 80% history weight, 20% today weight
      hourlyPresenceHistoryWeekly[dayIndex][h] = (uint8_t)((hourlyPresenceHistoryWeekly[dayIndex][h] * 4 + todayPct) / 5);
    }
    presenceMsCurrentDay[h] = 0; // Clear accumulator for the new session
  }
  
  if (historyDaysCountWeekly[dayIndex] < 30) {
    historyDaysCountWeekly[dayIndex]++;
  }
}

#endif // PRESENCE_ANALYSIS_H
