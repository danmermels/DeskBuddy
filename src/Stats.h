#ifndef STATS_H
#define STATS_H

#include <Arduino.h>

#include "State.h"

extern void saveDailyStats();

extern void clearRecentMotionWindow();

// Resets session-level metrics
inline void resetSessionStats() {
  appState.sessionDeskTime = 0;
  appState.sessionMotionTime = 0;
  appState.sessionDistanceSum = 0;
  appState.sessionDistanceCount = 0;
  appState.sessionDistanceAverage = 0.0;
  clearRecentMotionWindow();
}

// Records the current task-diligence tally (done/total) for the given day and
// month into appStats, keeping the current-period snapshot and the rolling
// history rings (most-recent-first) in sync. Call on each journal generation.
inline void updateTodoTally(int dailyDone, int dailyTotal, int monthlyDone, int monthlyTotal,
                            const String& dayKey, const String& monthKey) {
  appStats.dailyTaskDone = dailyDone;
  appStats.dailyTaskTotal = dailyTotal;
  appStats.dailyTallyDate = dayKey;
  appStats.monthlyTaskDone = monthlyDone;
  appStats.monthlyTaskTotal = monthlyTotal;
  appStats.monthlyTallyMonth = monthKey;

  if (appStats.diligenceDailyDays[0] == dayKey) {
    appStats.diligenceDailyDone[0] = dailyDone;
    appStats.diligenceDailyTotal[0] = dailyTotal;
  } else {
    for (int i = 6; i > 0; i--) {
      appStats.diligenceDailyDays[i] = appStats.diligenceDailyDays[i - 1];
      appStats.diligenceDailyDone[i] = appStats.diligenceDailyDone[i - 1];
      appStats.diligenceDailyTotal[i] = appStats.diligenceDailyTotal[i - 1];
    }
    appStats.diligenceDailyDays[0] = dayKey;
    appStats.diligenceDailyDone[0] = dailyDone;
    appStats.diligenceDailyTotal[0] = dailyTotal;
  }

  if (appStats.diligenceMonthlyMonths[0] == monthKey) {
    appStats.diligenceMonthlyDone[0] = monthlyDone;
    appStats.diligenceMonthlyTotal[0] = monthlyTotal;
  } else {
    for (int i = 11; i > 0; i--) {
      appStats.diligenceMonthlyMonths[i] = appStats.diligenceMonthlyMonths[i - 1];
      appStats.diligenceMonthlyDone[i] = appStats.diligenceMonthlyDone[i - 1];
      appStats.diligenceMonthlyTotal[i] = appStats.diligenceMonthlyTotal[i - 1];
    }
    appStats.diligenceMonthlyMonths[0] = monthKey;
    appStats.diligenceMonthlyDone[0] = monthlyDone;
    appStats.diligenceMonthlyTotal[0] = monthlyTotal;
  }
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
  appState.wasFirstSitThisSession = false;
  appState.originalLastAwayEpoch = 0;
  appState.totalStopByTimeMs = 0;
  appStats.previousLatestBreakDuration = 0;
  appStats.lunchReminderTriggered = false;
  appStats.excessiveBreaksTriggered = false;
  appStats.goalCompletedTriggered = false;
  appStats.morningJournalTriggered = false;
  appStats.preLunchJournalTriggered = false;
  appStats.endOfDayJournalTriggered = false;
  appStats.nagQueueIndex = 0;
  appStats.lastNtpDay = currentDay;
  appStats.fsWritesToday = 0;
  
  saveDailyStats();
}

#endif // STATS_H
