#ifndef CURATION_H
#define CURATION_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
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
          synthBullets.push_back("- Daily task: " + taskText + " (due " + String(timeBuf) + ")");
        }
      }

      // Check if highly overdue (overdue > 3 days)
      if (!isCompleted && tDate.length() == 10) {
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
            synthBullets.push_back("- Monthly task: " + taskText + " (DUE TODAY: Day " + String(dueDay) + ")");
          } else if (currentDay > dueDay) {
            monthlyOverdue++;
            synthBullets.push_back("- Monthly task: " + taskText + " (OVERDUE: was due Day " + String(dueDay) + ")");
          } else {
            synthBullets.push_back("- Monthly task: " + taskText + " (due Day " + String(dueDay) + ")");
          }
        }
      }

      // Check if highly overdue (overdue > 3 months)
      if (!isCompleted) {
        int diffMonths = (currentYear - tYear) * 12 + (currentMonth - tMonth);
        if (diffMonths > TASK_OVERDUE_MONTHS_LIMIT) {
          monthlyOverdueMoreThan3Months++;
          synthBullets.push_back("- Monthly task: " + taskText + " (overdue by " + String(diffMonths) + " months!)");
        }
      }
    }
  }

  // Compact task synthesis (counts + names) injected into AI prompt observations.
  // Covers everything the AI needs to talk about tasks: pending today, due today,
  // and overdue (daily >3d, monthly this-month and >3mo), by name.
  String synthesis = "[TASK SYNTHESIS]\n";
  if (dailyTotal == 0 && monthlyTotal == 0) {
    synthesis += "No tasks on the list.\n";
  } else if (dailyUncompleted == 0 && monthlyUncompleted == 0 &&
             dailyOverdueMoreThan3Days == 0 && monthlyOverdue == 0 && monthlyOverdueMoreThan3Months == 0) {
    synthesis += "All tasks for today completed.\n";
  } else {
    synthesis += String(dailyUncompleted) + "/" + String(dailyTotal) + " daily pending today, " +
                 String(monthlyDueToday) + " monthly due today; overdue: " +
                 String(dailyOverdueMoreThan3Days) + " daily (3d+), " +
                 String(monthlyOverdue) + " monthly this month, " +
                 String(monthlyOverdueMoreThan3Months) + " monthly (3mo+).\n";
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
    obs += "Highly Overdue Tasks Alert!\n";
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

      if (!isCompleted) {
        monthlyUncompleted++;
        if (isActiveThisMonth) {
          char dateBuf[6];
          snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", dueDay, currentMonth);
          String dateStr = String(dateBuf);

          if (currentDay == dueDay) {
            dueTodayMonthlies++;
            monthlyListItems.push_back("[GREEN]" + dateStr + " | " + truncateTaskText(taskText, 30));
          } else if (currentDay > dueDay) {
            overdueMonthlies++;
            monthlyListItems.push_back("[RED]" + dateStr + " | " + truncateTaskText(taskText, 30));
          } else {
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
  String p1 = "[YELLOW]TODO\n\n";
  p1 += "[BLUE]-- DAILY --\n";
  p1 += "[WHITE]Open: " + String(dailyUncompleted) + "\n";
  p1 += "[GREEN]Due Today: " + String(dueTodayDailies) + "\n";
  p1 += "[RED]Overdue: " + String(overdueDailies) + "\n\n";
  p1 += "[BLUE]-- MONTHLY --\n";
  p1 += "[WHITE]Open: " + String(monthlyUncompleted) + "\n";
  p1 += "[GREEN]Due Today: " + String(dueTodayMonthlies) + "\n";
  p1 += "[RED]Overdue: " + String(overdueMonthlies);
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
