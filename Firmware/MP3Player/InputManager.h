#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "config.h"

#define BTN_SW      6
#define EV_PRESS    0
#define EV_RELEASE  1

struct InputEvent { uint8_t btn; uint8_t type; };

class InputManager {
public:
  void begin() {
    if (!mcp.begin_I2C(0x20, &Wire)) {
      Serial.println("ERROR: MCP23017 not found!");
    }
    for (int i = 0; i < 6; i++) {
      mcp.pinMode(i, INPUT_PULLUP);
    }
    for (int i = 0; i < 3; i++) {
      mcp.pinMode(8 + i, INPUT_PULLUP);
    }
    uint16_t s = readAll();
    lastRaw = s;
    lastStable = s;
    prevAB = (s >> 8) & 3;
    xTaskCreatePinnedToCore(task, "inputs", 3072, this, 2, NULL, 0);
  }

  bool pending() {
    return head != tail;
  }

  bool getEvent(InputEvent &e) {
    if (head == tail) {
      return false;
    }
    portENTER_CRITICAL(&mux);
    e = evq[tail];
    tail = (tail + 1) & 7;
    portEXIT_CRITICAL(&mux);
    return true;
  }

  int encoderDelta() {
    int d = encDelta;
    encDelta = 0;
    return d;
  }

  void clearAll() {
    head = tail = 0;
    encoderDelta();
  }

private:
  Adafruit_MCP23X17 mcp;
  InputEvent evq[8];
  volatile uint8_t head = 0, tail = 0;
  volatile int encDelta = 0;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  uint16_t lastRaw = 0, lastStable = 0;
  uint8_t prevAB = 0;
  int8_t encAcc = 0;
  uint32_t debounceUntil = 0;
  static const int8_t qtab[16];

  static void task(void *p) {
    ((InputManager *)p)->run();
  }

  uint16_t readAll() {
    uint8_t a = mcp.readGPIO(MCP23XXX_PORT::GPIOA);
    uint8_t b = mcp.readGPIO(MCP23XXX_PORT::GPIOB);
    return ((uint16_t)b << 8) | a;
  }

  void push(uint8_t btn, uint8_t type) {
    portENTER_CRITICAL(&mux);
    uint8_t n = (head + 1) & 7;
    if (n != tail) {
      evq[head] = {btn, type};
      head = n;
    }
    portEXIT_CRITICAL(&mux);
  }

  void run() {
    for (;;) {
      uint16_t raw = readAll();
      if (raw != lastRaw) {
        lastRaw = raw;
        debounceUntil = millis() + 30;
      } else if ((int32_t)(millis() - debounceUntil) >= 0 && raw != lastStable) {
        uint16_t chg = raw ^ lastStable;
        for (int i = 0; i < 6; i++) {
          if (chg & (1 << i)) {
            push(i, (raw & (1 << i)) ? EV_RELEASE : EV_PRESS);
          }
        }
        if (chg & (1 << 10)) {
          push(BTN_SW, (raw & (1 << 10)) ? EV_RELEASE : EV_PRESS);
        }
        lastStable = raw;
      }
      uint8_t ab = (raw >> 8) & 3;
      encAcc += qtab[(prevAB << 2) | ab];
      prevAB = ab;
      if (encAcc >= 4) {
        encDelta++;
        encAcc = 0;
      } else if (encAcc <= -4) {
        encDelta--;
        encAcc = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
};
const int8_t InputManager::qtab[16] =
  {0,-1, 1, 0,  1, 0, 0,-1,  -1, 0, 0, 1,  0, 1,-1, 0};
