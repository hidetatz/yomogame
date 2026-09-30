#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_system.h>

#include <audio.h>
#include <input.h>

#include "yomogame.h"

// Pin assignments, sourced from the yomogame custom PCB schematic
// (hardware/kicad/yomogame/yomogame.kicad_sch). GPIO0 (BOOT), EN, USB_D-/D+
// (native USB) and GPIO35-37 (octal PSRAM) are wired on the board but are not
// application GPIOs, so they don't appear below.

const int btnA = 16;
const int btnB = 15;
const int btnStart = 18;
const int btnSelect = 17;
const int btnRight = 7;
const int btnUp = 6;
const int btnDown = 5;
const int btnLeft = 4;
const int btnVolUp = 21;
const int btnVolDown = 47;

const int pinI2SBCLK = 46;
const int pinI2SLRC = 9;
const int pinI2SDIN = 3;

// LCD (TFT_eSPI) and SD pins are set via platformio.ini build_flags and
// src/nes/hw_config.h respectively, both cross-checked against the same
// schematic. Battery voltage (VBAT_SENSE, GPIO8) is read directly in
// yomogame.h's BatteryIndicator, next to the UI that displays it.

void setup() {
  // serial setting
  Serial.begin(115200);
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  // screen setting
  static TFT_eSPI screen;
  screen.init();
  digitalWrite(TFT_BL, LOW); // hide stale GRAM contents until cleared below
  screen.setRotation(1);
  screen.fillScreen(TFT_BLACK);
  digitalWrite(TFT_BL, HIGH);

  static input::Buttons buttons(btnA, btnB, btnStart, btnSelect, btnRight, btnUp, btnDown, btnLeft);
  static input::VolumeButtons volume_buttons(btnVolUp, btnVolDown);
  static audio::Audio audio(pinI2SBCLK, pinI2SLRC, pinI2SDIN, volume_buttons);

  static yomogame::MainLoop mainloop(screen, audio, buttons);
  mainloop.run(); // never returns
}

void loop() {
  while (true) delay(100);
}
