#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>

inline void playKnock() {
  int p = AUDIO_PIN;
  // Tap 1: ~150Hz for 8ms — 3 cycles at ~3333us per half-period
  for (int i = 0; i < 3; i++) {
    digitalWrite(p, HIGH); delayMicroseconds(3333);
    digitalWrite(p, LOW);  delayMicroseconds(3333);
  }
  delayMicroseconds(10000); // 10ms gap
  // Tap 2: ~120Hz for 10ms — 3 cycles at ~4166us per half-period
  for (int i = 0; i < 3; i++) {
    digitalWrite(p, HIGH); delayMicroseconds(4166);
    digitalWrite(p, LOW);  delayMicroseconds(4166);
  }
}

inline void audioInit() {
  pinMode(AUDIO_PIN, OUTPUT);
}

#endif
