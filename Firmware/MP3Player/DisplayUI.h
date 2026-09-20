#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "AudioPlayer.h"

#ifndef ESP_ARDUINO_VERSION_VAL
#define ESP_ARDUINO_VERSION_VAL(a,b,c) 0
#endif

class DisplayUI {
public:
  static const int16_t W = 280, H = 240;
  Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

  void begin() {
    tft.init(240, 280);
    tft.setSPISpeed(40000000);
    tft.setRotation(1);
    tft.fillScreen(ST7789_BLACK);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
    ledcAttach(PIN_TFT_BL, 5000, 8);
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_TFT_BL, 0);
#endif
    setBL(255);
  }

  void setBL(uint8_t b) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
    ledcWrite(PIN_TFT_BL, b);
#else
    ledcWrite(0, b);
#endif
  }

  void popupVolume(uint8_t vol) {
    popupVol = vol;
    popupUntil = millis() + 1200;
  }

  void toast(const String &s, uint16_t ms = 2000) {
    toastMsg = s;
    toastUntil = millis() + ms;
  }

  void nowPlayingFrame() {
    tft.fillScreen(ST7789_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST7789_DARKGREY);
    tft.setCursor(4, H - 16);
    tft.print("OK PLAY  < > SKIP  ^ v VOL  ENC VOL  MENU");
  }

  void updateNowPlaying(const String &title, int idx, int total,
                        uint16_t cur, uint16_t dur, int pct,
                        const char *mode, const char *extra) {
    statusBar(pct, mode, extra);

    tft.fillRect(0, 22, W, 22, ST7789_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_WHITE);
    int tw = title.length() * 12;
    if (tw <= W - 10) {
      tft.setCursor(5, 26);
      tft.print(title);
    } else {
      int off = (millis() / 180) % (tw + 60);
      tft.setCursor(5 - off, 26);
      tft.print(title);
    }

    tft.fillRect(0, 47, W, 12, ST7789_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST7789_DARKGREY);
    tft.setCursor(5, 49);
    if (idx >= 0) {
      char b[24];
      sprintf(b, "TRACK %d / %d", idx + 1, total);
      tft.print(b);
    } else {
      tft.print("PRESS OK TO PLAY");
    }

    char t1[8], t2[8];
    mmss(cur, t1);
    mmss(dur, t2);
    tft.fillRect(0, 66, W, 66, ST7789_BLACK);
    tft.setTextSize(4);
    tft.setTextColor(ST7789_GREEN);
    int16_t w1 = strlen(t1) * 24;
    tft.setCursor((W - w1) / 2, 70);
    tft.print(t1);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_DARKGREY);
    tft.setCursor((W - w1) / 2 + w1 + 10, 86);
    tft.print("/ ");
    tft.print(t2);

    const int bx = 30, by = 142, bw = W - 60;
    tft.drawRect(bx - 2, by - 2, bw + 4, 14, ST7789_WHITE);
    tft.fillRect(bx, by, bw, 10, ST7789_BLACK);
    if (dur > 0) {
      tft.fillRect(bx, by, constrain(map(cur, 0, dur, 0, bw), 0, bw), 10, ST7789_CYAN);
    }

    tft.fillRect(40, 164, W - 80, 30, ST7789_BLACK);
    if (millis() < toastUntil) {
      drawCenterBox(toastMsg);
    } else if (millis() < popupUntil) {
      char b[10];
      sprintf(b, "VOL %d%%", popupVol);
      drawBarBox(b, popupVol);
    }
  }

  void menuList(const char *title, const String *items, int count, int cursor) {
    tft.fillScreen(ST7789_BLACK);
    tft.fillRect(0, 0, W, 20, 0x0004);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_WHITE);
    tft.setCursor(6, 2);
    tft.print(title);
    for (int i = 0; i < count; i++) {
      int y = 26 + i * 23;
      if (i == cursor) {
        tft.fillRect(0, y, W, 22, 0x0016);
        tft.setTextColor(ST7789_CYAN);
      } else {
        tft.setTextColor(ST7789_LIGHTGREY);
      }
      tft.setTextSize(2);
      tft.setCursor(8, y + 3);
      String s = items[i];
      if (s.length() > 22) {
        s = s.substring(0, 22);
      }
      tft.print(s);
    }
  }

  void libraryList(AudioPlayer &pl, int start, int cursor) {
    tft.fillScreen(ST7789_BLACK);
    tft.fillRect(0, 0, W, 20, 0x0004);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_WHITE);
    tft.setCursor(6, 2);
    tft.print("LIBRARY (");
    tft.print(pl.count());
    tft.print(")");
    const int rows = 6;
    for (int r = 0; r < rows; r++) {
      int i = start + r;
      if (i >= pl.count()) {
        break;
      }
      int y = 26 + r * 34;
      if (i == cursor) {
        tft.fillRect(0, y, W, 32, 0x0016);
        tft.setTextColor(ST7789_CYAN);
      } else {
        tft.setTextColor(ST7789_LIGHTGREY);
      }
      tft.setTextSize(2);
      tft.setCursor(8, y + 8);
      String s = pl.name(i);
      if (s.length() > 22) {
        s = s.substring(0, 21) + "~";
      }
      tft.print(s);
    }
    if (pl.count() > rows) {
      int sh = (H - 24) * rows / pl.count();
      int sy = 22 + (int)(H - 24 - sh) * start / max(1, pl.count() - rows);
      tft.fillRect(W - 4, 22, 3, H - 24, 0x2104);
      tft.fillRect(W - 4, sy, 3, sh, ST7789_CYAN);
    }
  }

  void btEditDraw(const char *name, int pos) {
    tft.fillScreen(ST7789_BLACK);
    tft.fillRect(0, 0, W, 20, 0x0004);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_WHITE);
    tft.setCursor(6, 2);
    tft.print("BT DEVICE NAME");
    int len = strlen(name);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_WHITE);
    int x0 = max(4, (W - len * 12) / 2);
    tft.setCursor(x0, 70);
    tft.print(name);
    if (pos <= len) {
      int cx = x0 + pos * 12;
      tft.fillRect(cx, 90, 12, 3, ST7789_CYAN);
      tft.drawRect(cx - 1, 54, 14, 40, ST7789_DARKGREY);
    }
    tft.setTextSize(1);
    tft.setTextColor(ST7789_LIGHTGREY);
    tft.setCursor(10, 130);
    tft.print("Enter the advertised name of your BT");
    tft.setCursor(10, 142);
    tft.print("headphones / speaker (e.g. WH-1000XM4)");
    tft.setTextColor(ST7789_DARKGREY);
    tft.setCursor(4, H - 16);
    tft.print("ENC change char  OK next char  MENU save");
  }

  void showMessage(const char *l1, const char *l2) {
    tft.fillScreen(ST7789_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(ST7789_CYAN);
    tft.setCursor(max(0, (W - (int)strlen(l1) * 18) / 2), H / 2 - 30);
    tft.print(l1);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_LIGHTGREY);
    tft.setCursor(max(0, (W - (int)strlen(l2) * 12) / 2), H / 2 + 10);
    tft.print(l2);
  }

private:
  uint32_t popupUntil = 0, toastUntil = 0;
  uint8_t popupVol = 0;
  String toastMsg;

  static void mmss(uint16_t s, char *b) {
    sprintf(b, "%02d:%02d", s / 60, s % 60);
  }

  void statusBar(int pct, const char *mode, const char *extra) {
    tft.fillRect(0, 0, W, 18, 0x2104);
    tft.setTextSize(1);
    tft.setTextColor(ST7789_CYAN);
    tft.setCursor(3, 5);
    tft.print(mode);
    tft.setTextColor(ST7789_ORANGE);
    tft.setCursor(80, 5);
    tft.print(extra);
    char b[12];
    sprintf(b, "%d%%", pct);
    tft.setTextColor(ST7789_WHITE);
    tft.setCursor(W - 66, 5);
    tft.print(b);
    batteryIcon(W - 34, 3, pct);
  }

  void batteryIcon(int x, int y, int pct) {
    tft.drawRect(x, y, 22, 11, ST7789_WHITE);
    tft.fillRect(x + 22, y + 3, 2, 5, ST7789_WHITE);
    tft.fillRect(x + 1, y + 1, 20, 9, ST7789_BLACK);
    tft.fillRect(x + 1, y + 1, map(constrain(pct, 0, 100), 0, 100, 0, 20), 9,
                 pct > 50 ? ST7789_GREEN : (pct > 20 ? ST7789_YELLOW : ST7789_RED));
  }

  void drawBarBox(const char *label, uint8_t pct) {
    const int bx = 50, by = 170, bw = W - 100;
    tft.drawRect(bx, by, bw, 18, ST7789_WHITE);
    tft.fillRect(bx + 2, by + 2, map(pct, 0, 100, 0, bw - 4), 14, ST7789_GREEN);
    tft.setTextSize(1);
    tft.setTextColor(ST7789_WHITE);
    tft.setCursor(bx + 4, by + 5);
    tft.print(label);
  }

  void drawCenterBox(const String &s) {
    const int bx = 30, by = 166, bw = W - 60, bh = 26;
    tft.drawRect(bx, by, bw, bh, ST7789_ORANGE);
    tft.setTextSize(2);
    tft.setTextColor(ST7789_ORANGE);
    tft.setCursor(bx + (bw - (int)s.length() * 12) / 2, by + 5);
    tft.print(s);
  }
};
