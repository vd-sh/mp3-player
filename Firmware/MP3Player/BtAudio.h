#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <BluetoothA2DPSource.h>
#include "driver/i2s.h"
#include "config.h"

class BtAudio {
public:
  static const uint32_t RING_FRAMES = 4096;

  BluetoothA2DPSource a2dp;
  Preferences prefs;

  bool started = false;
  volatile bool connected = false;
  uint8_t volPct = 60;
  String devName = "BT Speaker";

  int16_t ring[2 * RING_FRAMES];
  volatile uint32_t rHead = 0;
  volatile uint32_t rTail = 0;
  int16_t lastL = 0, lastR = 0;
  TaskHandle_t readerTask = nullptr;

  static BtAudio self;
  static BtAudio &I() {
    return self;
  }

  void begin() {
    prefs.begin("luxe", true);
    devName = prefs.getString("btname", "BT Speaker");
    volPct  = prefs.getUChar("btvol", 60);
    bool on = prefs.getBool("bton", false);
    prefs.end();

    a2dp.set_data_callback_in_frames(&BtAudio::dataCB);
    a2dp.set_auto_reconnect(true);
    a2dp.set_connection_state_callback(&BtAudio::connCB);

    if (on) {
      start();
    }
  }

  void start() {
    if (started) {
      return;
    }
    started = true;
    rHead = rTail = 0;
    lastL = lastR = 0;
    startTap();
    xTaskCreatePinnedToCore(readerFn, "bt_i2s", 4096, this, 3, &readerTask, 0);
    std::vector<const char *> names{devName.c_str()};
    a2dp.start(names);
    prefs.begin("luxe", false);
    prefs.putBool("bton", true);
    prefs.end();
  }

  void stop() {
    if (!started) {
      return;
    }
    a2dp.end();
    started = false;
    connected = false;
    if (readerTask) {
      vTaskDelete(readerTask);
      readerTask = nullptr;
    }
    stopTap();
    prefs.begin("luxe", false);
    prefs.putBool("bton", false);
    prefs.end();
  }

  void setDeviceName(const String &n) {
    devName = n;
    prefs.begin("luxe", false);
    prefs.putString("btname", devName);
    prefs.end();
    if (started) {
      stop();
      start();
    }
  }

  void setVolumePct(uint8_t p) {
    volPct = constrain(p, 0, 100);
    a2dp.set_volume(volPct * 127 / 100);
    prefs.begin("luxe", false);
    prefs.putUChar("btvol", volPct);
    prefs.end();
  }

  void fadeOut(uint16_t ms) {
    int top = volPct * 127 / 100;
    for (int i = 0; i < 10; i++) {
      a2dp.set_volume(map(i, 0, 9, top, 0));
      delay(ms / 10);
    }
    a2dp.set_volume(top);
  }

  const char *status() {
    if (!started) {
      return "";
    }
    return connected ? "BT OK" : "BT...";
  }

private:
  void startTap() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX);
    cfg.sample_rate          = 44100;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 8;
    cfg.dma_buf_len          = 256;
    cfg.use_apll             = false;
    i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
    i2s_pin_config_t pins = { PIN_I2S_BCK, PIN_I2S_LCK, I2S_PIN_NO_CHANGE, PIN_BT_TAP };
    i2s_set_pin(I2S_NUM_1, &pins);
  }

  void stopTap() {
    i2s_driver_uninstall(I2S_NUM_1);
  }

  static void readerFn(void *p) {
    BtAudio *b = (BtAudio *)p;
    int16_t tmp[1024];
    while (b->started) {
      size_t n = 0;
      if (i2s_read(I2S_NUM_1, tmp, sizeof(tmp), &n, portMAX_DELAY) != ESP_OK || n == 0) {
        continue;
      }
      uint32_t frames = n / 4;
      for (uint32_t i = 0; i < frames; i++) {
        uint32_t tail = b->rTail;
        b->ring[2 * tail]     = tmp[2 * i];
        b->ring[2 * tail + 1] = tmp[2 * i + 1];
        b->lastL = tmp[2 * i];
        b->lastR = tmp[2 * i + 1];
        uint32_t next = (tail + 1) % RING_FRAMES;
        if (next == b->rHead) {
          b->rHead = (b->rHead + 1) % RING_FRAMES;
        }
        b->rTail = next;
      }
    }
    vTaskDelete(NULL);
  }

  static int32_t dataCB(Frame *frame, int32_t count) {
    BtAudio &b = BtAudio::I();
    uint32_t avail = (b.rTail - b.rHead + RING_FRAMES) % RING_FRAMES;
    if (avail >= (uint32_t)count) {
      for (int32_t i = 0; i < count; i++) {
        frame[i].channel1 = b.ring[2 * b.rHead];
        frame[i].channel2 = b.ring[2 * b.rHead + 1];
        b.rHead = (b.rHead + 1) % RING_FRAMES;
      }
    } else {
      for (int32_t i = 0; i < count; i++) {
        frame[i].channel1 = b.lastL;
        frame[i].channel2 = b.lastR;
      }
    }
    return count;
  }

  static void connCB(esp_a2d_connection_state_t state, void *) {
    BtAudio::I().connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
  }
};
BtAudio BtAudio::self;

BtAudio &bt = BtAudio::I();
