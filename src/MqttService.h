#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <WiFi.h>
#include <PubSubClient.h>

// Extern instances defined in main.cpp
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

// MQTT History Buffer declarations
struct MqttMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};

#define MQTT_HISTORY_SIZE 50
extern MqttMessage mqttHistory[MQTT_HISTORY_SIZE];
extern int mqttHistoryHead;
extern int mqttHistoryCount;
extern SemaphoreHandle_t mqttHistoryMutex;

// Safe helper to append messages to history
inline void addMqttHistory(String topic, String payload) {
  if (mqttHistoryMutex == NULL) return;
  xSemaphoreTake(mqttHistoryMutex, portMAX_DELAY);
  
  mqttHistory[mqttHistoryHead].topic = topic;
  mqttHistory[mqttHistoryHead].payload = payload;
  mqttHistory[mqttHistoryHead].timestamp = millis();
  
  mqttHistoryHead = (mqttHistoryHead + 1) % MQTT_HISTORY_SIZE;
  if (mqttHistoryCount < MQTT_HISTORY_SIZE) {
    mqttHistoryCount++;
  }
  
  xSemaphoreGive(mqttHistoryMutex);
}

// Extern variables for triggering display events
extern SemaphoreHandle_t geminiMutex;
extern String aiResponse;
extern volatile bool lastResponseIsAi;
extern volatile bool hasNewAIResponse;
extern int lastTriggeredEventType;

// Callback invoked when MQTT messages are received
inline void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String p = "";
  for (unsigned int i = 0; i < length; i++) {
    p += (char)payload[i];
  }
  
  String t = String(topic);
  addMqttHistory(t, p);
  
  // If a message is published to the display topic, pop it up on the screen
  if (t == "deskbuddy/display") {
    if (geminiMutex != NULL) {
      xSemaphoreTake(geminiMutex, portMAX_DELAY);
      aiResponse = p;
      lastResponseIsAi = false;      // Display as standard local/MQTT message
      hasNewAIResponse = true;       // Trigger the display system
      lastTriggeredEventType = 99;   // Use a custom event type to bypass welcome delays
      xSemaphoreGive(geminiMutex);
    }
  }
}

// Configure connection settings to MQTT broker
inline void setupMqtt() {
  mqttClient.setServer("192.168.15.18", 1883);
  mqttClient.setCallback(mqttCallback);
}

// Asynchronously and non-blockingly maintain MQTT connection
inline void loopMqtt() {
  static unsigned long lastReconnectMqtt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastReconnectMqtt > 10000) {
        lastReconnectMqtt = now;
        // Attempt to connect asynchronously
        if (mqttClient.connect("DeskBuddyClient")) {
          mqttClient.publish("deskbuddy/status", "online");
          mqttClient.subscribe("deskbuddy/#"); // Subscribe to all deskbuddy topics
        }
      }
    } else {
      mqttClient.loop();
    }
  }
}

// Publish display alerts to MQTT broker
inline void publishMqttMessage(String msg) {
  if (mqttClient.connected()) {
    mqttClient.publish("deskbuddy/message", msg.c_str());
  }
}

#endif // MQTT_SERVICE_H
