#ifndef NETWORKTASK_H
#define NETWORKTASK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "State.h"
#include "Constants.h"
#include "Logger.h"
#include "Telemetry.h"

extern NTPClient timeClient;

inline IPAddress netParseIP(const String& s) {
  IPAddress ip;
  ip.fromString(s);
  return ip;
}

// Recycle the WiFi link. The ESP32-C3 can hold WL_CONNECTED with a dead path
// (lost beacons not detected, wrong subnet, stale association), so after a few
// consecutive DNS sync failures we force a full re-association with the static
// config re-applied (same sequence as checkWiFiConnection()).
inline void recycleWifiLink() {
  String ssid, pass;
  bool staticEn;
  String ipS, gwS, snS, d1S, d2S;
  xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
  ssid = appConfig.wifiSsid;
  pass = appConfig.wifiPass;
  staticEn = appConfig.wifiStaticEnabled;
  ipS = appConfig.wifiIp;
  gwS = appConfig.wifiGw;
  snS = appConfig.wifiSubnet;
  d1S = appConfig.wifiDns1;
  d2S = appConfig.wifiDns2;
  xSemaphoreGive(appState.aiMutex);

  if (ssid.length() == 0) return;

  WiFi.disconnect();
  delay(200);
  if (staticEn) {
    WiFi.config(netParseIP(ipS), netParseIP(gwS), netParseIP(snS), netParseIP(d1S), netParseIP(d2S));
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  WiFi.setSleep(false); // keep radio awake for low-latency TCP (HTTP/MQTT)
  Logger::log("NET", "link recycle: ssid=%s ip=%s", ssid.c_str(), netParseIP(ipS).toString().c_str());
}

// One-minute WiFi link + DNS state diagnostic (words "error"/"fail" avoided
// so the control_center observer does not count them as AI errors).
inline void logWifiState() {
  String ssid;
  xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
  ssid = appConfig.wifiSsid;
  xSemaphoreGive(appState.aiMutex);
  Logger::log("NET", "wifi=%s ssid=%s ip=%s gw=%s dns0=%s dns1=%s rssi=%d",
              (WiFi.status() == WL_CONNECTED) ? "up" : "down",
              ssid.c_str(),
              WiFi.localIP().toString().c_str(),
              WiFi.gatewayIP().toString().c_str(),
              WiFi.dnsIP(0).toString().c_str(),
              WiFi.dnsIP(1).toString().c_str(),
              WiFi.RSSI());
}

// Weather fetch (HTTPS). Runs on the network task only, never on the main loop.
// Skips while an AI query's TLS buffers are in flight (dual-TLS guard) and
// caps the HTTP timeout so a slow endpoint cannot stall the device.
inline bool fetchWeatherTask() {
  if (appState.isAILoading) return false;

  // Snapshot config into locals (config Strings are written by web handlers).
  String apiKey;
  float lat = appConfig.openWeatherLat;
  float lon = appConfig.openWeatherLon;
  xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
  apiKey = appConfig.openWeatherKey;
  lat = appConfig.openWeatherLat;
  lon = appConfig.openWeatherLon;
  xSemaphoreGive(appState.aiMutex);

  if (apiKey.length() == 0) return false;

  HTTPClient http;
  http.setTimeout(5000);
  String weatherUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" +
                      String(lat, 4) + "&units=metric&lon=" +
                      String(lon, 4) + "&lang=fr&appid=" + apiKey;
  http.begin(weatherUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument jsonBuffer(1024);
    DeserializationError error = deserializeJson(jsonBuffer, payload);
    if (!error) {
      int temp = (int)(jsonBuffer["main"]["temp"] | 0.0f);
      const char* desc = jsonBuffer["weather"][0]["main"] | "Clear";
      xSemaphoreTake(appState.aiMutex, portMAX_DELAY);
      appState.temp = temp;
      strncpy(appState.weatherDesc, desc, sizeof(appState.weatherDesc) - 1);
      appState.weatherDesc[sizeof(appState.weatherDesc) - 1] = '\0';
      xSemaphoreGive(appState.aiMutex);
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

// Background task owning all blocking network I/O: NTP sync, weather fetch,
// telemetry send, firmware check + download. The main loop stays responsive
// (display, web server, MQTT, radar) even when DNS is down or an HTTPS call
// takes tens of seconds.
inline void networkTask(void* param) {
  unsigned long lastNtpAttempt = 0;
  unsigned long lastWeatherAttempt = 0;
  unsigned long lastTelemetrySend = 0;
  unsigned long lastFirmwareCheck = 0;
  unsigned long lastNetLog = 0;
  bool firstTelemetry = true;
  bool hwmLogged = false;
  int dnsFailStreak = 0;
  unsigned long ntpIntervalMs = NTP_RETRY_MS;

  for (;;) {
    unsigned long now = millis();
    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    // 1. NTP sync (was blocking the main loop). Backs off when unsynced so a
    //    dead DNS does not cause a perpetual retry storm. After repeated sync
    //    failures the WiFi link itself is recycled (zombie-link recovery).
    if (wifiOk && now - lastNtpAttempt > ntpIntervalMs) {
      lastNtpAttempt = now;
      timeClient.update();
      if (timeClient.isTimeSet()) {
        dnsFailStreak = 0;
        ntpIntervalMs = NTP_INTERVAL_MS;
        lastWeatherAttempt = now; // DNS works; weather may run this cycle
      } else {
        dnsFailStreak++;
        if (dnsFailStreak >= 4) {
          dnsFailStreak = 0;
          recycleWifiLink();
          ntpIntervalMs = NTP_RETRY_MS;
        } else {
          if (ntpIntervalMs < NTP_RETRY_MS * 4UL) ntpIntervalMs *= 2;
        }
      }
    }

    // Link + DNS state diagnostic (once per minute).
    if (now - lastNetLog >= 60000UL) {
      lastNetLog = now;
      logWifiState();
    }

    // 2. Weather (hourly, only once NTP synced, never concurrent with AI TLS,
    //    suppressed in emergency low-heap mode).
    if (wifiOk && !lowHeapMode && timeClient.isTimeSet() && now - lastWeatherAttempt > NTP_INTERVAL_MS) {
      lastWeatherAttempt = now;
      if (!appState.isAILoading) {
        fetchWeatherTask();
      }
    }

    // 3. Telemetry (guarded against AI TLS overlap, suppressed in low-heap mode).
    if (wifiOk && !lowHeapMode && timeClient.isTimeSet()) {
      unsigned long sendInterval = firstTelemetry ? 30000UL : TELEMETRY_SEND_INTERVAL_MS;
      if (now - lastTelemetrySend > sendInterval) {
        if (!appState.isAILoading && !appState.otaInProgress) {
          String response = sendTelemetry();
          lastTelemetrySend = now;
          firstTelemetry = false;
          if (response.length() > 0) {
            DynamicJsonDocument doc(256);
            if (deserializeJson(doc, response) == DeserializationError::Ok) {
              if (doc["update_available"].as<bool>()) {
                JsonObject fw = doc["firmware"].as<JsonObject>();
                if (!fw.isNull()) {
                  pendingFirmwareUrl = fw["url"].as<String>();
                  pendingFirmwareSize = fw["size"].as<size_t>();
                  firmwareUpdatePending = true;
                }
              }
            }
          }
        } else {
          lastTelemetrySend = now - sendInterval + 30000UL; // retry in 30s
        }
      }
    }

    // 4. Firmware check + download (guarded, suppressed in low-heap mode).
    if (wifiOk && !lowHeapMode && timeClient.isTimeSet() && now - lastFirmwareCheck > TELEMETRY_FW_CHECK_INTERVAL_MS) {
      lastFirmwareCheck = now;
      if (!appState.isAILoading && !appState.otaInProgress) {
        checkForFirmwareUpdate();
      } else {
        lastFirmwareCheck = now - TELEMETRY_FW_CHECK_INTERVAL_MS + 30000UL;
      }
    }
    if (firmwareUpdatePending && wifiOk && !lowHeapMode && !appState.isAILoading && !appState.otaInProgress) {
      downloadAndApplyFirmware();
    }

    // One-shot stack high-water mark for later stack sizing.
    if (!hwmLogged && now > 60000UL) {
      hwmLogged = true;
      Logger::log("HWM", "networkTask stack high-water=%u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

inline void startNetworkTask() {
  static TaskHandle_t handle = NULL;
  if (handle != NULL) return;
  xTaskCreate(networkTask, "NetTask", 8192, NULL, 1, &handle);
}

#endif // NETWORKTASK_H