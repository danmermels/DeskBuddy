#ifndef PRESENCE_ANALYSIS_H
#define PRESENCE_ANALYSIS_H

#include <Arduino.h>
#include "State.h"
#include "Logger.h"

// Presence Analysis Functions (formerly in Learning.h)

inline void accumulatePresence(int hour, unsigned long elapsedMs) {
  
  if (hour >= 0 && hour < 24) {
    appStats.presenceMsCurrentDay[hour] += elapsedMs;
  }
}

inline bool shouldResetDaySession(uint32_t currentEpoch, uint32_t referenceAwayEpoch, int currentDay, int referenceNtpDay) {
  Logger::log("STATE", "ResetCheck: cur=%s ref=%s curDay=%d refDay=%d", 
              Logger::formatEpoch(currentEpoch).c_str(), Logger::formatEpoch(referenceAwayEpoch).c_str(), currentDay, referenceNtpDay);
  
  if (referenceAwayEpoch == 0) {
    Logger::log("STATE", "ResetCheck: true (fresh boot/no departure)");
    return true; // Fresh startup / no prior departure, perform reset
  }

  if (currentEpoch < referenceAwayEpoch) {
    Logger::log("STATE", "ResetCheck: true (clock discrepancy: cur < ref)");
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

  Logger::log("STATE", "ResetCheck: abs=%u s (%u h %u m) dep=%02d:00 depDay=%d depWork=%d th=%u s (%u h)", 
              absenceDuration, absenceDuration / 3600, (absenceDuration % 3600) / 60, departureHour, departureDay, departedDuringWork, threshold, threshold / 3600);

  if (currentDay == referenceNtpDay) {
    // SAME CALENDAR DAY ROLLOVER CRITERIA:
    // We only reset the session on the same day if:
    // 1. The previous session today was very short (less than 15 minutes / 900,000ms of desk time).
    // 2. The absence duration is long enough (greater than or equal to the dynamic threshold).
    // This preserves the session if the user takes a long lunch/break after a productive morning.
    if (appStats.totalDeskTime < 900000UL && absenceDuration >= threshold) {
      Logger::log("STATE", "ResetCheck: true (same-day reset, deskTime=%u)", appStats.totalDeskTime);
      return true;
    }
    Logger::log("STATE", "ResetCheck: false (same-day, resume session)");
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

    Logger::log("STATE", "ResetCheck: diff-day. curHr=%d lStart=%d lim=%d", currentHour, learnedStart, limit);

    // If we are before the limit hour and the absence was less than 10 hours (36,000s):
    // Treat this as a temporary night interruption (e.g. bathroom visit) and do not roll over.
    if (currentHour < limit && absenceDuration < 36000UL) {
      Logger::log("STATE", "ResetCheck: false (night interruption, curHr < lim, abs < 10h)");
      return false; // Treat as night interruption, do not rollover yet
    }
    Logger::log("STATE", "ResetCheck: true (rollover, abs >= 4h)");
    return true; // Valid workday rollover
  }
  Logger::log("STATE", "ResetCheck: false (diff-day, abs < 4h)");
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
