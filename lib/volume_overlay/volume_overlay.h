#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <audio.h>

namespace volui {

class VolumeOverlay {
  public:
    int width;
    int height;
    int x;
    int y;

    VolumeOverlay(audio::Player& player, int width, int height, int x, int y) : player(player), width(width), height(height), x(x), y(y) {}

    void tick(TFT_eSPI& tft, uint16_t clear_color) {
      uint32_t seq = player.volume_generation();
      if (seq != last_seq) { last_seq = seq; visible_until = millis() + 1200; }

      bool visible = (int32_t)(millis() - visible_until) < 0;
      if (visible)    { draw(tft); shown = true; }
      else if (shown) { tft.fillRect(x, y, width, height, clear_color); shown = false; }
    }

  private:
    audio::Player& player;

    uint32_t last_seq = 0;
    uint32_t visible_until = 0;
    bool shown = false;

    void draw(TFT_eSPI& tft) {
      int max_vol = player.max_volume();
      int vol = player.current_volume();
      tft.fillRect(x, y, width, height, TFT_DARKGREY);
      tft.drawRect(x, y, width, height, TFT_LIGHTGREY);
      int filled = (width - 4) * vol / max_vol;
      tft.fillRect(x + 2, y + 2, filled, height - 4, TFT_BLUE);
    }
};

} // namespace volui
