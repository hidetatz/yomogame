#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_system.h>

#include <audio.h>
#include <input.h>

#include "yomogame.h"

// 1:ok   2:LED  3:JTAG_EN  4:ok  5:ok     6:ok     7:ok     8:ok     9:ok  10:ok
// 11:ok 12:ok  13:ok      14:ok 15:uart? 16:uart? 17:uart? 18:uart? 19:usb 20:usbpio pkg list
// 21:ok 35:psram 36:psram 37:psram 38:ok 39:jtag(ok) 40:jtag(ok) 41:jtag(ok) 42:jtag(ok) 43:usb 44:usb
// 45:vspi 46:log 47:ok 48:LED

const int ledPin = 48;

const int btnA = 1;
const int btnB = 4;
const int btnS = 5;
const int btnE = 6;
const int btnR = 7;
const int btnU = 8;
const int btnD = 9;
const int btnL = 21;
// const int btnRR = 13;
// const int btnLL = 42;
const int btnVolUp = 47;
const int btnVolDown = 45;

const int pinI2SBCLK = 39;
const int pinI2SLRC = 40;
const int pinI2SDIN = 41;

// -D TFT_CS=10
// -D TFT_MOSI=11
// -D TFT_SCLK=12
// -D TFT_MISO=-1
// -D TFT_DC=14
// -D TFT_RST=38

void setup() {
  // serial setting
  Serial.begin(115200);
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  // screen setting
  static TFT_eSPI screen;
  screen.init();
  screen.fillScreen(TFT_BLACK);

  static input::Buttons buttons(btnA, btnB, btnS, btnE, btnR, btnU, btnD, btnL);
  static input::VolumeButtons volume_buttons(btnVolUp, btnVolDown);
  static audio::Audio audio(pinI2SBCLK, pinI2SLRC, pinI2SDIN, volume_buttons);

  static yomogame::MainLoop mainloop(screen, audio, buttons);
  mainloop.run(); // never returns
}

void loop() {
  while (true) delay(100);
}
