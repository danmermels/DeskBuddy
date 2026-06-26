#ifndef RADAR_H
#define RADAR_H

#include <Arduino.h>
#include <ld2410.h>
#include <algorithm>

// Extern global references from main.cpp
extern ld2410 radar;
extern int g0mSens;
extern int g0sSens;
extern int g1mSens;
extern int g1sSens;
extern int g2mSens;
extern int g2sSens;
extern int g3mSens;
extern int g3sSens;
extern int g4mSens;
extern int g4sSens;
extern int g5mSens;
extern int g5sSens;
extern int g6mSens;
extern int g6sSens;
extern int deskDistanceLimit;

class RollingMedianFilter {
public:
  RollingMedianFilter(int maxSize = 100) {
    _maxSize = maxSize;
    _buffer = new float[_maxSize];
    clear();
  }

  ~RollingMedianFilter() {
    delete[] _buffer;
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

    float* temp = new float[countToCopy];
    int idx = _head;
    for (int i = 0; i < countToCopy; i++) {
      idx = (idx - 1 + _maxSize) % _maxSize;
      temp[i] = _buffer[idx];
    }

    std::sort(temp, temp + countToCopy);
    
    float result;
    if (countToCopy % 2 == 1) {
      result = temp[countToCopy / 2];
    } else {
      result = (temp[countToCopy / 2 - 1] + temp[countToCopy / 2]) / 2.0;
    }
    delete[] temp;
    return result;
  }

private:
  float* _buffer;
  int _maxSize;
  int _head;
  int _count;
};

// Rolling Median Filter instances defined in main.cpp are accessed here or declared
extern RollingMedianFilter detectionDistFilter;
extern RollingMedianFilter motionFilter;

// Filtered values
extern float filteredDetectionDist;

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
    
    // Re-verify serial connection and configure gates
    radar.setGateSensitivityThreshold(0, g0mSens, g0sSens);
    radar.setGateSensitivityThreshold(1, g1mSens, g1sSens);
    radar.setGateSensitivityThreshold(2, g2mSens, g2sSens);
    radar.setGateSensitivityThreshold(3, g3mSens, g3sSens);
    radar.setGateSensitivityThreshold(4, g4mSens, g4sSens);
    radar.setGateSensitivityThreshold(5, g5mSens, g5sSens);
    radar.setGateSensitivityThreshold(6, g6mSens, g6sSens);
    
    int requiredGates = (deskDistanceLimit + 19) / 20;
    if (requiredGates < 2) requiredGates = 2;
    if (requiredGates > 8) requiredGates = 8;
    radar.setMaxValues(requiredGates, requiredGates, 5);
  }
}

#endif // RADAR_H
