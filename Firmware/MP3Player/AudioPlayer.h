#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <Audio.h>
#include "config.h"

enum PlayMode { MODE_ORDER = 0, MODE_SHUFFLE, MODE_LOOP_ALL, MODE_LOOP_ONE };
enum EqPreset { EQ_NORMAL = 0, EQ_BASS, EQ_VOCAL, EQ_TREBLE };

class AudioPlayer {
public:
  Audio audio;
  std::vector<String> tracks;
  int current = -1;
  bool isPlaying = false;
  bool trackEnded = false;
  bool sdOK = false;
  PlayMode mode = MODE_ORDER;
  EqPreset eq = EQ_NORMAL;
  uint8_t volumePct = 60;

  void begin() {
    audio.setPinout(PIN_I2S_BCK, PIN_I2S_LCK, PIN_I2S_DIN);
    applyVolume();
    applyEq();

    SPI1.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI);

    if (SD.begin(PIN_SD_CS, SPI1, 40000000)) {
      sdOK = true;
      scan("/");
      Serial.printf("SD OK, %d tracks\n", (int)tracks.size());
    } else {
      Serial.println("SD CARD ERROR");
    }
  }

  int count() {
    return tracks.size();
  }

  String name(int i) {
    String s = tracks[i];
    int p = s.lastIndexOf('/');
    if (p >= 0) {
      return s.substring(p + 1);
    }
    return s;
  }

  void play(int i) {
    if (!sdOK || i < 0 || i >= (int)tracks.size()) {
      return;
    }
    current = i;
    audio.connecttoFS(SD, tracks[i].c_str());
    isPlaying = true;
  }

  void resumePause() {
    if (current < 0) {
      if (tracks.size()) {
        play(0);
      }
      return;
    }
    audio.pauseResume();
    isPlaying = !isPlaying;
  }

  void stop() {
    audio.stopSong();
    isPlaying = false;
  }

  void next() {
    int n = tracks.size();
    if (!n) {
      return;
    }
    if (mode == MODE_SHUFFLE) {
      if (n == 1) {
        play(0);
      } else {
        int k;
        do {
          k = random(n);
        } while (k == current);
        play(k);
      }
    } else if (mode == MODE_LOOP_ONE) {
      play(current < 0 ? 0 : current);
    } else {
      play((current + 1) % n);
    }
  }

  void prev() {
    int n = tracks.size();
    if (!n) {
      return;
    }
    if (mode == MODE_LOOP_ONE) {
      play(current < 0 ? 0 : current);
    } else {
      play((current - 1 + n) % n);
    }
  }

  void onEnd() {
    isPlaying = false;
    if (mode == MODE_ORDER && current >= (int)tracks.size() - 1) {
      current = -1;
      return;
    }
    next();
  }

  void loop() {
    audio.loop();
    if (trackEnded) {
      trackEnded = false;
      onEnd();
    }
  }

  uint16_t curTime() {
    return audio.getAudioCurrentTime();
  }

  uint16_t durTime() {
    return audio.getAudioFileDuration();
  }

  void setVolumePct(uint8_t p) {
    volumePct = constrain(p, 0, 100);
    applyVolume();
  }

  void changeVolume(int d) {
    setVolumePct((int)volumePct + d);
  }

  void applyVolume() {
    audio.setVolume(map(volumePct, 0, 100, 0, 21));
  }

  void applyEq() {
    switch (eq) {
      case EQ_NORMAL:
        audio.setTone(0, 0, 0);
        break;
      case EQ_BASS:
        audio.setTone(12, 3, 5);
        break;
      case EQ_VOCAL:
        audio.setTone(-3, 10, 4);
        break;
      case EQ_TREBLE:
        audio.setTone(0, -3, 12);
        break;
    }
  }

  void fadeOut(uint16_t ms) {
    int v = volumePct;
    for (int i = 0; i < 10; i++) {
      setVolumePct(map(i, 0, 9, v, 0));
      delay(ms / 10);
    }
    audio.stopSong();
    isPlaying = false;
    setVolumePct(v);
  }

private:
  void scan(const char *dir) {
    File root = SD.open(dir);
    if (!root) {
      return;
    }
    File f = root.openNextFile();
    while (f) {
      if (f.isDirectory()) {
        String p = f.path();
        if (!p.startsWith("/System")) {
          scan(p.c_str());
        }
      } else {
        String p = f.path();
        if (p.endsWith(".mp3") || p.endsWith(".MP3")) {
          tracks.push_back(p);
        }
      }
      f = root.openNextFile();
    }
    root.close();
  }
};
