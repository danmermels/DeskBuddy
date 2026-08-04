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
            char timeBuf[10];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tHour, tMin);
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
          if (diff > 0) {
            overdue = true;
            expired = diff * 1440L;
          } else if (diff == 0) {
            // Due today: overdue once the due time has passed.
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
          if (currentDay > dueDay || missedMonths > 0) {
            overdue = true;
            expired = (long)(currentDay - dueDay) * 1440L + (long)missedMonths * 30L * 1440L;
          }
        } else {
          int monthDiff = (currentYear - tYear) * 12 + (currentMonth - tMonth);
          if (monthDiff > 0) {
            overdue = true;
            expired = monthDiff * 30 * 1440L;
          } else if (monthDiff == 0 && currentDay > dueDay) {
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
  int currentMonth = 7;
  int currentDay = 18;
  String currentMonthString = "2026-07";
  String currentDayString = "2026-07-18";
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
  int overdueMonthlies = 0;
  int dueTodayMonthlies = 0;
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
          
          char timeBuf[6];
          snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tHour, tMin);
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
      } else {
        tMonth = task["month"] | 1;
        tYear = task["year"] | 2026;
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

          char dateBuf[6];
          snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", dueDay, currentMonth);
          String dateStr = String(dateBuf);

          if (currentDay == dueDay) {
            dueTodayMonthlies++;
            monthlyListItems.push_back("[GREEN]" + dateStr + " | " + truncateTaskText(taskText, 30));
            if (missedMonth.length() > 0) {
              overdueMonthlies++;
              char mbuf[6];
              snprintf(mbuf, sizeof(mbuf), "%02d/%02d", dueDay, missedMonth.substring(5, 7).toInt());
              monthlyListItems.push_back("[RED]" + String(mbuf) + " | " + truncateTaskText(taskText, 30));
            }
          } else if (currentDay > dueDay) {
            overdueMonthlies++;
            monthlyListItems.push_back("[RED]" + dateStr + " | " + truncateTaskText(taskText, 30));
            if (missedMonth.length() > 0) {
              overdueMonthlies++;
              char mbuf[6];
              snprintf(mbuf, sizeof(mbuf), "%02d/%02d", dueDay, missedMonth.substring(5, 7).toInt());
              monthlyListItems.push_back("[RED]" + String(mbuf) + " | " + truncateTaskText(taskText, 30));
            }
          } else {
            if (missedMonth.length() > 0) {
              overdueMonthlies++;
              char mbuf[6];
              snprintf(mbuf, sizeof(mbuf), "%02d/%02d", dueDay, missedMonth.substring(5, 7).toInt());
              monthlyListItems.push_back("[RED]" + String(mbuf) + " | " + truncateTaskText(taskText, 30));
            }
            monthlyListItems.push_back("[WHITE]" + dateStr + " | " + truncateTaskText(taskText, 30));
          }
        } else {
          if (!isRecurrent && (tYear < currentYear || (tYear == currentYear && tMonth < currentMonth))) {
            overdueMonthlies++;
            
            char dateBuf[6];
            snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", dueDay, tMonth);
            String dateStr = String(dateBuf);
            monthlyListItems.push_back("[RED]" + dateStr + " | " + truncateTaskText(taskText, 30));
          }
        }
      }
    }
  }

  // Page 1 generation - Clean Center-Justified Dashboard
  updateTodoTally(dailyDone, dailyActive, monthlyDone, monthlyActive, currentDayString, currentMonthString);
  saveDailyStats();

  String p1 = "[YELLOW]TODO\n";
  p1 += "[BLUE]-- DAILY --\n";
  p1 += "[WHITE]Open: " + String(dailyUncompleted) + "\n";
  p1 += "[GREEN]Due Today: " + String(dueTodayDailies) + "\n";
  p1 += "[RED]Overdue: " + String(overdueDailies) + "\n\n";
  p1 += "[BLUE]-- MONTHLY --\n";
  p1 += "[WHITE]Open: " + String(monthlyUncompleted) + "\n";
  p1 += "[GREEN]Due Today: " + String(dueTodayMonthlies) + "\n";
  p1 += "[RED]Overdue: " + String(overdueMonthlies) + "\n";

  int dailyNet = 2 * dailyDone - dailyActive;
  int monthlyNet = 2 * monthlyDone - monthlyActive;
  String dailyCol = (dailyNet >= 0) ? "[GREEN]" : "[RED]";
  String monthlyCol = (monthlyNet >= 0) ? "[GREEN]" : "[RED]";

  p1 += dailyCol + "Daily " + diligenceSigned(dailyNet) + " | " + monthlyCol + "Monthly " + diligenceSigned(monthlyNet);
  pages.push_back(p1);
  pageLines.push_back(11);

  // Group task list lines
  std::vector<String> pageLinesList;
  if (dailyListItems.size() > 0) {
    pageLinesList.push_back("[YELLOW]-- DAILY --");
    for (const auto& item : dailyListItems) {
      pageLinesList.push_back(item);
    }
  }
  if (monthlyListItems.size() > 0) {
    pageLinesList.push_back("[YELLOW]-- MONTHLY --");
    for (const auto& item : monthlyListItems) {
      pageLinesList.push_back(item);
    }
  }

  // Chunk pages
  if (pageLinesList.size() > 0) {
    int currentPageIndex = 2;
    String currentPageContent = "";
    int currentLineCount = 0;

    for (size_t i = 0; i < pageLinesList.size(); i++) {
      if (currentLineCount == 0) {
        currentPageContent = "[YELLOW]TASK LIST (" + String(currentPageIndex - 1) + ")\n";
        currentLineCount = 1;
      }

      // Orphan Header Protection
      if (pageLinesList[i].startsWith("[YELLOW]--") && currentLineCount == 11) {
        if (currentPageContent.length() > 0 && currentPageContent[currentPageContent.length() - 1] == '\n') {
          currentPageContent.remove(currentPageContent.length() - 1);
        }
        pages.push_back(currentPageContent);
        pageLines.push_back(currentLineCount);

        currentPageIndex++;
        currentPageContent = "[YELLOW]TASK LIST (" + String(currentPageIndex - 1) + ")\n";
        currentLineCount = 1;
      }

      currentPageContent += pageLinesList[i] + "\n";
      currentLineCount++;

      if (currentLineCount == 12 || i == pageLinesList.size() - 1) {
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

#endif // CURATION_H
