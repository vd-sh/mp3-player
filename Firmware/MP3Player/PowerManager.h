#pragma once
#include <Arduino.h>
#include "config.h"

class PowerManager {
public:
  void begin() {
    pinMode(PIN_BAT_ADC, INPUT);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
  }

  float voltage() {
    return v;
  }

  int percent() {
    return pct;
  }

  void noteActivity() {
    lastActivity = millis();
  }

  unsigned long lastActivityTime() {
    return lastActivity;
  }

  int update() {
    if (millis() - lastBat < BATT_UPDATE_MS) {
      return 0;
    }
    lastBat = millis();
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) {
      acc += analogRead(PIN_BAT_ADC);
      delayMicroseconds(300);
    }
    v = (acc / 16.0f) / 4095.0f * 3.3f * DIVIDER_RATIO;
    pct = voltToPercent(v);
    if (v <= BATT_CRIT_VOLT) {
      return 2;
    }
    if (v <= BATT_WARN_VOLT && !warned) {
      warned = true;
      return 1;
    }
    if (v > BATT_WARN_VOLT + 0.05f) {
      warned = false;
    }
    return 0;
  }

  bool inactiveTooLong(bool musicPlaying) {
    return !musicPlaying && (millis() - lastActivity > POWER_OFF_MS);
  }

private:
  float v = 4.0f;
  int pct = 100;
  bool warned = false;
  unsigned long lastBat = 0, lastActivity = 0;

  int voltToPercent(float volt) {
    if (volt >= 4.20f) {
      return 100;
    }
    if (volt >= 4.00f) {
      return (int)((volt - 4.00f) / 0.20f * 20 + 80);
    }
    if (volt >= 3.80f) {
      return (int)((volt - 3.80f) / 0.20f * 20 + 60);
    }
    if (volt >= 3.60f) {
      return (int)((volt - 3.60f) / 0.20f * 20 + 40);
    }
    if (volt >= 3.40f) {
      return (int)((volt - 3.40f) / 0.20f * 20 + 20);
    }
    if (volt >= 3.30f) {
      return (int)((volt - 3.30f) / 0.10f * 15 + 5);
    }
    return 0;
  }
};
