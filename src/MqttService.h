#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <WiFi.h>
#include <PubSubClient.h>

#include "Constants.h"
#include "State.h"
#include "Logger.h"


// Debug platform forward declaration (defined in MqttDebug.h)
void handleDebugCommand(const String& payload);

// Extern instances defined in main.cpp
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

// MQTT History Buffer declarations
#include <queue>
#include <freertos/semphr.h>

struct MqttQueueMessage {
  String topic;
  String payload;
};

extern std::queue<MqttQueueMessage> mqttPublishQueue;
extern SemaphoreHandle_t mqttPublishQueueMutex;

inline void enqueueMqttPublish(const String& topic, const String& payload) {
  if (!appState.mqttConnected) return;
  if (mqttPublishQueueMutex == NULL) return;
  if (xSemaphoreTake(mqttPublishQueueMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (mqttPublishQueue.size() < 20) {
      mqttPublishQueue.push({topic, payload});
    } else {
      mqttPublishQueue.pop(); // Discard oldest
      mqttPublishQueue.push({topic, payload});
    }
    xSemaphoreGive(mqttPublishQueueMutex);
  }
}


// Extern variables for triggering display events

// Callback invoked when MQTT messages are received
inline void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (topic != nullptr && strcmp(topic, "deskbuddy/heap") == 0) {
    return; // Ignore loopback heap telemetry to prevent heap fragmentation
  }
  String p = "";
  for (unsigned int i = 0; i < length; i++) {
    p += (char)payload[i];
  }
  
  String t = String(topic);
  
  if (t == MQTT_DEBUG_CMD_TOPIC) {
#if DESKBUDDY_DEBUG
    handleDebugCommand(p);
#endif
  }
}

// Configure connection settings to MQTT broker
inline void setupMqtt() {
  mqttClient.setServer(appConfig.mqttBroker.c_str(), appConfig.mqttPort);
  mqttClient.setCallback(mqttCallback);
}

// Asynchronously and non-blockingly maintain MQTT connection
inline void loopMqtt() {
  static unsigned long lastReconnectMqtt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      appState.mqttConnected = false;
      unsigned long now = millis();
      if (now - lastReconnectMqtt > MQTT_RECONNECT_INTERVAL_MS) {
        lastReconnectMqtt = now;
        // Attempt to connect asynchronously
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
          mqttClient.publish(MQTT_STATUS_TOPIC, MQTT_STATUS_PAYLOAD);
          mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC); // Subscribe to all deskbuddy topics
          appState.mqttConnected = true;
        }
      }
    } else {
      appState.mqttConnected = true;
      mqttClient.loop();

      // Process queued messages safely in the loopTask thread
      while (true) {
        MqttQueueMessage msg;
        bool hasMsg = false;
        if (mqttPublishQueueMutex != NULL && xSemaphoreTake(mqttPublishQueueMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          if (!mqttPublishQueue.empty()) {
            msg = mqttPublishQueue.front();
            mqttPublishQueue.pop();
            hasMsg = true;
          }
          xSemaphoreGive(mqttPublishQueueMutex);
        }

        if (hasMsg) {
          mqttClient.publish(msg.topic.c_str(), msg.payload.c_str());
        } else {
          break;
        }
      }
    }
  } else {
    appState.mqttConnected = false;
  }
}

// Publish display alerts to MQTT echo topic (separate from input topics to avoid loops)
inline void publishMqttMessage(String msg) {
  if (appState.mqttConnected) {
    enqueueMqttPublish(MQTT_ECHO_TOPIC, msg);
  }
}

#endif // MQTT_SERVICE_H
