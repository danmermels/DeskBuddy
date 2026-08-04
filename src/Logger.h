#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <PubSubClient.h>

extern NTPClient timeClient;
extern PubSubClient mqttClient;

class Logger {
public:
  static String formatEpoch(uint32_t epoch) {
    if (epoch == 0) return "00:00:00";
    if (epoch < 1000000000UL) {
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
    char msgBuffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(msgBuffer, sizeof(msgBuffer), format, args);
    va_end(args);

    for (int i = 0; msgBuffer[i] != '\0'; i++) {
      if ((unsigned char)msgBuffer[i] < 32) {
        msgBuffer[i] = ' ';
      }
    }

    uint32_t localTimeSec = timeClient.isTimeSet() ? timeClient.getEpochTime() : (millis() / 1000UL);
    String timeStr = formatEpoch(localTimeSec);

    Serial.printf("[%s] [%s] %s\n", category, timeStr.c_str(), msgBuffer);

    extern void enqueueMqttPublish(const String& topic, const String& payload);
    char topicBuffer[64];
    snprintf(topicBuffer, sizeof(topicBuffer), "deskbuddy/log/%s", category);
    char fullMsg[200];
    snprintf(fullMsg, sizeof(fullMsg), "[%s] %s", timeStr.c_str(), msgBuffer);
    enqueueMqttPublish(topicBuffer, fullMsg);
  }

};

#endif // LOGGER_H
