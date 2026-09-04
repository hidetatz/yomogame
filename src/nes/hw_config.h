#pragma once

// The DSN emulator's compile-time sound path (built-in i2s_driver_install) is
// disabled: audio is bridged into audio::Audio via nes_audio_push() instead.
#define ENABLE_SOUND 0

// SD card shares the TFT SPI bus (SCK 12 / MOSI 11); it only adds its own CS and
// a MISO line (the TFT is write-only, TFT_MISO=-1).
#define SD_CS   13
#define SD_MISO 42
#define SD_SCK  12   // shared with TFT
#define SD_MOSI 11   // shared with TFT
