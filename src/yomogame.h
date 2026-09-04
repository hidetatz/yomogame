#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>
#include <volume_overlay.h>

#include "scene.h"
#include "tetris/tetris.h"

namespace yomogame {

class SelectScene : public Scene {
  public:
    void enter(Context& ctx) override {
      draw(ctx);
      prev_ = ctx.buttons.get();
    }

    Scene* tick(Context& ctx) override {
      input::ButtonState b = ctx.buttons.get();
      Scene* next = this;
      if (b.A && !prev_.A) next = new tetris::TetrisScene();
      prev_ = b;
      return next;
    }

  private:
    input::ButtonState prev_{};

    void draw(Context& ctx) {
      ctx.screen.fillScreen(TFT_BLACK);
      ctx.screen.setTextDatum(MC_DATUM);

      ctx.screen.setTextColor(TFT_CYAN, TFT_BLACK);
      ctx.screen.drawString("SELECT GAME", 120, 55, 2);

      const int x = 55, y = 100, w = 130, h = 40;
      ctx.screen.drawRect(x, y, w, h, TFT_WHITE);
      ctx.screen.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_ORANGE);
      ctx.screen.setTextColor(TFT_BLACK, TFT_ORANGE);
      ctx.screen.drawString("TETRIS", 120, y + h / 2, 2);

      ctx.screen.setTextColor(TFT_GREEN, TFT_BLACK);
      ctx.screen.drawString("PRESS A", 120, 175, 2);
    }
};

class BootScene : public Scene {
  public:
    void enter(Context& ctx) override {
      started_ = millis();
      ctx.screen.setTextDatum(MC_DATUM);
      ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
      ctx.screen.drawString("YomoGame", 120, 120, 4);
    }

    Scene* tick(Context& ctx) override {
      if (millis() - started_ >= 1000) return new SelectScene();
      return this;
    }

  private:
    unsigned long started_{0};
};

class MainLoop {
  public:
    MainLoop(TFT_eSPI& screen, audio::Audio& audio, input::Buttons& buttons) :
      ctx_{screen, audio, buttons},
      overlay_(audio, 50, 10, 5, 230) {}

    void run() {
      ctx_.audio.begin(); // I2S bring-up on core 0
      while (!ctx_.audio.initialized()) delay(5);

      Scene* cur = new BootScene();
      cur->enter(ctx_);

      TickType_t last = xTaskGetTickCount();
      for (;;) {
        Scene* next = cur->tick(ctx_);
        overlay_.tick(ctx_.screen, TFT_BLACK);

        if (next != cur) {
          cur->exit(ctx_);
          delete cur;
          ctx_.screen.fillScreen(TFT_BLACK);
          cur = next;
          cur->enter(ctx_);
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(16));
        vTaskDelay(1);
      }
    }

  private:
    Context ctx_;
    volui::VolumeOverlay overlay_;
};

} // namespace yomogame
