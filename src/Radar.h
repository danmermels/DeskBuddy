#ifndef RADAR_H
#define RADAR_H

#include <Arduino.h>
#include <ld2410.h>
#include <algorithm>
#include <Preferences.h>

#include "State.h"

// Extern global references from main.cpp
extern ld2410 radar;
extern Preferences preferences;

class RollingMedianFilter {
public:
  RollingMedianFilter(int maxSize = 100) {
    _maxSize = maxSize;
    _buffer     = new float[_maxSize];
    _sortBuffer = new float[_maxSize]; // Pre-allocated: avoids per-call heap churn in getMedian()
    clear();
  }

  ~RollingMedianFilter() {
    delete[] _buffer;
    delete[] _sortBuffer;
  }

  void clear() {
    _head = 0;
    _count = 0;
    for (int i = 0; i < _maxSize; i++) {
      _buffer[i] = 0.0;
    }
  }

  void add(float val) {
    _buffer[_head] = val;
    _head = (_head + 1) % _maxSize;
    if (_count < _maxSize) {
      _count++;
    }
  }

  float getMedian(int windowSize) {
    if (windowSize <= 0) return 0.0;
    if (windowSize > _maxSize) windowSize = _maxSize;
    
    int countToCopy = (_count < windowSize) ? _count : windowSize;
    if (countToCopy == 0) return 0.0;

    // Use pre-allocated sort buffer — no heap allocation at call time
    int idx = _head;
    for (int i = 0; i < countToCopy; i++) {
      idx = (idx - 1 + _maxSize) % _maxSize;
      _sortBuffer[i] = _buffer[idx];
    }

    std::sort(_sortBuffer, _sortBuffer + countToCopy);
    
    float result;
    if (countToCopy % 2 == 1) {
      result = _sortBuffer[countToCopy / 2];
    } else {
      result = (_sortBuffer[countToCopy / 2 - 1] + _sortBuffer[countToCopy / 2]) / 2.0;
    }
    return result;
  }

private:
  float* _buffer;
  float* _sortBuffer; // Pre-allocated scratch space for sorting, avoids runtime heap churn
  int _maxSize;
  int _head;
  int _count;
};

// Rolling Median Filter instances defined in main.cpp are accessed here or declared
extern RollingMedianFilter detectionDistFilter;
extern RollingMedianFilter motionFilter;

// Filtered values

inline void setupRadar() {
  // Initialize Serial1 for Radar on Pins 0 (RX) and 5 (TX)
  Serial1.begin(256000, SERIAL_8N1, 0, 5); 
  delay(500);
  if (radar.begin(Serial1)) {
    // Configure sensor distance resolution to 0.2m (20cm) programmatically
    uint8_t enter_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(enter_cmd, sizeof(enter_cmd));
    delay(150);
    uint8_t res_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xAA, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(res_cmd, sizeof(res_cmd));
    delay(150);
    uint8_t exit_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(exit_cmd, sizeof(exit_cmd));
    delay(200);
    
    // Restart to apply new resolution
    radar.requestRestart();
    delay(1000); // Give the module time to reboot and load firmware settings
    
    // Re-verify serial connection and query configuration from the physical module
    if (radar.requestCurrentConfiguration()) {
      // Sync local state variables with settings retrieved from the module
      appConfig.g0mSens = radar.motion_sensitivity[0];
      appConfig.g0sSens = radar.stationary_sensitivity[0];
      appConfig.g1mSens = radar.motion_sensitivity[1];
      appConfig.g1sSens = radar.stationary_sensitivity[1];
      appConfig.g2mSens = radar.motion_sensitivity[2];
      appConfig.g2sSens = radar.stationary_sensitivity[2];
      appConfig.g3mSens = radar.motion_sensitivity[3];
      appConfig.g3sSens = radar.stationary_sensitivity[3];
      appConfig.g4mSens = radar.motion_sensitivity[4];
      appConfig.g4sSens = radar.stationary_sensitivity[4];
      appConfig.g5mSens = radar.motion_sensitivity[5];
      appConfig.g5sSens = radar.stationary_sensitivity[5];
      appConfig.g6mSens = radar.motion_sensitivity[6];
      appConfig.g6sSens = radar.stationary_sensitivity[6];
      
      // Calculate deskDistanceLimit based on max_gate (each gate corresponds to 20cm)
      if (radar.max_gate >= 2 && radar.max_gate <= 8) {
        appConfig.deskDistanceLimit = radar.max_gate * 20;
      }
      
      // Commit these synced values back to Preferences (Flash)
      preferences.begin("deskbuddy", false);
      preferences.putInt("distLimit", appConfig.deskDistanceLimit);
      preferences.putInt("g0mSens", appConfig.g0mSens);
      preferences.putInt("g0sSens", appConfig.g0sSens);
      preferences.putInt("g1mSens", appConfig.g1mSens);
      preferences.putInt("g1sSens", appConfig.g1sSens);
      preferences.putInt("g2mSens", appConfig.g2mSens);
      preferences.putInt("g2sSens", appConfig.g2sSens);
      preferences.putInt("g3mSens", appConfig.g3mSens);
      preferences.putInt("g3sSens", appConfig.g3sSens);
      preferences.putInt("g4mSens", appConfig.g4mSens);
      preferences.putInt("g4sSens", appConfig.g4sSens);
      preferences.putInt("g5mSens", appConfig.g5mSens);
      preferences.putInt("g5sSens", appConfig.g5sSens);
      preferences.putInt("g6mSens", appConfig.g6mSens);
      preferences.putInt("g6sSens", appConfig.g6sSens);
      preferences.end();
      
      Serial.println("LD2410 configurations retrieved and synced successfully from the module.");
    } else {
      Serial.println("Failed to retrieve configuration from LD2410. Falling back to local settings.");
      // Fallback: apply local configurations (loaded from Preferences at startup) to the module
      radar.setGateSensitivityThreshold(0, appConfig.g0mSens, appConfig.g0sSens);
      radar.setGateSensitivityThreshold(1, appConfig.g1mSens, appConfig.g1sSens);
      radar.setGateSensitivityThreshold(2, appConfig.g2mSens, appConfig.g2sSens);
      radar.setGateSensitivityThreshold(3, appConfig.g3mSens, appConfig.g3sSens);
      radar.setGateSensitivityThreshold(4, appConfig.g4mSens, appConfig.g4sSens);
      radar.setGateSensitivityThreshold(5, appConfig.g5mSens, appConfig.g5sSens);
      radar.setGateSensitivityThreshold(6, appConfig.g6mSens, appConfig.g6sSens);
      
      int requiredGates = (appConfig.deskDistanceLimit + 19) / 20;
      if (requiredGates < 2) requiredGates = 2;
      if (requiredGates > 8) requiredGates = 8;
      radar.setMaxValues(requiredGates, requiredGates, 5);
    }
  }
}

#endif // RADAR_H
