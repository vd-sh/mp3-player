#pragma once
#include <Arduino.h>

#define PIN_I2S_BCK   26
#define PIN_I2S_LCK   25
#define PIN_I2S_DIN   22

#define PIN_BT_TAP    4

#define PIN_TFT_CS    5
#define PIN_TFT_DC    17
#define PIN_TFT_RST   16
#define PIN_TFT_BL    15

#define PIN_SD_CS     27
#define PIN_SD_MOSI   13
#define PIN_SD_SCK    14
#define PIN_SD_MISO   12

#define PIN_I2C_SDA   19
#define PIN_I2C_SCL   21

#define PIN_BAT_ADC   34
#define DIVIDER_RATIO 2.0f

#define SCREEN_SLEEP_MS    10000UL
#define POWER_OFF_MS       180000UL
#define UI_UPDATE_MS       200
#define BATT_UPDATE_MS     1000

#define BATT_WARN_VOLT   3.45f
#define BATT_CRIT_VOLT   3.25f

enum Btn { BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_OK, BTN_MENU, BTN_COUNT };
