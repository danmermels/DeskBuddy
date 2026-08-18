#ifndef CURATION_H
#define CURATION_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include "State.h"
#include "Stats.h"
#include "Learning.h"
#include "Constants.h"
#include "Points.h"

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

// For a recurrent monthly task, returns how many months before currentMonthString
// it was due but left uncompleted (carried-over misses). 0 = no prior miss.
inline int recurrentMonthlyMissedMonths(const JsonObject& task, const String& currentMonthString) {
  String startMonth = task["startMonth"] | "";
  if (startMonth.length() != 7 || currentMonthString.length() != 7) return 0;
  int y = currentMonthString.substring(0, 4).toInt();
  int m = currentMonthString.substring(5, 7).toInt();
  int missed = 0;
  while (true) {
    m--;
    if (m < 1) { m = 12; y--; }
    char buf[8];
    snprintf(buf, sizeof(buf), "%04d-%02d", y, m);
    String key = String(buf);
    if (key < startMonth) break;
    bool completed = false;
    if (task.containsKey("completedMonths")) {
      JsonArray comp = task["completedMonths"].as<JsonArray>();
      for (JsonVariant c : comp) {
        if (c.as<String>() == key) { completed = true; break; }
      }
    }
    if (!completed) missed++;
  }
  return missed;
}

// For a recurrent monthly task, returns the nearest (last) month before
// currentMonthString that was due but left uncompleted, or "" if none.
inline String recurrentLastMissedMonth(const JsonObject& task, const String& currentMonthString) {
  String startMonth = task["startMonth"] | "";
  if (startMonth.length() != 7 || currentMonthString.length() != 7) return "";
  int y = currentMonthString.substring(0, 4).toInt();
  int m = currentMonthString.substring(5, 7).toInt();
  while (true) {
    m--;
    if (m < 1) { m = 12; y--; }
    char buf[8];
    snprintf(buf, sizeof(buf), "%04d-%02d", y, m);
    String key = String(buf);
    if (key < startMonth) return "";
    bool completed = false;
    if (task.containsKey("completedMonths")) {
      JsonArray comp = task["completedMonths"].as<JsonArray>();
      for (JsonVariant c : comp) {
        if (c.as<String>() == key) { completed = true; break; }
      }
    }
    if (!completed) return key;
  }
}

// Signed diligence score: "+N" when ahead, plain "N" when balanced/behind.
inline String diligenceSigned(int net) {
  if (net > 0) return "+" + String(net);
  return String(net);
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
  DynamicJsonDocument doc(4096);
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
  std::vector<String> synthBullets;
  
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
        isActiveToday = (targetDate.length() == 0 || targetDate <= currentDayString);
        isCompleted = task["completed"] | false;
        tDate = targetDate;
      }

      if (isActiveToday) {
        dailyTotal++;
        if (!isCompleted) {
          dailyUncompleted++;
          if (tDate.length() == 10 && tDate < currentDayString) {
            String ddMm = tDate.substring(8, 10) + "/" + tDate.substring(5, 7);
            synthBullets.push_back("- Daily task: " + taskText + " (OVERDUE: was due " + ddMm + ")");
          } else {
            char timeBuf[12];
            if (appConfig.time24h) {
              snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tHour, tMin);
            } else {
              int h12 = tHour % 12;
              if (h12 == 0) h12 = 12;
              const char* ampm = (tHour >= 12) ? "PM" : "AM";
              snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h12, tMin, ampm);
            }
            synthBullets.push_back("- Daily task: " + taskText + " (due " + String(timeBuf) + ")");
          }
        }
      }

      // Check if highly overdue (overdue > 3 days) — recurrent standing tasks only;
      // carried-over non-recurrent tasks are already surfaced above.
      if (!isCompleted && isRecurrent && tDate.length() == 10) {
        int diff = currentDaysCount - dateToDays(tDate);
        if (diff > TASK_OVERDUE_DAYS_LIMIT) {
          dailyOverdueMoreThan3Days++;
          synthBullets.push_back("- Daily task: " + taskText + " (overdue by " + String(diff) + " days!)");
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
  
  if (doc.containsKey("monthly")) {
    JsonArray monthly = doc["monthly"].as<JsonArray>();
    for (JsonObject task : monthly) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveThisMonth = false;
      bool isCarriedOver = false;
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
        isCompleted = task["completed"] | false;
        int diffMonths = (currentYear - tYear) * 12 + (currentMonth - tMonth);
        if (diffMonths > 0) {
          isActiveThisMonth = true;
          isCarriedOver = true;
        } else if (diffMonths == 0) {
          isActiveThisMonth = true;
        }
      }

      if (isActiveThisMonth) {
        monthlyTotal++;
        if (!isCompleted) {
          monthlyUncompleted++;
          if (isCarriedOver) {
            monthlyOverdue++;
            char dueMonthBuf[8];
            snprintf(dueMonthBuf, sizeof(dueMonthBuf), "%04d-%02d", tYear, tMonth);
            synthBullets.push_back("- Monthly task: " + taskText + " (OVERDUE: was due Day " + String(dueDay) + " " + String(dueMonthBuf) + ")");
          } else if (currentDay == dueDay) {
            monthlyDueToday++;
            synthBullets.push_back("- Monthly task: " + taskText + " (DUE TODAY: Day " + String(dueDay) + ")");
          } else if (currentDay > dueDay || (isRecurrent && recurrentMonthlyMissedMonths(task, currentMonthString) > 0)) {
            monthlyOverdue++;
            String missStr = "";
            if (isRecurrent && recurrentMonthlyMissedMonths(task, currentMonthString) > 0) {
              missStr = " " + recurrentLastMissedMonth(task, currentMonthString);
            }
            synthBullets.push_back("- Monthly task: " + taskText + " (OVERDUE: was due Day " + String(dueDay) + missStr + ")");
          } else {
            synthBullets.push_back("- Monthly task: " + taskText + " (due Day " + String(dueDay) + ")");
          }
        }
      }

      // Check if highly overdue (overdue > 3 months) — recurrent standing tasks only;
      // carried-over non-recurrent tasks are already surfaced above.
      if (!isCompleted && isRecurrent) {
        int diffMonths = (currentYear - tYear) * 12 + (currentMonth - tMonth);
        if (diffMonths > TASK_OVERDUE_MONTHS_LIMIT) {
          monthlyOverdueMoreThan3Months++;
          synthBullets.push_back("- Monthly task: " + taskText + " (overdue by " + String(diffMonths) + " months!)");
        }
      }
    }
  }

  // Keep the persisted task-diligence tally (appStats + stats.json) fresh on every
  // AI call, not just on journal generation: done = total - uncompleted.
  int dailyDone = dailyTotal - dailyUncompleted;
  int monthlyDone = monthlyTotal - monthlyUncompleted;
  updateTodoTally(dailyDone, dailyTotal, monthlyDone, monthlyTotal, currentDayString, currentMonthString);

  // Compact task synthesis (counts + names) injected into AI prompt observations.
  // Covers everything the AI needs to talk about tasks: pending today, due today,
  // and overdue (daily >3d, monthly this-month + carried-over, recurrent >3mo), by name.
  String synthesis = "[TASK SYNTHESIS]\n";
  if (dailyTotal == 0 && monthlyTotal == 0) {
    synthesis += "No tasks on the list.\n";
  } else if (dailyUncompleted == 0 && monthlyUncompleted == 0 &&
             dailyOverdueMoreThan3Days == 0 && monthlyOverdue == 0 && monthlyOverdueMoreThan3Months == 0) {
    synthesis += "All tasks for today completed.\n";
  } else {
    int dailyNet = 2 * dailyDone - dailyTotal;
    int monthlyNet = 2 * monthlyDone - monthlyTotal;
    synthesis += String(dailyUncompleted) + "/" + String(dailyTotal) + " daily pending today, " +
                 String(monthlyDueToday) + " monthly due today; overdue: " +
                 String(dailyOverdueMoreThan3Days) + " daily (3d+), " +
                 String(monthlyOverdue) + " monthly (incl. carried-over), " +
                 String(monthlyOverdueMoreThan3Months) + " monthly (3mo+).\n";
    synthesis += "Diligence: daily " + String(dailyDone) + "/" + String(dailyTotal) + " (" + diligenceSigned(dailyNet) +
                 "), monthly " + String(monthlyDone) + "/" + String(monthlyTotal) + " (" + diligenceSigned(monthlyNet) + ").\n";
    // Shuffle the bullets so the AI doesn't always latch onto the first task in the list
    for (int i = (int)synthBullets.size() - 1; i > 0; i--) {
      int j = random(i + 1);
      String tmp = synthBullets[i];
      synthBullets[i] = synthBullets[j];
      synthBullets[j] = tmp;
    }
    for (size_t i = 0; i < synthBullets.size(); i++) {
      synthesis += synthBullets[i];
      synthesis += "\n";
    }
    if (synthesis.length() > TASK_SYNTHESIS_MAX_CHARS) {
      synthesis = synthesis.substring(0, TASK_SYNTHESIS_MAX_CHARS) + "...\n";
    }
  }

  // Format Observations output based on trigger event type
  if (eventType == EVENT_NAGGING) {
    obs += "Overdue Tasks Alert!\n";
    obs += synthesis;
  } else if (eventType == EVENT_POINTS) {
    // Points tracker check-in: surface the live running total so the AI can
    // react to an actual number instead of a placeholder.
    obs += "Points Tracker Check-in!\n";
    obs += "- " + buildPointsDetail() + " (running total this month).\n";
    obs += synthesis;
  } else {
    // All AI prompt flows receive the task synthesis
    obs += synthesis;
    if (eventType != EVENT_JOURNAL) {
      // Dynamic time-based observation (task counts/names live in the synthesis)
      if (timeClient.isTimeSet() && currentHour >= MIDDAY_TASK_CHECK_HOUR && dailyUncompleted > 0) {
        obs += "- Observation: Past midday (it is " + String(currentHour) + ":00) and user has " + String(dailyUncompleted) + " uncompleted daily tasks remaining.\n";
      }
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

inline String truncateTaskText(String text, int maxLen = 16) {
  if (text.length() <= maxLen) return text;
  return text.substring(0, maxLen - 3) + "...";
}

struct OverdueTask {
  String text;
  long expiredMinutes;
};

// Builds the overdue-task queue used by the seated nag (one task per ring,
// most-expired-first). Includes BOTH daily and monthly tasks so monthlies can
// be nagged too once their due day has passed.
inline std::vector<OverdueTask> buildOverdueTaskQueue(String currentDayString, String currentMonthString, int currentDaysCount, int currentYear, int currentMonth, int currentDay, int nowMinutes) {
  std::vector<OverdueTask> queue;
  if (!LittleFS.exists("/todo.json")) return queue;
  fs::File file = LittleFS.open("/todo.json", "r");
  if (!file) return queue;
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return queue;

  if (doc.containsKey("daily")) {
    JsonArray daily = doc["daily"].as<JsonArray>();
    for (JsonObject task : daily) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      int tHour = task["hour"] | 12;
      int tMin = task["minute"] | 0;
      String taskText = task["text"] | "";
      String tDate = task["startDate"] | "";
      if (isRecurrent) {
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
        isCompleted = task["completed"] | false;
        tDate = task["targetDate"] | "";
      }

      if (!isCompleted) {
        long expired = 0;
        bool overdue = false;
        if (isRecurrent) {
          // Standing daily task: overdue once today's due time has passed.
          expired = (long)nowMinutes - (tHour * 60 + tMin);
          overdue = expired > 0;
        } else if (tDate.length() == 10) {
          int diff = currentDaysCount - dateToDays(tDate);
          if (diff == 0) {
            // Due today: overdue once the due time has passed. Ignore previous days (diff > 0).
            expired = (long)nowMinutes - (tHour * 60 + tMin);
            overdue = expired > 0;
          }
        }
        if (overdue) {
          queue.push_back({taskText, expired});
        }
      }
    }
  }

  if (doc.containsKey("monthly")) {
    JsonArray monthly = doc["monthly"].as<JsonArray>();
    for (JsonObject task : monthly) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      int dueDay = task["day"] | 1;
      String taskText = task["text"] | "";
      int tMonth = 1;
      int tYear = 2026;
      if (isRecurrent) {
        if (task.containsKey("completedMonths")) {
          JsonArray compMonths = task["completedMonths"].as<JsonArray>();
          for (JsonVariant m : compMonths) {
            if (m.as<String>() == currentMonthString) {
              isCompleted = true;
              break;
            }
          }
        }
      } else {
        tMonth = task["month"] | 1;
        tYear = task["year"] | 2026;
        isCompleted = task["completed"] | false;
      }

      if (!isCompleted) {
        long expired = 0;
        bool overdue = false;
        if (isRecurrent) {
          int missedMonths = recurrentMonthlyMissedMonths(task, currentMonthString);
          if (missedMonths == 0 && currentDay > dueDay) {
            overdue = true;
            expired = (long)(currentDay - dueDay) * 1440L;
          }
        } else {
          int monthDiff = (currentYear - tYear) * 12 + (currentMonth - tMonth);
          if (monthDiff == 0 && currentDay > dueDay) {
            overdue = true;
            expired = (long)(currentDay - dueDay) * 1440L;
          }
        }
        if (overdue) {
          queue.push_back({taskText, expired});
        }
      }
    }
  }

  // Sort most-expired-first (stable insertion sort keeps small lists cheap)
  for (size_t i = 1; i < queue.size(); i++) {
    OverdueTask key = queue[i];
    size_t j = i;
    while (j > 0 && queue[j - 1].expiredMinutes < key.expiredMinutes) {
      queue[j] = queue[j - 1];
      j--;
    }
    queue[j] = key;
  }
  return queue;
}

inline JournalDashboardViewData getJournalDashboardData() {
  JournalDashboardViewData data;
  data.titleStr = "";
  data.dueTodayCount = 0;
  data.dailyCount = 0;
  data.monthlyCount = 0;
  data.diligenceScore = 0;

  if (!LittleFS.exists("/todo.json")) return data;
  fs::File file = LittleFS.open("/todo.json", "r");
  if (!file) return data;

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return data;

  int currentYear = 2026, currentMonth = 8, currentDay = 7;
  String currentMonthString = "2026-08", currentDayString = "2026-08-07";
  if (timeClient.isTimeSet()) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      currentYear = ptm->tm_year + 1900;
      currentMonth = ptm->tm_mon + 1;
      currentDay = ptm->tm_mday;
      char mStr[8], dStr[11];
      snprintf(mStr, sizeof(mStr), "%04d-%02d", currentYear, currentMonth);
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", currentYear, currentMonth, currentDay);
      currentMonthString = String(mStr);
      currentDayString = String(dStr);
    }
  }

  int dailyUncompleted = 0, monthlyUncompleted = 0;
  int dueTodayDailies = 0, dueTodayMonthlies = 0;
  int dailyActive = 0, dailyDone = 0, monthlyActive = 0, monthlyDone = 0;

  if (doc.containsKey("daily")) {
    for (JsonObject task : doc["daily"].as<JsonArray>()) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveToday = false;
      String tDate = task["startDate"] | "";

      if (isRecurrent) {
        String endDate = task["endDate"] | "";
        if ((tDate.length() == 0 || currentDayString >= tDate) &&
            (endDate.length() == 0 || currentDayString < endDate)) {
          isActiveToday = true;
        }
        if (task.containsKey("completedDates")) {
          for (JsonVariant d : task["completedDates"].as<JsonArray>()) {
            if (d.as<String>() == currentDayString) { isCompleted = true; break; }
          }
        }
      } else {
        isActiveToday = (String(task["targetDate"] | "") == currentDayString);
        isCompleted = task["completed"] | false;
      }
      if (isActiveToday) { dailyActive++; if (isCompleted) dailyDone++; }
      if (!isCompleted) { dailyUncompleted++; if (isActiveToday) dueTodayDailies++; }
    }
  }

  if (doc.containsKey("monthly")) {
    for (JsonObject task : doc["monthly"].as<JsonArray>()) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveThisMonth = false;
      String startMonth = task.containsKey("startMonth") ? String(task["startMonth"] | "") : (task.containsKey("startDate") ? String(task["startDate"] | "").substring(0, 7) : "");
      int dueDay = task.containsKey("day") ? (int)task["day"] : (task.containsKey("dayOfMonth") ? (int)task["dayOfMonth"] : 1);

      if (isRecurrent) {
        String endDate = task.containsKey("endDate") ? String(task["endDate"] | "").substring(0, 7) : (task.containsKey("endMonth") ? String(task["endMonth"] | "") : "");
        if ((startMonth.length() == 0 || currentMonthString >= startMonth) &&
            (endDate.length() == 0 || currentMonthString < endDate)) {
          isActiveThisMonth = true;
        }
        if (task.containsKey("completedMonths")) {
          for (JsonVariant d : task["completedMonths"].as<JsonArray>()) {
            if (d.as<String>() == currentMonthString) { isCompleted = true; break; }
          }
        }
      } else {
        String targetDate = task["targetDate"] | "";
        isActiveThisMonth = (targetDate.length() >= 7 && targetDate.substring(0, 7) == currentMonthString);
        isCompleted = task["completed"] | false;
      }
      if (isActiveThisMonth) { monthlyActive++; if (isCompleted) monthlyDone++; }
      if (!isCompleted) {
        monthlyUncompleted++;
        if (isRecurrent) {
          if (dueDay == currentDay) dueTodayMonthlies++;
        } else {
          if (String(task["targetDate"] | "") == currentDayString) dueTodayMonthlies++;
        }
      }
    }
  }

  data.dueTodayCount = dueTodayDailies + dueTodayMonthlies;
  data.dailyCount = dailyUncompleted;
  data.monthlyCount = monthlyUncompleted;

  int dailyNet = 2 * dailyDone - dailyActive;
  int monthlyNet = 2 * monthlyDone - monthlyActive;
  int net = dailyNet + monthlyNet;
  data.diligenceScore = (int)constrain(50 + net * 10, 0, 100);

  return data;
}

// Inline helper to build task journal summary and chunk pages
inline void buildTaskJournalSummary(std::vector<String>& pages, std::vector<int>& pageLines) {
  pages.clear();
  pageLines.clear();

  if (!LittleFS.exists("/todo.json")) {
    pages.push_back("[YELLOW]TODO\n[RED]No tasks found.\n[WHITE]Create in Web UI");
    pageLines.push_back(3);
    return;
  }
  
  fs::File file = LittleFS.open("/todo.json", "r");
  if (!file) {
    pages.push_back("[YELLOW]TODO\n[RED]Failed to open\n[WHITE]todo.json");
    pageLines.push_back(3);
    return;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    pages.push_back("[YELLOW]TODO\n[RED]Failed to parse\n[WHITE]todo.json");
    pageLines.push_back(3);
    return;
  }

  // Get current date details
  int currentYear = 2026;
  int currentMonth = 8;
  int currentDay = 7;
  String currentMonthString = "2026-08";
  String currentDayString = "2026-08-07";
  int currentDaysCount = 0;

  int curHour = 12;
  int curMin = 0;

  if (timeClient.isTimeSet()) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    if (ptm != nullptr) {
      currentYear = ptm->tm_year + 1900;
      currentMonth = ptm->tm_mon + 1;
      currentDay = ptm->tm_mday;
      curHour = ptm->tm_hour;
      curMin = ptm->tm_min;

      char mStr[8];
      snprintf(mStr, sizeof(mStr), "%04d-%02d", currentYear, currentMonth);
      currentMonthString = String(mStr);

      char dStr[11];
      snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", currentYear, currentMonth, currentDay);
      currentDayString = String(dStr);
      
      currentDaysCount = dateToDays(currentDayString);
    }
  }

  int dailyUncompleted = 0;
  int monthlyUncompleted = 0;
  int overdueDailies = 0;
  int dueTodayDailies = 0;
  int dueTodayMonthlies = 0;
  int overdueMonthlies = 0;
  int dailyActive = 0;
  int dailyDone = 0;
  int monthlyActive = 0;
  int monthlyDone = 0;

  std::vector<String> dailyListItems;
  std::vector<String> monthlyListItems;

  // 1. Process daily tasks
  if (doc.containsKey("daily")) {
    JsonArray daily = doc["daily"].as<JsonArray>();
    for (JsonObject task : daily) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveToday = false;
      String taskText = task["text"] | "";
      String tDate = task["startDate"] | "";
      int tHour = task["hour"] | 12;
      int tMin = task["minute"] | 0;

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
        dailyActive++;
        if (isCompleted) dailyDone++;
      }

      if (!isCompleted) {
        dailyUncompleted++;
        if (isActiveToday) {
          dueTodayDailies++;
          
          char timeBuf[12];
          if (appConfig.time24h) {
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tHour, tMin);
          } else {
            int h12 = tHour % 12;
            if (h12 == 0) h12 = 12;
            const char* ampm = (tHour >= 12) ? "PM" : "AM";
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h12, tMin, ampm);
          }
          String timeStr = String(timeBuf);

          bool isOverdueToday = (curHour > tHour || (curHour == tHour && curMin > tMin));
          String colorTag = isOverdueToday ? "[RED]" : "[GREEN]";

          dailyListItems.push_back(colorTag + timeStr + " | " + truncateTaskText(taskText, 30));
        } else if (!isRecurrent && tDate.length() == 10 && tDate < currentDayString) {
          overdueDailies++;
          
          String ddMm = tDate.substring(8, 10) + "/" + tDate.substring(5, 7);
          dailyListItems.push_back("[RED]" + ddMm + " | " + truncateTaskText(taskText, 30));
        }
      }
    }
  }

  // 2. Process monthly tasks
  struct MonthlyTaskEntry {
    String colorTag;
    int daysOrder;
    String dateStr;
    String text;
  };
  std::vector<MonthlyTaskEntry> monthlyEntries;

  if (doc.containsKey("monthly")) {
    JsonArray monthly = doc["monthly"].as<JsonArray>();
    for (JsonObject task : monthly) {
      bool isRecurrent = task["recurrent"] | false;
      bool isCompleted = false;
      bool isActiveThisMonth = false;
      int dueDay = task.containsKey("day") ? (int)task["day"] : (task.containsKey("dayOfMonth") ? (int)task["dayOfMonth"] : 1);
      String taskText = task["text"] | "";
      int tMonth = task["month"] | currentMonth;
      int tYear = task["year"] | currentYear;
      String targetDate = task["targetDate"] | "";
      if (targetDate.length() >= 10) {
        tYear = targetDate.substring(0, 4).toInt();
        tMonth = targetDate.substring(5, 7).toInt();
        dueDay = targetDate.substring(8, 10).toInt();
      }

      String startMonth = task.containsKey("startMonth") ? String(task["startMonth"] | "") : (task.containsKey("startDate") ? String(task["startDate"] | "").substring(0, 7) : "");
      String endMonth = task.containsKey("endMonth") ? String(task["endMonth"] | "") : (task.containsKey("endDate") ? String(task["endDate"] | "").substring(0, 7) : "");

      if (isRecurrent) {
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
      } else {
        isActiveThisMonth = (tMonth == currentMonth && tYear == currentYear);
        isCompleted = task["completed"] | false;
      }

      if (isActiveThisMonth) {
        monthlyActive++;
        if (isCompleted) monthlyDone++;
      }

      if (!isCompleted) {
        monthlyUncompleted++;
        if (isActiveThisMonth) {
          String missedMonth = (isRecurrent) ? recurrentLastMissedMonth(task, currentMonthString) : "";

          if (missedMonth.length() > 0) {
            overdueMonthlies++;
            int mYr = missedMonth.substring(0, 4).toInt();
            int mMon = missedMonth.substring(5, 7).toInt();
            char mbuf[6];
            snprintf(mbuf, sizeof(mbuf), "%02d/%02d", dueDay, mMon);
            monthlyEntries.push_back({ "[RED]", mYr * 10000 + mMon * 100 + dueDay, String(mbuf), taskText });
          }

          char dateBuf[6];
          snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", dueDay, currentMonth);
          int orderVal = currentYear * 10000 + currentMonth * 100 + dueDay;

          if (currentDay == dueDay) {
            dueTodayMonthlies++;
            monthlyEntries.push_back({ "[GREEN]", orderVal, String(dateBuf), taskText });
          } else if (currentDay > dueDay) {
            overdueMonthlies++;
            monthlyEntries.push_back({ "[RED]", orderVal, String(dateBuf), taskText });
          } else {
            monthlyEntries.push_back({ "[WHITE]", orderVal, String(dateBuf), taskText });
          }
        } else {
          if (!isRecurrent && (tYear < currentYear || (tYear == currentYear && tMonth < currentMonth))) {
            overdueMonthlies++;
            char dateBuf[6];
            snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", dueDay, tMonth);
            monthlyEntries.push_back({ "[RED]", tYear * 10000 + tMonth * 100 + dueDay, String(dateBuf), taskText });
          }
        }
      }
    }
  }

  // Sort monthly entries by most overdue first (earliest due date ascending)
  std::sort(monthlyEntries.begin(), monthlyEntries.end(), [](const MonthlyTaskEntry& a, const MonthlyTaskEntry& b) {
    return a.daysOrder < b.daysOrder;
  });

  for (const auto& entry : monthlyEntries) {
    monthlyListItems.push_back(entry.colorTag + entry.dateStr + " | " + truncateTaskText(entry.text, 30));
  }

  // Page 1 generation - Clean Center-Justified Dashboard
  updateTodoTally(dailyDone, dailyActive, monthlyDone, monthlyActive, currentDayString, currentMonthString);
  saveDailyStats();

#if DESKBUDDY_LANG_PTBR
  const char* titleStr       = "[YELLOW]RESUMO TODO\n";
  const char* secDaily       = "[BLUE]-- DIARIAS --\n";
  const char* lblOpen        = "[WHITE]Abertas: ";
  const char* lblDue         = "[GREEN]Para Hoje: ";
  const char* lblOver        = "[RED]Atrasadas: ";
  const char* secMonthly     = "[BLUE]-- MENSAIS --\n";
  const char* nameDaily      = "Diarias ";
  const char* nameMonthly    = "Mensais ";
  const char* secDailyList   = "[YELLOW]-- DIARIAS --";
  const char* secMonthlyList = "[YELLOW]-- MENSAIS --";
  const char* titleList      = "[YELLOW]LISTA DE TAREFAS (";
#else
  const char* titleStr       = "[YELLOW]TODO SUMMARY\n";
  const char* secDaily       = "[BLUE]-- DAILY --\n";
  const char* lblOpen        = "[WHITE]Open: ";
  const char* lblDue         = "[GREEN]Due Today: ";
  const char* lblOver        = "[RED]Overdue: ";
  const char* secMonthly     = "[BLUE]-- MONTHLY --\n";
  const char* nameDaily      = "Daily ";
  const char* nameMonthly    = "Monthly ";
  const char* secDailyList   = "[YELLOW]-- DAILY --";
  const char* secMonthlyList = "[YELLOW]-- MONTHLY --";
  const char* titleList      = "[YELLOW]TASK LIST (";
#endif

  String p1 = String(titleStr);
  p1 += String(secDaily);
  p1 += String(lblOpen) + String(dailyUncompleted) + "\n";
  p1 += String(lblDue) + String(dueTodayDailies) + "\n";
  p1 += String(lblOver) + String(overdueDailies) + "\n\n";
  p1 += String(secMonthly);
  p1 += String(lblOpen) + String(monthlyUncompleted) + "\n";
  p1 += String(lblDue) + String(dueTodayMonthlies) + "\n";
  p1 += String(lblOver) + String(overdueMonthlies) + "\n";

  int dailyNet = 2 * dailyDone - dailyActive;
  int monthlyNet = 2 * monthlyDone - monthlyActive;
  String dailyCol = (dailyNet >= 0) ? "[GREEN]" : "[RED]";
  String monthlyCol = (monthlyNet >= 0) ? "[GREEN]" : "[RED]";

  p1 += dailyCol + String(nameDaily) + diligenceSigned(dailyNet) + " | " + monthlyCol + String(nameMonthly) + diligenceSigned(monthlyNet);
  pages.push_back(p1);
  pageLines.push_back(11);

  // Group task list lines (Daily first, then Monthly rolls directly after)
  std::vector<String> pageLinesList;
  if (dailyListItems.size() > 0) {
    pageLinesList.push_back(String(secDailyList));
    for (const auto& item : dailyListItems) {
      pageLinesList.push_back(item);
    }
  }
  if (monthlyListItems.size() > 0) {
    pageLinesList.push_back(String(secMonthlyList));
    for (const auto& item : monthlyListItems) {
      pageLinesList.push_back(item);
    }
  }

  // Chunk pages (Controlled by TASK_LIST_MAX_PAGE_LINES in Constants.h / Faceplates.h)
  if (pageLinesList.size() > 0) {
    int currentPageIndex = 2;
    String currentPageContent = "";
    int currentLineCount = 0;

    for (size_t i = 0; i < pageLinesList.size(); i++) {
      if (currentLineCount == 0) {
        currentPageContent = String(titleList) + String(currentPageIndex - 1) + ")\n";
        currentLineCount = 1;
      }

      // Orphan Header Protection (if -- MENSAIS -- falls on the last line of a page)
      if (pageLinesList[i].startsWith("[YELLOW]--") && currentLineCount == (TASK_LIST_MAX_PAGE_LINES - 1)) {
        if (currentPageContent.length() > 0 && currentPageContent[currentPageContent.length() - 1] == '\n') {
          currentPageContent.remove(currentPageContent.length() - 1);
        }
        pages.push_back(currentPageContent);
        pageLines.push_back(currentLineCount);

        currentPageIndex++;
        currentPageContent = String(titleList) + String(currentPageIndex - 1) + ")\n";
        currentLineCount = 1;
      }

      currentPageContent += pageLinesList[i] + "\n";
      currentLineCount++;

      if (currentLineCount == TASK_LIST_MAX_PAGE_LINES || i == pageLinesList.size() - 1) {
        if (currentPageContent.length() > 0 && currentPageContent[currentPageContent.length() - 1] == '\n') {
          currentPageContent.remove(currentPageContent.length() - 1);
        }
        pages.push_back(currentPageContent);
        pageLines.push_back(currentLineCount);

        currentPageIndex++;
        currentPageContent = "";
        currentLineCount = 0;
      }
    }
  }
}

// Observation-driven curation nudge used by EVENT_CURATION (16).
// Scans live state for noteworthy patterns not covered by other triggers.
// Returns a human-readable detail string for the AI prompt, or empty if nothing found.
// Uses variety tracking: avoids repeating the same topic key two fires in a row.
inline String getCurationNudge() {
  static String lastKey = "";
  if (!timeClient.isTimeSet()) return "";

  time_t epoch = timeClient.getEpochTime();
  struct tm* ptm = localtime(&epoch);
  if (!ptm) return "";

  int currentHour = ptm->tm_hour;
  int currentDay = ptm->tm_wday;
  char dStr[11], mStr[8];
  snprintf(dStr, sizeof(dStr), "%04d-%02d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
  snprintf(mStr, sizeof(mStr), "%04d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1);
  String todayStr = String(dStr);
  String curMonth = String(mStr);

  struct { String key; String detail; } cands[12];
  int cnt = 0;
  auto add = [&](const String& k, const String& d) { if (cnt < 12) { cands[cnt].key = k; cands[cnt].detail = d; cnt++; } };

  // --- Parse tasks for D1/D2/D3 ---
  int dailyCompleted = 0, dailyUncompleted = 0, dailyTotal = 0;
  bool monthlyDueToday = false;
  String monthlyTaskName = "";

  if (LittleFS.exists("/todo.json")) {
    fs::File file = LittleFS.open("/todo.json", "r");
    if (file) {
      DynamicJsonDocument doc(4096);
      if (deserializeJson(doc, file) == DeserializationError::Ok) {
        if (doc.containsKey("daily")) {
          for (JsonObject t : doc["daily"].as<JsonArray>()) {
            bool rec = t["recurrent"] | false;
            if (rec) {
              String sd = t["startDate"] | "";
              String ed = t["endDate"] | "";
              if (!(sd.length() == 0 || todayStr >= sd)) continue;
              if (ed.length() > 0 && todayStr >= ed) continue;
            }
            dailyTotal++;
            if (rec) {
              if (t.containsKey("completedDates")) {
                for (JsonVariant d : t["completedDates"].as<JsonArray>()) {
                  if (d.as<String>() == todayStr) { dailyCompleted++; break; }
                }
              }
            } else {
              if ((t["completed"] | false)) dailyCompleted++;
            }
          }
        }
        dailyUncompleted = dailyTotal - dailyCompleted;

        if (doc.containsKey("monthly")) {
          for (JsonObject t : doc["monthly"].as<JsonArray>()) {
            bool rec = t["recurrent"] | false;
            int dueDay = t["day"] | 1;
            if (ptm->tm_mday != dueDay) continue;
            if (!rec) {
              if ((t["year"] | 0) != ptm->tm_year + 1900 || (t["month"] | 0) != ptm->tm_mon + 1) continue;
            } else {
              String sm = t["startMonth"] | "";
              String em = t["endMonth"] | "";
              if (sm.length() > 0 && curMonth < sm) continue;
              if (em.length() > 0 && curMonth >= em) continue;
            }
            bool done = false;
            if (rec && t.containsKey("completedMonths")) {
              for (JsonVariant m : t["completedMonths"].as<JsonArray>()) {
                if (m.as<String>() == curMonth) { done = true; break; }
              }
            } else if (!rec) { done = t["completed"] | false; }
            if (!done) { monthlyDueToday = true; monthlyTaskName = t["text"] | ""; }
          }
        }
      }
      file.close();
    }
  }

  // --- A3: Shift progress ---
  if (appConfig.targetHours > 0.0f) {
    unsigned long targetMs = (unsigned long)(appConfig.targetHours * 3600000.0f * 1000.0f);
    if (appStats.totalDeskTime > targetMs / 4) {
      if (appStats.totalDeskTime < targetMs) {
        unsigned long leftMs = targetMs - appStats.totalDeskTime;
        add("shift_progress", formatTime(appStats.totalDeskTime) + " in, " + formatTime(leftMs) + " left");
      }
    }
  }

  // --- B1: Marathon no-break ---
  if (appStats.breakCount == 0 && appStats.totalDeskTime > 7200000UL)
    add("marathon_nobreak", "over " + formatTime(appStats.totalDeskTime) + " without a single break");

  // --- B2: Micro-break pattern ---
  if (appStats.breakCount >= 3 && appStats.totalDeskTime > 7200000UL) {
    unsigned long avgBreak = appStats.totalBreakTime / (unsigned long)appStats.breakCount;
    if (avgBreak < 300000UL)
      add("microbreaks", String(appStats.breakCount) + " micro-breaks under 5 min each");
  }

  // --- B3: Unusual hour presence ---
  int hourPresence = getEffectivePresence(currentDay, currentHour);
  if (hourPresence > 0 && hourPresence < 15) {
    char hBuf[6]; int h12 = (currentHour % 12 == 0) ? 12 : currentHour % 12;
    snprintf(hBuf, sizeof(hBuf), "%d%sm", h12, currentHour >= 12 ? "p" : "a");
    add("unusual_hour", "usually away at " + String(hBuf) + " but at the desk today");
  }

  // --- D1: Rapid task clearance ---
  if (dailyCompleted >= 3 && currentHour < 12)
    add("rapid_tasks", "already cleared " + String(dailyCompleted) + " tasks before noon");

  // --- D2: All daily tasks done ---
  if (dailyTotal > 0 && dailyUncompleted == 0 && currentHour < getLearnedWorkdayEnd(currentDay))
    add("all_tasks_done", "all daily tasks cleared — nothing left on the list");

  // --- D3: Monthly goal due ---
  if (monthlyDueToday && dailyUncompleted == 0 && dailyTotal > 0)
    add("monthly_due", "monthly goal \"" + monthlyTaskName + "\" due today and dailies are done");

  // --- E1: Weather hot ---
  if (appState.temp > 30)
    add("weather_hot", "it's " + String(appState.temp) + "C outside — good call staying in");

  // --- E2: Weather nice ---
  if (appState.temp >= 20 && appState.temp <= 26 && strcmp(appState.weatherDesc, "Clear") == 0)
    add("weather_nice", "beautiful " + String(appState.temp) + "C outside — step out for a walk?");

  // --- Variety: avoid repeating last key ---
  if (cnt == 0) return "";
  int usable = 0;
  for (int i = 0; i < cnt; i++) if (cands[i].key != lastKey) usable++;
  int pool = (usable > 0) ? usable : cnt;
  int pick = random(pool);
  int chosen = 0;
  if (usable > 0) {
    for (int i = 0; i < cnt; i++) {
      if (cands[i].key != lastKey) { if (chosen == pick) { chosen = i; break; } chosen++; }
    }
  } else { chosen = pick; }
  lastKey = cands[chosen].key;
  return cands[chosen].detail;
}

#endif // CURATION_H
