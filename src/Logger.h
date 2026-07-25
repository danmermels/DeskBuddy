#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include "State.h"

extern NTPClient timeClient;
extern PubSubClient mqttClient;

class Logger {
public:
  static String formatEpoch(uint32_t epoch) {
    if (epoch == 0) return "00:00:00";
    if (epoch < 1000000000UL) {
      // It's uptime seconds
      uint32_t h = epoch / 3600;
      uint32_t m = (epoch % 3600) / 60;
      uint32_t s = epoch % 60;
      char buf[16];
      snprintf(buf, sizeof(buf), "UP:%02d:%02d:%02d", h, m, s);
      return String(buf);
    }
    time_t raw = (time_t)epoch;
    struct tm t;
    gmtime_r(&raw, &t);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return String(buf);
  }

  static void log(const char* category, const char* format, ...) {
    if (appState.systemLogMutex == NULL) return;

    char msgBuffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(msgBuffer, sizeof(msgBuffer), format, args);
    va_end(args);

    // Sanitize message: replace control characters (ASCII < 32) with spaces to prevent JSON parsing crashes
    for (int i = 0; msgBuffer[i] != '\0'; i++) {
      if ((unsigned char)msgBuffer[i] < 32) {
        msgBuffer[i] = ' ';
      }
    }

    // Calculate local time for serial and MQTT
    uint32_t localTimeSec = timeClient.isTimeSet() ? timeClient.getEpochTime() : (millis() / 1000UL);
    String timeStr = formatEpoch(localTimeSec);

    // Mirror to Serial
    Serial.printf("[%s] [%s] %s\n", category, timeStr.c_str(), msgBuffer);

    // Publish to MQTT if connected (Remote Logging) with prepended local timestamp
    if (mqttClient.connected()) {
      char topicBuffer[64];
      snprintf(topicBuffer, sizeof(topicBuffer), "deskbuddy/log/%s", category);
      char fullMsg[200];
      snprintf(fullMsg, sizeof(fullMsg), "[%s] %s", timeStr.c_str(), msgBuffer);
      mqttClient.publish(topicBuffer, fullMsg);
    }

    // Calculate UTC timestamp for web logs console storage (web console converts UTC epoch to browser local time)
    uint32_t tsValue = 0;
    if (timeClient.isTimeSet()) {
      tsValue = timeClient.getEpochTime() - NTP_TIME_OFFSET;
    } else {
      tsValue = millis() / 1000UL;
    }

    if (xSemaphoreTake(appState.systemLogMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      int head = appState.systemLogHead;
      appState.systemLog[head].timestamp = tsValue;
      
      // Clean target strings
      memset(appState.systemLog[head].category, 0, sizeof(appState.systemLog[head].category));
      memset(appState.systemLog[head].message, 0, sizeof(appState.systemLog[head].message));
      
      strncpy(appState.systemLog[head].category, category, sizeof(appState.systemLog[head].category) - 1);
      strncpy(appState.systemLog[head].message, msgBuffer, sizeof(appState.systemLog[head].message) - 1);

      appState.systemLogHead = (head + 1) % SYSTEM_LOG_SIZE;
      if (appState.systemLogCount < SYSTEM_LOG_SIZE) {
        appState.systemLogCount++;
      }
      xSemaphoreGive(appState.systemLogMutex);
    }
  }

  // Periodically flushes new log entries to Flash filesystem in a single blocking burst (reduces flash cache halts)
  static void flushToFlash() {
    if (appState.systemLogMutex == NULL) return;

    static int lastSavedIdx = 0;
    if (xSemaphoreTake(appState.systemLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      int idx = appState.systemLogHead;
      int count = appState.systemLogCount;

      if (idx != lastSavedIdx && count > 0) {
        fs::File file = LittleFS.open("/system.log", "a");
        if (file) {
          int cur = lastSavedIdx;
          // Loop through the circular slots that haven't been written to flash yet
          while (cur != idx) {
            file.printf("%u [%s] %s\n", 
                        appState.systemLog[cur].timestamp, 
                        appState.systemLog[cur].category, 
                        appState.systemLog[cur].message);
            cur = (cur + 1) % SYSTEM_LOG_SIZE;
          }
          file.close();
          lastSavedIdx = idx;
        }

        // Check size and perform rotation check only once per batch
        fs::File check = LittleFS.open("/system.log", "r");
        if (check) {
          size_t size = check.size();
          check.close();

          if (size > 8192) {
            if (LittleFS.exists("/system.log.old")) {
              LittleFS.remove("/system.log.old");
            }
            LittleFS.rename("/system.log", "/system.log.old");
          }
        }
      }
      xSemaphoreGive(appState.systemLogMutex);
    }
  }
};

#endif // LOGGER_H
