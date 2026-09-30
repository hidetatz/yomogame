#pragma once

#include <SPI.h>

// The DSN emulator's compile-time sound path (built-in i2s_driver_install) is
// disabled: audio is bridged into audio::Audio via nes_audio_push() instead.
#define ENABLE_SOUND 0

// SD card has its own dedicated SPI bus on this board, separate from the LCD
// (TFT_eSPI owns the other hardware SPI peripheral via TFT_SCLK/TFT_MOSI).
#define SD_CS   38
#define SD_MISO 40
#define SD_SCK  41
#define SD_MOSI 39

// Lazily-initialised SPI bus for the SD card. Shared by nes_scene.h and
// gb_scene.h (whichever scene runs first performs the one-time begin()).
inline SPIClass& sd_spi_bus() {
  static SPIClass bus(HSPI);
  static bool started = false;
  if (!started) {
    bus.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    started = true;
  }
  return bus;
}
