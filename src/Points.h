#ifndef POINTS_H
#define POINTS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "State.h"

#define POINTS_FLOOR -100
#define TODO_PATH "/todo.json"

extern int dateToDays(String dateStr);

inline String pointsMonthOf(String date) {
  return (date.length() >= 7) ? date.substring(0, 7) : "";
}

inline bool pointsHasDate(JsonObject task, const String& date) {
  if (!task.containsKey("completedDates")) return false;
  for (JsonVariant v : task["completedDates"].as<JsonArray>()) {
    if (v.as<String>() == date) return true;
  }
  return false;
}

inline bool pointsHasMonth(JsonObject task, const String& month) {
  if (!task.containsKey("completedMonths")) return false;
  for (JsonVariant v : task["completedMonths"].as<JsonArray>()) {
    if (v.as<String>() == month) return true;
  }
  return false;
}

inline int pointsBase(JsonObject task, const String& type) {
  bool rec = task["recurrent"] | false;
  if (type == "daily") return rec ? 2 : 1;
  return rec ? 10 : 5;
}

inline long pointsAdd(long running, long delta) {
  long v = running + delta;
  if (v < POINTS_FLOOR) v = POINTS_FLOOR;
  return v;
}

inline String pointsCategory(long running, int poorMax, int excellentMin) {
  if (running <= poorMax) return "poor";
  if (running >= excellentMin) return "excellent";
  return "good";
}

// Compact current-month points snapshot used by the seated points check-in
// trigger, e.g. "12 points this month (good)". Safe to call anywhere.
inline String buildPointsDetail() {
  long running = 0;
  String curMonth = "";
  if (LittleFS.exists("/todo.json")) {
    fs::File file = LittleFS.open("/todo.json", "r");
    if (file) {
      DynamicJsonDocument doc(8192);
      if (deserializeJson(doc, file) == DeserializationError::Ok && doc.containsKey("points")) {
        JsonObject p = doc["points"];
        running = p["running"] | 0L;
        curMonth = p["currentMonth"] | "";
      }
      file.close();
    }
  }
  if (curMonth.length() != 7 && timeClient.isTimeSet()) {
    time_t e = timeClient.getEpochTime();
    struct tm* ptm = localtime(&e);
    if (ptm != nullptr) {
      char b[8];
      snprintf(b, sizeof(b), "%04d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1);
      curMonth = String(b);
    }
  }
  String category = pointsCategory(running, appConfig.pointsPoorMax, appConfig.pointsExcellentMin);
  String detail = String(running) + " points";
  if (curMonth.length() == 7) detail += " this month";
  detail += " (" + category + ")";
  return detail;
}

// Stable identity for diff matching. endDate/endMonth are treated as state, not identity.
inline String pointsTaskKey(JsonObject task, const String& type) {
  String s = type + "|" + String((task["text"] | "")) + "|" + String((task["hour"] | 0)) + ":" + String((task["minute"] | 0)) + "|day=" + String((task["day"] | 0));
  if (task["recurrent"] | false) {
    if (type == "daily") {
      s += "|rec|start=" + String((task["startDate"] | ""));
    } else {
      s += "|rec|start=" + String((task["startMonth"] | ""));
    }
  } else if (type == "daily") {
    s += "|" + String((task["targetDate"] | ""));
  } else {
    s += "|" + String((task["year"] | 0)) + "-" + String((task["month"] | 0));
  }
  return s;
}

// Inverse of dateToDays(): "YYYY-MM-DD".
inline String pointsDateToStr(int days) {
  if (days < 1) return "";
  int y = 2000;
  long rem = days;
  while (true) {
    int ld = 365 + ((y % 4 == 0) ? 1 : 0);
    if (rem < ld) break;
    rem -= ld;
    y++;
  }
  int m = 1;
  const int monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (m = 1; m <= 12; m++) {
    int md = monthDays[m - 1];
    if (m == 2 && y % 4 == 0) md = 29;
    if (rem <= md) break;
    rem -= md;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, (int)rem);
  return String(buf);
}

inline void pointsEnsure(JsonObject root, const String& curMonth) {
  if (!root.containsKey("points")) {
    JsonObject p = root.createNestedObject("points");
    p["currentMonth"] = curMonth;
    p["running"] = 0L;
    p.createNestedObject("months");
  } else {
    JsonObject p = root["points"];
    if (!p.containsKey("currentMonth")) p["currentMonth"] = curMonth;
    if (!p.containsKey("running")) p["running"] = 0L;
    if (!p.containsKey("months")) p.createNestedObject("months");
  }
}

// Bakes the running total into the months archive at month rollover.
inline bool pointsBake(JsonObject root, const String& curMonth) {
  if (!root.containsKey("points")) return false;
  JsonObject p = root["points"];
  String cur = p["currentMonth"] | "";
  if (cur.length() != 7 || cur == curMonth) return false;
  JsonObject months = p["months"];
  if (months.isNull()) months = p.createNestedObject("months");
  months[cur] = p["running"] | 0L;
  p["currentMonth"] = curMonth;
  p["running"] = 0L;
  if (root.containsKey("daily")) {
    for (JsonObject task : root["daily"].as<JsonArray>()) task.remove("lastOverdueProcessed");
  }
  if (root.containsKey("monthly")) {
    for (JsonObject task : root["monthly"].as<JsonArray>()) task.remove("lastOverdueProcessed");
  }
  return true;
}

// Compares a fresh client payload against the persisted document and returns the
// net point delta for the current month. Mutates newRoot by carrying markers.
inline int pointsApplyDeltas(JsonObject oldRoot, JsonObject newRoot, const String& curMonth) {
  int delta = 0;
  const char* types[2] = { "daily", "monthly" };
  for (int t = 0; t < 2; t++) {
    String type = types[t];
    JsonArray oldArr = oldRoot.containsKey(type) ? oldRoot[type].as<JsonArray>() : JsonArray();
    JsonArray newArr = newRoot.containsKey(type) ? newRoot[type].as<JsonArray>() : JsonArray();
    int oldCount = oldArr.size();
    int newCount = newArr.size();
    bool* used = new bool[oldCount > 0 ? oldCount : 1];
    for (int i = 0; i < oldCount; i++) used[i] = false;

    for (int n = 0; n < newCount; n++) {
      JsonObject nt = newArr[n];
      String nk = pointsTaskKey(nt, type);
      int matchIdx = -1;
      for (int o = 0; o < oldCount; o++) {
        if (!used[o] && pointsTaskKey(oldArr[o], type) == nk) {
          matchIdx = o;
          break;
        }
      }
      if (matchIdx < 0) {
        delta += pointsBase(nt, type);
        continue;
      }
      used[matchIdx] = true;
      JsonObject ot = oldArr[matchIdx];
      int base = pointsBase(nt, type);

      if (!nt.containsKey("lastOverdueProcessed") && ot.containsKey("lastOverdueProcessed")) {
        nt["lastOverdueProcessed"] = ot["lastOverdueProcessed"].as<const char*>();
      }

      if (type == "daily") {
        String oe = ot["endDate"] | "";
        String ne = nt["endDate"] | "";
        if ((oe.length() == 0 && ne.length() > 0) || (oe.length() > 0 && ne != oe)) delta -= base;
      } else {
        String oe = ot["endMonth"] | "";
        String ne = nt["endMonth"] | "";
        if ((oe.length() == 0 && ne.length() > 0) || (oe.length() > 0 && ne != oe)) delta -= base;
      }

      bool rec = nt["recurrent"] | false;
      if (!rec) {
        bool oldDone = ot["completed"] | false;
        bool newDone = nt["completed"] | false;
        String instMonth = "";
        if (type == "daily") {
          instMonth = pointsMonthOf(nt["targetDate"] | "");
        } else {
          int ty = nt["year"] | 0;
          int tm = nt["month"] | 0;
          if (ty > 0 && tm >= 1 && tm <= 12) {
            char b[8];
            snprintf(b, sizeof(b), "%04d-%02d", ty, tm);
            instMonth = String(b);
          }
        }
        bool inCur = (instMonth.length() == 7 && instMonth == curMonth);
        if (!oldDone && newDone && inCur) delta += base;
        else if (oldDone && !newDone && inCur) delta -= base;
      } else if (type == "daily") {
        if (nt.containsKey("completedDates")) {
          for (JsonVariant v : nt["completedDates"].as<JsonArray>()) {
            String d = v.as<String>();
            if (!pointsHasDate(ot, d) && pointsMonthOf(d) == curMonth) delta += base;
          }
        }
        if (ot.containsKey("completedDates")) {
          for (JsonVariant v : ot["completedDates"].as<JsonArray>()) {
            String d = v.as<String>();
            if (!pointsHasDate(nt, d) && pointsMonthOf(d) == curMonth) delta -= base;
          }
        }
      } else {
        if (nt.containsKey("completedMonths")) {
          for (JsonVariant v : nt["completedMonths"].as<JsonArray>()) {
            String d = v.as<String>();
            if (!pointsHasMonth(ot, d) && d == curMonth) delta += base;
          }
        }
        if (ot.containsKey("completedMonths")) {
          for (JsonVariant v : ot["completedMonths"].as<JsonArray>()) {
            String d = v.as<String>();
            if (!pointsHasMonth(nt, d) && d == curMonth) delta -= base;
          }
        }
      }
    }

    for (int o = 0; o < oldCount; o++) {
      if (!used[o]) delta -= pointsBase(oldArr[o], type);
    }
    delete[] used;
  }
  return delta;
}

// Live per-period accrual for overdue tasks. Returns true if anything changed.
inline bool pointsApplyOverdue(JsonObject root, int nowMins, const String& currentDayString, const String& curMonth) {
  if (curMonth.length() != 7 || currentDayString.length() != 10) return false;
  pointsEnsure(root, curMonth);
  bool changed = false;

  if (root.containsKey("daily")) {
    for (JsonObject task : root["daily"].as<JsonArray>()) {
      int base = pointsBase(task, "daily");
      bool rec = task["recurrent"] | false;
      String marker = task["lastOverdueProcessed"] | "";
      if (marker == currentDayString) continue;
      int tHour = task["hour"] | 12;
      int tMin = task["minute"] | 0;
      if (nowMins < tHour * 60 + tMin) continue;

      int fromDays = 0;
      int toDays = dateToDays(currentDayString);

      if (!rec) {
        String target = task["targetDate"] | "";
        if (target.length() != 10 || pointsMonthOf(target) != curMonth) continue;
        if (task["completed"] | false) continue;
        if (currentDayString < target) continue;
        fromDays = dateToDays(target);
        if (marker.length() == 10) {
          int m = dateToDays(marker) + 1;
          if (m > fromDays) fromDays = m;
        }
      } else {
        String start = task["startDate"] | "";
        String end = task["endDate"] | "";
        if (start.length() > 0 && currentDayString < start) continue;
        if (end.length() > 0 && currentDayString >= end) continue;
        if (pointsHasDate(task, currentDayString)) continue;
        fromDays = dateToDays(start.length() == 10 ? start : curMonth + "-01");
        if (marker.length() == 10) {
          int m = dateToDays(marker) + 1;
          if (m > fromDays) fromDays = m;
        }
        int firstOfMonth = dateToDays(curMonth + "-01");
        if (firstOfMonth > fromDays) fromDays = firstOfMonth;
        if (end.length() == 10) {
          int ed = dateToDays(end) - 1;
          if (ed < toDays) toDays = ed;
        }
      }

      int cnt = 0;
      for (int d = fromDays; d <= toDays; d++) {
        String ds = pointsDateToStr(d);
        if (ds.length() != 10) continue;
        if (pointsMonthOf(ds) != curMonth) continue;
        if (pointsHasDate(task, ds)) continue;
        cnt++;
      }
      if (cnt > 0) {
        root["points"]["running"] = pointsAdd(root["points"]["running"] | 0L, -(long)base * cnt);
        task["lastOverdueProcessed"] = currentDayString;
        changed = true;
      }
    }
  }

  if (root.containsKey("monthly")) {
    for (JsonObject task : root["monthly"].as<JsonArray>()) {
      int base = pointsBase(task, "monthly");
      bool rec = task["recurrent"] | false;
      String marker = task["lastOverdueProcessed"] | "";
      if (marker == curMonth) continue;
      if (!rec) {
        int ty = task["year"] | 0;
        int tm = task["month"] | 0;
        if (ty <= 0 || tm < 1 || tm > 12) continue;
        char b[8];
        snprintf(b, sizeof(b), "%04d-%02d", ty, tm);
        if (String(b) != curMonth) continue;
        if (task["completed"] | false) continue;
      } else {
        String start = task["startMonth"] | "";
        String end = task["endMonth"] | "";
        if (start.length() == 7 && curMonth < start) continue;
        if (end.length() == 7 && curMonth >= end) continue;
        if (pointsHasMonth(task, curMonth)) continue;
      }
      int dueDay = task["day"] | 1;
      if (dueDay < 1) dueDay = 1;
      int todayDay = currentDayString.substring(8, 10).toInt();
      if (todayDay < dueDay) continue;
      root["points"]["running"] = pointsAdd(root["points"]["running"] | 0L, -(long)base);
      task["lastOverdueProcessed"] = curMonth;
      changed = true;
    }
  }

  return changed;
}

inline bool pointsSaveDoc(DynamicJsonDocument& doc) {
  String tmp = String(TODO_PATH) + ".tmp";
  {
    fs::File f = LittleFS.open(tmp, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
  }
  if (LittleFS.rename(tmp, TODO_PATH)) return true;
  if (LittleFS.exists(TODO_PATH)) LittleFS.remove(TODO_PATH);
  if (LittleFS.rename(tmp, TODO_PATH)) return true;
  if (LittleFS.exists(tmp)) LittleFS.remove(tmp);
  return false;
}

#endif // POINTS_H
