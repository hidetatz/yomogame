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
#include "nes/nes_scene.h"
#include "gb/gb_scene.h"

namespace yomogame {

class SelectScene : public Scene {
  public:
    void enter(Context& ctx) override {
      dirty_ = true;
      prev_ = ctx.buttons.get();
    }

    Scene* tick(Context& ctx) override {
      if (dirty_) { draw(ctx); dirty_ = false; }

      input::ButtonState b = ctx.buttons.get();
      Scene* next = this;
      if (b.A && !prev_.A) {
        if      (selected_ == 0) next = new tetris::TetrisScene();
        else if (selected_ == 1) next = new nes::NesScene();
        else                     next = new gb::GbScene();
      }
      if (b.DOWN && !prev_.DOWN && selected_ < kCount - 1) { selected_++; dirty_ = true; }
      if (b.UP   && !prev_.UP   && selected_ > 0)          { selected_--; dirty_ = true; }
      prev_ = b;
      return next;
    }

  private:
    static constexpr int kCount = 3;
    static constexpr const char* kNames[kCount] = {"TETRIS", "NES", "GB"};

    input::ButtonState prev_{};
    int selected_{0};
    bool dirty_{true};

    void draw(Context& ctx) {
      TFT_eSPI& s = ctx.screen;
      s.fillScreen(TFT_BLACK);
      s.setTextDatum(MC_DATUM);
      s.setTextColor(TFT_CYAN, TFT_BLACK);
      s.drawString("SELECT GAME", 120, 30, 2);

      const int w = 150, h = 38, x = (240 - w) / 2;
      for (int i = 0; i < kCount; i++) {
        int y = 62 + i * 48;
        bool sel = i == selected_;
        s.drawRect(x, y, w, h, TFT_WHITE);
        s.fillRect(x + 1, y + 1, w - 2, h - 2, sel ? TFT_ORANGE : TFT_BLACK);
        s.setTextColor(sel ? TFT_BLACK : TFT_WHITE, sel ? TFT_ORANGE : TFT_BLACK);
        s.drawString(kNames[i], 120, y + h / 2, 2);
      }

      s.setTextColor(TFT_GREEN, TFT_BLACK);
      s.drawString("UP/DOWN + A", 120, 215, 1);
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
      overlay_(audio, 50, 10, 5, 230),
      ctx_{screen, audio, buttons, overlay_} {}

    void run() {
      ctx_.audio.begin(); // I2S bring-up on core 0
      while (!ctx_.audio.initialized()) delay(5);

      Scene* cur = new BootScene();
      cur->enter(ctx_);

      TickType_t last = xTaskGetTickCount();
      for (;;) {
        Scene* next = cur->tick(ctx_);
        if (!cur->owns_overlay()) overlay_.tick(ctx_.screen, TFT_BLACK);

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
    volui::VolumeOverlay overlay_;
    Context ctx_;
};

} // namespace yomogame
