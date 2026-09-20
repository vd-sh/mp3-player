#include "config.h"
#include "PowerManager.h"
#include "InputManager.h"
#include "AudioPlayer.h"
#include "DisplayUI.h"
#include "BtAudio.h"

PowerManager power;
InputManager input;
AudioPlayer  player;
DisplayUI    ui;

enum AppState { ST_PLAY, ST_MENU, ST_LIBRARY, ST_BTEDIT };
AppState state = ST_PLAY;

const char *modeNames[4]   = {"ORDER", "SHUFFLE", "LOOP ALL", "LOOP ONE"};
const char *eqNames[4]     = {"Normal", "Bass +", "Vocal", "Treble +"};
const char *brightNames[3] = {"Low", "Medium", "High"};
const uint8_t brightVals[3] = {60, 150, 255};

int  menuCursor = 0;
int  libCursor = 0, libStart = 0;
int  brightnessIdx = 2;
int  sleepTimerMin = 0;
uint32_t sleepRemainSec = 0;
bool customSet = false;
int  customMin = 15;
uint32_t lastUi = 0, lastSecTick = 0;
bool screenAsleep = false;

const char BTCHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
char btNameBuf[25];
int  btEditPos = 0;

void audio_eof_mp3(const char *) { player.trackEnded = true; }
void audio_info(const char *) {}
void audio_id3data(const char *) {}
void audio_showstreamtitle(const char *) {}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(PIN_BAT_ADC));
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  power.begin();
  input.begin();
  ui.begin();
  player.begin();
  bt.begin();
  bt.setVolumePct(player.volumePct);
  ui.showMessage("LUXE PLAYER", player.sdOK ? "SD card OK" : "SD CARD ERROR!");
  delay(1500);
  ui.nowPlayingFrame();
  power.noteActivity();
}

void loop() {
  player.loop();
  handleInput();
  sleepTimerTick();
  powerTick();

  if (state == ST_PLAY && !screenAsleep && millis() - lastUi >= UI_UPDATE_MS) {
    lastUi = millis();
    refreshPlay();
  }
  if (!screenAsleep && millis() - power.lastActivityTime() >= SCREEN_SLEEP_MS) {
    screenSleep();
  }
}

void screenSleep() {
  ui.setBL(0);
  screenAsleep = true;
}

void screenWake() {
  ui.setBL(brightVals[brightnessIdx]);
  screenAsleep = false;
  power.noteActivity();
  lastUi = 0;
}

void uiRefresh() {
  lastUi = 0;
}

void closeMenu() {
  state = ST_PLAY;
  ui.nowPlayingFrame();
  uiRefresh();
}

void changeVol(int d) {
  player.changeVolume(d);
  if (bt.started) {
    bt.setVolumePct(player.volumePct);
  }
  ui.popupVolume(player.volumePct);
}

void handleInput() {
  int d = input.encoderDelta();
  bool anyBtn = input.pending();

  if (screenAsleep) {
    if (d != 0 || anyBtn) {
      input.clearAll();
      screenWake();
    }
    return;
  }
  if (d != 0) {
    power.noteActivity();
    if (customSet) {
      customMin = constrain(customMin + d, 1, 120);
      drawMenu();
    } else if (state == ST_PLAY) {
      changeVol(d * 5);
    } else if (state == ST_MENU) {
      menuEncoder(d);
    } else if (state == ST_LIBRARY) {
      libCursor = constrain(libCursor + d, 0, max(0, player.count() - 1));
      drawLibrary();
    } else if (state == ST_BTEDIT) {
      btEditEncoder(d);
    }
  }
  InputEvent ev;
  while (input.getEvent(ev)) {
    if (ev.type != EV_PRESS) {
      continue;
    }
    power.noteActivity();
    dispatch(ev.btn);
  }
}

void dispatch(uint8_t btn) {
  if (state == ST_PLAY) {
    switch (btn) {
      case BTN_OK:    player.resumePause(); uiRefresh(); break;
      case BTN_SW:    player.resumePause(); uiRefresh(); break;
      case BTN_LEFT:  if (player.current >= 0) { player.prev(); uiRefresh(); } break;
      case BTN_RIGHT: if (player.current >= 0) { player.next(); uiRefresh(); } break;
      case BTN_UP:    changeVol(5);  break;
      case BTN_DOWN:  changeVol(-5); break;
      case BTN_MENU:  state = ST_MENU; drawMenu(); break;
    }
  } else if (state == ST_MENU) {
    switch (btn) {
      case BTN_UP:    if (!customSet) { menuCursor = (menuCursor + 8) % 9; drawMenu(); } break;
      case BTN_DOWN:  if (!customSet) { menuCursor = (menuCursor + 1) % 9; drawMenu(); } break;
      case BTN_LEFT:  menuAdjust(-1); break;
      case BTN_RIGHT: menuAdjust(1);  break;
      case BTN_OK:    menuOk(); break;
      case BTN_MENU:
        if (customSet) {
          customSet = false;
          drawMenu();
        } else {
          closeMenu();
        }
        break;
    }
  } else if (state == ST_BTEDIT) {
    switch (btn) {
      case BTN_UP:    btEditEncoder(1);  break;
      case BTN_DOWN:  btEditEncoder(-1); break;
      case BTN_OK:    btEditOk();  break;
      case BTN_MENU:  btEditSave(); break;
    }
  } else {
    switch (btn) {
      case BTN_UP:    libCursor = max(0, libCursor - 1); drawLibrary(); break;
      case BTN_DOWN:  libCursor = min(player.count() - 1, libCursor + 1); drawLibrary(); break;
      case BTN_OK:    player.play(libCursor); closeMenu(); break;
      case BTN_MENU:  closeMenu(); break;
    }
  }
}

String sleepTimerStr() {
  if (sleepTimerMin == 0) {
    return "Off";
  }
  return String(sleepTimerMin) + " min";
}

void drawMenu() {
  String items[9];
  items[0] = "Now Playing";
  items[1] = "Library  (" + String(player.count()) + " tracks)";
  items[2] = "Play Mode : " + String(modeNames[player.mode]);
  items[3] = "Equalizer : " + String(eqNames[player.eq]);
  items[4] = "Sleep Tmr : " + (customSet ? String(customMin) + " min  (ENC set/OK save)" : sleepTimerStr());
  items[5] = "Brightness: " + String(brightNames[brightnessIdx]);
  items[6] = "Bluetooth : " + String(bt.started ? "ON" : "OFF");
  items[7] = "BT Device : " + (bt.devName.length() > 13 ? bt.devName.substring(0, 13) + "~" : bt.devName);
  items[8] = "Power Off";
  ui.menuList("MENU", items, 9, menuCursor);
}

void cycleSleep(int dir) {
  const int seq[5] = {0, 10, 15, 30, -1};
  int i = 0;
  while (i < 5 && seq[i] != sleepTimerMin) {
    i++;
  }
  if (i == 5) {
    i = 0;
  }
  int n = (i + 5 + dir) % 5;
  if (seq[n] == -1) {
    customSet = true;
  } else {
    sleepTimerMin = seq[n];
    sleepRemainSec = 0;
  }
}

void toggleBt() {
  if (bt.started) {
    bt.stop();
    ui.toast("Bluetooth OFF");
  } else {
    bt.start();
    bt.setVolumePct(player.volumePct);
    ui.toast("BT searching: " + bt.devName, 3000);
  }
  drawMenu();
}

void menuAdjust(int dir) {
  switch (menuCursor) {
    case 2: player.mode = (PlayMode)((player.mode + 4 + dir) % 4); break;
    case 3: player.eq = (EqPreset)((player.eq + 4 + dir) % 4); player.applyEq(); break;
    case 4: cycleSleep(dir); break;
    case 5: brightnessIdx = (brightnessIdx + 3 + dir) % 3; ui.setBL(brightVals[brightnessIdx]); break;
    case 6: toggleBt(); return;
    default: menuCursor = (menuCursor + 9 + dir) % 9;
  }
  drawMenu();
}

void menuOk() {
  if (customSet) {
    sleepTimerMin = customMin;
    customSet = false;
    sleepRemainSec = 0;
    drawMenu();
    return;
  }
  switch (menuCursor) {
    case 0: closeMenu(); break;
    case 1: if (player.count()) { state = ST_LIBRARY; libCursor = max(0, player.current); libStart = libCursor; drawLibrary(); } break;
    case 2: player.mode = (PlayMode)((player.mode + 1) % 4); drawMenu(); break;
    case 3: player.eq = (EqPreset)((player.eq + 1) % 4); player.applyEq(); drawMenu(); break;
    case 4: cycleSleep(1); drawMenu(); break;
    case 5: brightnessIdx = (brightnessIdx + 1) % 3; ui.setBL(brightVals[brightnessIdx]); drawMenu(); break;
    case 6: toggleBt(); break;
    case 7: enterBtEdit(); break;
    case 8: shutdownSeq("Goodbye", "Flip switch off/on"); break;
  }
}

void drawLibrary() {
  int n = player.count();
  if (!n) {
    return;
  }
  libCursor = constrain(libCursor, 0, n - 1);
  if (libCursor < libStart) {
    libStart = libCursor;
  }
  if (libCursor > libStart + 5) {
    libStart = libCursor - 5;
  }
  ui.libraryList(player, libStart, libCursor);
}

void enterBtEdit() {
  state = ST_BTEDIT;
  strncpy(btNameBuf, bt.devName.c_str(), 24);
  btNameBuf[24] = 0;
  btEditPos = strlen(btNameBuf);
  if (btEditPos > 22) {
    btEditPos = 22;
  }
  ui.btEditDraw(btNameBuf, btEditPos);
}

void btEditEncoder(int d) {
  int len = strlen(btNameBuf);
  if (btEditPos > len) {
    btEditPos = len;
  }
  const char *p = strchr(BTCHARS, btNameBuf[btEditPos]);
  int idx = p ? (int)(p - BTCHARS) : 0;
  int n = strlen(BTCHARS);
  idx = ((idx + d) % n + n) % n;
  btNameBuf[btEditPos] = BTCHARS[idx];
  ui.btEditDraw(btNameBuf, btEditPos);
}

void btEditOk() {
  int len = strlen(btNameBuf);
  if (btEditPos < 23) {
    btEditPos++;
    if (btEditPos > len) {
      btNameBuf[btEditPos] = 'A';
      btNameBuf[btEditPos + 1] = 0;
    }
    ui.btEditDraw(btNameBuf, btEditPos);
  } else {
    btEditSave();
  }
}

void btEditSave() {
  String s = btNameBuf;
  s.trim();
  if (s.length() == 0) {
    s = "BT Speaker";
  }
  bt.setDeviceName(s);
  state = ST_MENU;
  drawMenu();
  ui.toast("BT device set: " + s);
}

void sleepTimerTick() {
  if (sleepTimerMin <= 0) {
    return;
  }
  if (!player.isPlaying) {
    sleepRemainSec = (uint32_t)sleepTimerMin * 60;
    lastSecTick = millis();
    return;
  }
  if (millis() - lastSecTick >= 1000) {
    lastSecTick = millis();
    if (sleepRemainSec > 0) {
      sleepRemainSec--;
      if (sleepRemainSec == 0) {
        player.fadeOut(1500);
        if (bt.started) {
          bt.fadeOut(1500);
        }
        sleepTimerMin = 0;
        ui.toast("Sleep timer - good night");
        uiRefresh();
      }
    }
  }
}

void powerTick() {
  int r = power.update();
  if (r == 1) {
    ui.toast("Low battery " + String(power.percent()) + "%");
  }
  if (r == 2) {
    shutdownSeq("Battery empty", "Please charge");
  }
  if (power.inactiveTooLong(player.isPlaying)) {
    shutdownSeq("Auto power-off", "No activity 3 min");
  }
}

void shutdownSeq(const char *l1, const char *l2) {
  player.fadeOut(800);
  if (bt.started) {
    bt.fadeOut(800);
  }
  ui.setBL(brightVals[brightnessIdx]);
  ui.showMessage(l1, l2);
  delay(2500);
  esp_deep_sleep_start();
}

String extraStr() {
  if (sleepTimerMin > 0 && player.isPlaying) {
    char b[16];
    sprintf(b, "SLEEP %02d:%02d", sleepRemainSec / 60, sleepRemainSec % 60);
    return b;
  }
  if (bt.started) {
    return bt.status();
  }
  return "";
}

void refreshPlay() {
  String t = (player.current >= 0) ? player.name(player.current) : "NO TRACK LOADED";
  ui.updateNowPlaying(t, player.current, player.count(),
                      player.curTime(), player.durTime(),
                      power.percent(),
                      modeNames[player.mode], extraStr().c_str());
}
