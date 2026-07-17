#ifndef PRESENCE_ANALYSIS_H
#define PRESENCE_ANALYSIS_H

#include <Arduino.h>
#include "State.h"

// Presence Analysis Functions (formerly in Learning.h)

inline void accumulatePresence(int hour, unsigned long elapsedMs) {
  
  if (hour >= 0 && hour < 24) {
    appStats.presenceMsCurrentDay[hour] += elapsedMs;
  }
}

inline bool shouldResetDaySession(uint32_t currentEpoch, uint32_t referenceAwayEpoch, int currentDay, int referenceNtpDay) {
  if (referenceAwayEpoch == 0) {
    return true; // Fresh startup / no prior departure, perform reset
  }

  if (currentEpoch < referenceAwayEpoch) {
    return true; // Time sync discrepancy, perform safety reset
  }

  uint32_t absenceDuration = currentEpoch - referenceAwayEpoch;

  // Calculate local departure hour using gmtime_r (since local offset is already shifted in NTPClient)
  time_t rawDeparture = (time_t)referenceAwayEpoch;
  struct tm depTime;
  gmtime_r(&rawDeparture, &depTime);
  int departureHour = depTime.tm_hour;
  if (departureHour < 0 || departureHour >= 24) departureHour = 0;

  int departureDay = depTime.tm_wday;
  if (departureDay < 0 || departureDay >= 7) departureDay = 1;

  // Work hours: occupancy history probability >= 15%
  bool departedDuringWork = (getEffectivePresence(departureDay, departureHour) >= 15);

  // Dynamic threshold: 3 hours if departing during workday (known off-hours), 7 hours default (unknown/work hours)
  uint32_t threshold = departedDuringWork ? 10800UL : 25200UL;

  if (currentDay == referenceNtpDay) {
    // SAME CALENDAR DAY ROLLOVER CRITERIA:
    // We only reset the session on the same day if:
    // 1. The previous session today was very short (less than 15 minutes / 900,000ms of desk time).
    // 2. The absence duration is long enough (greater than or equal to the dynamic threshold).
    // This preserves the session if the user takes a long lunch/break after a productive morning.
    if (appStats.totalDeskTime < 900000UL && absenceDuration >= threshold) {
      return true;
    }
    return false; // Same calendar day, continue current session
  }
  
  // DIFFERENT CALENDAR DAY ROLLOVER CRITERIA:
  // We use a lower threshold (4 hours = 14,400s) to catch short sleep windows (like 1 AM to 6 AM).
  if (absenceDuration >= 14400UL) {
    time_t rawCurrent = (time_t)currentEpoch;
    struct tm curTime;
    gmtime_r(&rawCurrent, &curTime);
    int currentHour = curTime.tm_hour;
    if (currentHour < 0 || currentHour >= 24) currentHour = 0;

    // Retrieve learned workday start hour for today (default 8 AM).
    int learnedStart = getLearnedWorkdayStart(currentDay);
    
    // Set a limit window (typically 3 hours before learned start, bounded to 3 AM minimum).
    int limit = learnedStart - 3;
    if (limit < 3) limit = 3;

    // If we are before the limit hour and the absence was less than 10 hours (36,000s):
    // Treat this as a temporary night interruption (e.g. bathroom visit) and do not roll over.
    if (currentHour < limit && absenceDuration < 36000UL) {
      return false; // Treat as night interruption, do not rollover yet
    }
    return true; // Valid workday rollover
  }
  return false;
}

inline void mergeCurrentDayPresence(int dayIndex) {
  if (dayIndex < 0 || dayIndex >= 7) dayIndex = 1; // Default to Monday
  
  for (int h = 0; h < 24; h++) {
    uint32_t todayMs = appStats.presenceMsCurrentDay[h];
    // Maximum milliseconds in an hour is 3,600,000
    uint8_t todayPct = (uint8_t)constrain((todayMs * 100UL) / 3600000UL, 0UL, 100UL);

    // Blend: 60% existing history, 40% today's percentage
    appStats.hourlyPresenceHistoryWeekly[dayIndex][h] = 
      (uint8_t)((appStats.hourlyPresenceHistoryWeekly[dayIndex][h] * 3 + todayPct * 2) / 5);

    appStats.presenceMsCurrentDay[h] = 0; // Clear accumulator for the new session
  }
  
  if (appStats.historyDaysCountWeekly[dayIndex] < 1000) {
    appStats.historyDaysCountWeekly[dayIndex]++;
  }
}

#endif // PRESENCE_ANALYSIS_H
