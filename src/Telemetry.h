#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include "State.h"
#include "Constants.h"
#include "Logger.h"

extern NTPClient timeClient;

static bool firmwareUpdatePending = false;
static String pendingFirmwareUrl = "";
static size_t pendingFirmwareSize = 0;

static String generateChipId() {
  String macStr = WiFi.macAddress();
  uint8_t mac[6];
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  uint8_t hash[32];
  mbedtls_sha256(mac, 6, hash, 0);
  char chipId[9];
  snprintf(chipId, sizeof(chipId), "%02x%02x%02x%02x", hash[0], hash[1], hash[2], hash[3]);
  return String(chipId);
}

static int countCsvEntries(const String &csv) {
  if (csv.length() == 0) return 0;
  int count = 1;
  for (size_t i = 0; i < csv.length(); i++) {
    if (csv.charAt(i) == ',') count++;
  }
  return count;
}

static void buildTelemetryPayload(JsonDocument &doc) {
  String chipId = generateChipId();

  doc["chip_id"] = chipId;
  doc["fw_ver"] = DESKBUDDY_VERSION;
  doc["hw_rev"] = "esp32c3";
  doc["ts"] = timeClient.getEpochTime();
  doc["uptime_h"] = roundf((float)millis() / 3600000.0f * 10.0f) / 10.0f;
  doc["boot_count"] = appStats.bootCount;
  doc["clock_face"] = appConfig.clockFace;
  doc["ai_mode"] = appConfig.aiMode;
  doc["ai_persona"] = appConfig.aiPersona;
  doc["temp_unit"] = appConfig.tempUnitF ? 1 : 0;
  doc["time_24h"] = appConfig.time24h ? 1 : 0;
  doc["font_idx"] = appConfig.buddyFontIndex;
  doc["daily_desk_h"] = roundf(appStats.totalDeskTime / 3600000.0f * 10.0f) / 10.0f;
  doc["daily_focus_h"] = roundf(appStats.totalFocusTime / 3600000.0f * 10.0f) / 10.0f;
  doc["daily_breaks"] = appStats.breakCount;
  doc["prod_score"] = appStats.productivityScore;
  doc["daily_ai_requests"] = appStats.dailyAiRequestCount;
  doc["heap_free_kb"] = (int)(ESP.getFreeHeap() / 1024);

  doc["daily_task_active"] = appStats.dailyTaskTotal;
  doc["daily_task_done"] = appStats.dailyTaskDone;
  doc["daily_task_overdue"] = countCsvEntries(appStats.dueFiredKeys);
  doc["monthly_task_active"] = appStats.monthlyTaskTotal;
  doc["monthly_task_done"] = appStats.monthlyTaskDone;
  doc["monthly_task_overdue"] = countCsvEntries(appStats.dueFiredMonthKeys);
}

static String sendTelemetry() {
  if (!appConfig.telemetryEnabled) return "";
  if (WiFi.status() != WL_CONNECTED) return "";
  if (appConfig.telemetryEndpoint.length() == 0) return "";
  if (!timeClient.isTimeSet()) return "";

  DynamicJsonDocument doc(512);
  buildTelemetryPayload(doc);

  String json;
  serializeJson(doc, json);

  HTTPClient http;
  String url = appConfig.telemetryEndpoint + "/telemetry";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(json);

  String response = "";
  if (httpCode > 0) {
    response = http.getString();
    appStats.bootCount = 0;
    saveDailyStats();
    Logger::log("SYSTEM", "Telemetry sent successfully");
  } else {
    Logger::log("SYSTEM", ("Telemetry send failed: " + String(httpCode)).c_str());
  }
  http.end();

  return response;
}

static bool checkForFirmwareUpdate() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (appConfig.telemetryEndpoint.length() == 0) return false;

  HTTPClient http;
  String url = appConfig.telemetryEndpoint + "/firmware/check?ver=" + String(DESKBUDDY_VERSION);
  http.begin(url);
  http.setTimeout(8000);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String body = http.getString();
    http.end();

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, body);
    if (!err && doc["update_available"].as<bool>()) {
      pendingFirmwareUrl = doc["url"].as<String>();
      pendingFirmwareSize = doc["size"].as<size_t>();
      firmwareUpdatePending = true;

      Logger::log("SYSTEM", ("Firmware update available: v" + doc["version"].as<String>()).c_str());
      return true;
    }
  } else {
    http.end();
  }

  return false;
}

static bool downloadAndApplyFirmware() {
  if (!firmwareUpdatePending) return false;
  if (pendingFirmwareUrl.length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  Logger::log("SYSTEM", "Starting firmware download...");

  HTTPClient http;
  http.begin(pendingFirmwareUrl);
  http.setTimeout(30000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Logger::log("SYSTEM", ("Firmware download failed: HTTP " + String(httpCode)).c_str());
    http.end();
    firmwareUpdatePending = false;
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Logger::log("SYSTEM", "Firmware download failed: invalid content length");
    http.end();
    firmwareUpdatePending = false;
    return false;
  }

  bool canBegin = Update.begin(contentLength);
  if (!canBegin) {
    Logger::log("SYSTEM", "Update.begin() failed - not enough space or wrong partition");
    http.end();
    firmwareUpdatePending = false;
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = 0;
  size_t total = contentLength;
  uint8_t buf[1024];
  unsigned long lastProgress = millis();

  while (stream->connected() && written < total) {
    size_t available = stream->available();
    if (available > 0) {
      size_t toRead = available > sizeof(buf) ? sizeof(buf) : available;
      int bytesRead = stream->read(buf, toRead);
      if (bytesRead > 0) {
        size_t bytesWritten = Update.write(buf, bytesRead);
        if (bytesWritten != (size_t)bytesRead) {
          Logger::log("SYSTEM", "Update.write() failed - write error");
          Update.abort();
          http.end();
          firmwareUpdatePending = false;
          return false;
        }
        written += bytesWritten;
      }
    }
    if (millis() - lastProgress > 10000) {
      int pct = (int)((written * 100) / total);
      Logger::log("SYSTEM", ("Firmware download progress: " + String(pct) + "%").c_str());
      lastProgress = millis();
    }
    delay(1);
  }
  http.end();

  if (written != total) {
    Logger::log("SYSTEM", "Firmware download incomplete - aborting");
    Update.abort();
    firmwareUpdatePending = false;
    return false;
  }

  if (!Update.end()) {
    Logger::log("SYSTEM", "Update.end() failed - checksum error");
    firmwareUpdatePending = false;
    return false;
  }

  Logger::log("SYSTEM", "Firmware update applied successfully. Rebooting in 2 seconds...");
  delay(2000);
  ESP.restart();
  return true;
}

#endif // TELEMETRY_H
