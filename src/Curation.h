#ifndef CURATION_H
#define CURATION_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "State.h"
#include "Learning.h"
#include "Constants.h"

// Thresholds defined as constants
#define EXCESSIVE_BREAKS_LIMIT_PER_HOUR 1.0
#define LONG_BREAK_MULTIPLIER           3

extern String formatTime(unsigned long ms);
extern NTPClient timeClient;

inline int dateToDays(String dateStr) {
  if (dateStr.length() != 10) return 0;
  int y = dateStr.substring(0, 4).toInt();
  int m = dateStr.substring(5, 7).toInt();
  int d = dateStr.substring(8, 10).toInt();
  
  // Simple conversion to days since year 2000
  int days = (y - 2000) * 365;
  // Add leap years
  days += (y - 2000) / 4;
  
  // Month days offset (non-leap year baseline)
  const int monthDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  days += monthDays[m - 1];
  if (m > 2 && (y % 4 == 0)) {
    days += 1;
  }
  days += d;
  return days;
}

inline String getTodoObservations(int eventType) {
  String obs = "";
  if (!LittleFS.exists("/todo.json")) {
    return obs;
  }
  
  fs::File file = LittleFS.open("/todo.json", "r");
  if (!file) {
    return obs;
  }

  // Parse JSON
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    return obs;
  }

  // Current date parameters from NTP
  int currentYear = 2026;
  int currentMonth = 7;
  int currentDay = 18;
  int currentHour = 12;
  String currentMonthString = "2026-07";
  String currentDayString = "2026-07-18";
  int currentDaysCount = 0;

  if (timeClient.isTimeSet()) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      currentYear = ptm->tm_year + 1900;
      currentMonth = ptm->tm_mon + 1;
      currentDay = ptm->tm_mday;
      currentHour = ptm->tm_hour;

      char mStr[8];
      snprintf(mStr, sizeof(mStr), "%04d-%02d", currentYear, currentMonth);
      currentMonthString = String(mStr);

      char dStr[11];
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", currentYear, currentMonth, currentDay);
      currentDayString = String(dStr);
      
      currentDaysCount = dateToDays(currentDayString);
    }
  }

  // 1. Check daily tasks
  int dailyTotal = 0;
  int dailyUncompleted = 0;
  int dailyOverdueMoreThan3Days = 0;
  String dailyListStr = "";
  String overdueDailyList = "";
  
  if (doc.containsKey("daily")) {
    JsonArray daily = doc["daily"].as<JsonArray>();
    for (JsonObject task : daily) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveToday = false;
      int tHour = task["hour"] | 12;
      int tMin = task["minute"] | 0;
      String taskText = task["text"] | "";
      String tDate = task["startDate"] | "";

      if (isRecurrent) {
        String endDate = task["endDate"] | "";
        if ((tDate.length() == 0 || currentDayString >= tDate) &&
            (endDate.length() == 0 || currentDayString < endDate)) {
          isActiveToday = true;
        }
        if (task.containsKey("completedDates")) {
          JsonArray compDates = task["completedDates"].as<JsonArray>();
          for (JsonVariant d : compDates) {
            if (d.as<String>() == currentDayString) {
              isCompleted = true;
              break;
            }
          }
        }
      } else {
        String targetDate = task["targetDate"] | "";
        isActiveToday = (targetDate == currentDayString);
        isCompleted = task["completed"] | false;
        tDate = targetDate;
      }

      if (isActiveToday) {
        dailyTotal++;
        if (!isCompleted) {
          dailyUncompleted++;
          char timeBuf[10];
          snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tHour, tMin);
          dailyListStr += "- Daily task: " + taskText + " (due " + String(timeBuf) + ")\n";
        }
      }

      // Check if highly overdue (overdue > 3 days)
      if (!isCompleted && tDate.length() == 10) {
        int diff = currentDaysCount - dateToDays(tDate);
        if (diff > TASK_OVERDUE_DAYS_LIMIT) {
          dailyOverdueMoreThan3Days++;
          overdueDailyList += "- Daily task: " + taskText + " (overdue by " + String(diff) + " days!)\n";
        }
      }
    }
  }

  // 2. Check monthly tasks
  int monthlyTotal = 0;
  int monthlyUncompleted = 0;
  int monthlyOverdue = 0;
  int monthlyDueToday = 0;
  int monthlyOverdueMoreThan3Months = 0;
  String monthlyListStr = "";
  String overdueMonthlyList = "";
  
  if (doc.containsKey("monthly")) {
    JsonArray monthly = doc["monthly"].as<JsonArray>();
    for (JsonObject task : monthly) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveThisMonth = false;
      int dueDay = task["day"] | 1;
      String taskText = task["text"] | "";
      int tMonth = 1;
      int tYear = 2026;

      if (isRecurrent) {
        String startMonth = task["startMonth"] | "";
        String endMonth = task["endMonth"] | "";
        if ((startMonth.length() == 0 || currentMonthString >= startMonth) &&
            (endMonth.length() == 0 || currentMonthString < endMonth)) {
          isActiveThisMonth = true;
        }
        if (task.containsKey("completedMonths")) {
          JsonArray compMonths = task["completedMonths"].as<JsonArray>();
          for (JsonVariant m : compMonths) {
            if (m.as<String>() == currentMonthString) {
              isCompleted = true;
              break;
            }
          }
        }
        if (startMonth.length() == 7) {
          tYear = startMonth.substring(0, 4).toInt();
          tMonth = startMonth.substring(5, 7).toInt();
        }
      } else {
        tMonth = task["month"] | 1;
        tYear = task["year"] | 2026;
        isActiveThisMonth = (tMonth == currentMonth && tYear == currentYear);
        isCompleted = task["completed"] | false;
      }

      if (isActiveThisMonth) {
        monthlyTotal++;
        if (!isCompleted) {
          monthlyUncompleted++;
          if (currentDay == dueDay) {
            monthlyDueToday++;
            monthlyListStr += "- Monthly task: " + taskText + " (DUE TODAY: Day " + String(dueDay) + ")\n";
          } else if (currentDay > dueDay) {
            monthlyOverdue++;
            monthlyListStr += "- Monthly task: " + taskText + " (OVERDUE: was due Day " + String(dueDay) + ")\n";
          } else {
            monthlyListStr += "- Monthly task: " + taskText + " (due Day " + String(dueDay) + ")\n";
          }
        }
      }

      // Check if highly overdue (overdue > 3 months)
      if (!isCompleted) {
        int diffMonths = (currentYear - tYear) * 12 + (currentMonth - tMonth);
        if (diffMonths > TASK_OVERDUE_MONTHS_LIMIT) {
          monthlyOverdueMoreThan3Months++;
          overdueMonthlyList += "- Monthly task: " + taskText + " (overdue by " + String(diffMonths) + " months!)\n";
        }
      }
    }
  }

  // Format Observations output based on trigger event type
  if (eventType == EVENT_JOURNAL) {
    obs += "User is requesting a task journal overview. Here are the remaining tasks to complete:\n";
    obs += "Daily Tasks remaining: " + String(dailyUncompleted) + " out of " + String(dailyTotal) + "\n";
    if (dailyUncompleted > 0) {
      obs += "Pending Daily Tasks:\n" + dailyListStr;
    }
    obs += "Monthly Tasks remaining: " + String(monthlyUncompleted) + " out of " + String(monthlyTotal) + "\n";
    if (monthlyUncompleted > 0) {
      obs += "Pending Monthly Tasks:\n" + monthlyListStr;
    }
  } else if (eventType == EVENT_NAGGING) {
    obs += "Highly Overdue Tasks Alert! User has tasks that are severely overdue:\n";
    if (dailyOverdueMoreThan3Days > 0) {
      obs += "Daily tasks overdue by > " + String(TASK_OVERDUE_DAYS_LIMIT) + " days:\n" + overdueDailyList;
    }
    if (monthlyOverdueMoreThan3Months > 0) {
      obs += "Monthly tasks overdue by > " + String(TASK_OVERDUE_MONTHS_LIMIT) + " months:\n" + overdueMonthlyList;
    }
  } else {
    // Normal prompt flows: check for dynamic observations
    // 1. Midday task check
    if (timeClient.isTimeSet() && currentHour >= MIDDAY_TASK_CHECK_HOUR && dailyUncompleted > 0) {
      obs += "- Observation: Past midday (it is " + String(currentHour) + ":00) and user has " + String(dailyUncompleted) + " uncompleted daily tasks remaining.\n";
    }

    // 2. Overdue/due today tasks
    if (monthlyDueToday > 0) {
      obs += "- Observation: User has " + String(monthlyDueToday) + " monthly task(s) DUE TODAY that are not completed.\n";
    }
    if (monthlyOverdue > 0) {
      obs += "- Observation: User has " + String(monthlyOverdue) + " monthly task(s) OVERDUE that are not completed.\n";
    }

    // 3. Nagging observations in normal prompts if overdue limits are breached
    if (dailyOverdueMoreThan3Days > 0) {
      obs += "- Observation: User has " + String(dailyOverdueMoreThan3Days) + " daily task(s) overdue by more than " + String(TASK_OVERDUE_DAYS_LIMIT) + " days!\n";
    }
    if (monthlyOverdueMoreThan3Months > 0) {
      obs += "- Observation: User has " + String(monthlyOverdueMoreThan3Months) + " monthly task(s) overdue by more than " + String(TASK_OVERDUE_MONTHS_LIMIT) + " months!\n";
    }
  }

  return obs;
}

inline String getCurationObservations(int eventType, String detail) {
  String obs = "";

  // 1. Break frequency check
  double hoursWorked = (double)appStats.totalDeskTime / 3600000.0;
  if (hoursWorked > 0.5) {
    double breakRate = (double)appStats.breakCount / hoursWorked;
    if (breakRate > EXCESSIVE_BREAKS_LIMIT_PER_HOUR) {
      obs += "- Observation: User is taking breaks frequently today (averaging " + String(breakRate, 1) + " breaks per hour of work).\n";
    }
  }

  // 2. Unusually long break check
  if (eventType == EVENT_WELCOME_BACK) {
    unsigned long currentBreakMs = appState.currentBreakDurationMs;
    unsigned long avgBreakMs = 0;
    
    // Calculate average break duration from previous breaks today
    if (appStats.breakCount > 1) {
      avgBreakMs = appStats.totalBreakTime / appStats.breakCount;
    } else {
      avgBreakMs = 15 * 60 * 1000UL; // 15 mins default reference
    }

    if (currentBreakMs > avgBreakMs * LONG_BREAK_MULTIPLIER) {
      obs += "- Observation: The break just taken lasted " + detail + ", which is unusually long (more than 3x their average break duration of " + formatTime(avgBreakMs) + ").\n";
    }
  }

  // 3. Goal progress check
  double progressPct = 0.0;
  if (appConfig.targetHours > 0.0f) {
    progressPct = (appStats.totalDeskTime * 100.0) / (appConfig.targetHours * 3600000.0);
  }
  if (progressPct >= 100.0) {
    obs += "- Observation: User has successfully reached their daily desk time goal today!\n";
  } else if (progressPct > 75.0) {
    obs += "- Observation: User has completed over 75% of their daily desk time goal.\n";
  }

  // 4. TODO observations
  obs += getTodoObservations(eventType);

  return obs;
}

#endif // CURATION_H
