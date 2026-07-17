#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "MessageManager.h"

#include "Constants.h"
#include "State.h"

// Debug platform forward declaration (defined in MqttDebug.h)
void handleDebugCommand(const String& payload);

// Extern instances defined in main.cpp
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;
extern MessageManager messageManager;

// MQTT History Buffer declarations
#ifndef MQTT_MESSAGE_STRUCT
#define MQTT_MESSAGE_STRUCT
struct MqttMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};
#endif


// Safe helper to append messages to history
inline void addMqttHistory(String topic, String payload) {
  if (appState.mqttHistoryMutex == NULL) return;
  xSemaphoreTake(appState.mqttHistoryMutex, portMAX_DELAY);
  
  appState.mqttHistory[appState.mqttHistoryHead].topic = topic;
  appState.mqttHistory[appState.mqttHistoryHead].payload = payload;
  appState.mqttHistory[appState.mqttHistoryHead].timestamp = millis();
  
  appState.mqttHistoryHead = (appState.mqttHistoryHead + 1) % MQTT_HISTORY_SIZE;
  if (appState.mqttHistoryCount < MQTT_HISTORY_SIZE) {
    appState.mqttHistoryCount++;
  }
  
  xSemaphoreGive(appState.mqttHistoryMutex);
}

// Extern variables for triggering display events

// Callback invoked when MQTT messages are received
inline void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String p = "";
  for (unsigned int i = 0; i < length; i++) {
    p += (char)payload[i];
  }
  
  String t = String(topic);
  addMqttHistory(t, p);
  
  // Route MQTT messages through MessageManager for proper queueing
  if (t == MQTT_DISPLAY_TOPIC || t == MQTT_PUBLISH_TOPIC) {
    messageManager.scheduleMessage(EVENT_MQTT_MESSAGE, p, MSG_PRIORITY_HIGH, 0, MSG_RELEVANCE_URGENT);
  }
  else if (t == MQTT_DEBUG_CMD_TOPIC) {
    handleDebugCommand(p);
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
      unsigned long now = millis();
      if (now - lastReconnectMqtt > MQTT_RECONNECT_INTERVAL_MS) {
        lastReconnectMqtt = now;
        // Attempt to connect asynchronously
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
          mqttClient.publish(MQTT_STATUS_TOPIC, MQTT_STATUS_PAYLOAD);
          mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC); // Subscribe to all deskbuddy topics
        }
      }
    } else {
      mqttClient.loop();
    }
  }
}

// Publish display alerts to MQTT echo topic (separate from input topics to avoid loops)
inline void publishMqttMessage(String msg) {
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_ECHO_TOPIC, msg.c_str());
  }
}

#endif // MQTT_SERVICE_H
