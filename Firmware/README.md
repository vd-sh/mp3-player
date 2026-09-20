# Steps to setup the firmware
1. Download an install Arduino IDE from [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Download all of the 7 codes in this repo
   - AudioPlayer.h
   - BtAudio.h
   - config.h
   - DisplayUI.h
   - InputManager.h
   - MP3Player.ino
   - PowerManager.h
3. Upload these 7 files in your project repositories and then navigate in Arduino IDE to open it
4. Download the following 4 libraries in Arduino IDE
   - ESP32-audioI2S (Specifically Version v2.0.0) (Along with all the recommended dependencies by Arduino IDE)
   - Adafruit ST7789 (Latest Version) (Along with all the recommended dependencies by Arduino IDE)
   - Adafruit MCP23017 Arduino Library (Latest Version) (Along with all the recommended dependencies by Arduino IDE)
   - ESP32-A2DP by pschatzmann (Specifically Version v1.8.1) (Not availabe on Arduino IDE so download from [https://github.com/pschatzmann/ESP32-A2DP/tree/v.1.8.1](https://github.com/pschatzmann/ESP32-A2DP/tree/v.1.8.1) and upload the downloaded .zip file in the library manager of Arduino IDE)
5. Flash your ESP32 WROOM 32E with this uploaded codes and libs
6. Run, Setup for the first time and it's all set!