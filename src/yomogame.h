#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>
#include <volume_overlay.h>
#include <yomogi.h>

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
      // No TFT_eSPI built-in brown is light enough for text on black, so
      // these are hand-picked rather than named constants.
      uint16_t light_brown = s.color565(200, 140, 90);
      uint16_t dark_brown  = s.color565(90, 50, 20);

      s.fillScreen(TFT_BLACK);
      s.setTextDatum(MC_DATUM);
      s.setTextColor(light_brown, TFT_BLACK);
      s.drawString("SELECT GAME", 120, 30, 2);

      const int w = 150, h = 38, x = (240 - w) / 2;
      for (int i = 0; i < kCount; i++) {
        int y = 62 + i * 48;
        bool sel = i == selected_;
        s.drawRect(x, y, w, h, TFT_WHITE);
        s.fillRect(x + 1, y + 1, w - 2, h - 2, sel ? dark_brown : TFT_BLACK);
        s.setTextColor(TFT_WHITE, sel ? dark_brown : TFT_BLACK);
        s.drawString(kNames[i], 120, y + h / 2, 2);
      }

      s.setTextColor(light_brown, TFT_BLACK);
      s.drawString("UP/DOWN + A", 120, 215, 1);
    }
};

inline Scene* make_select_scene() { return new SelectScene(); }

// Logo screen: the yomogi.h mascot image, "YomoGame" below it, and a short
// Game-Boy-style startup chime (converted offline from sound_raw/pikororon.mp3
// to data/boot/pikororon.raw -- 22050Hz mono 16-bit PCM, same format as
// tetris's sound assets). Shown for a fixed 1.5s before the game-select screen.
class BootScene : public Scene {
  public:
    void enter(Context& ctx) override {
      started_ = millis();

      TFT_eSPI& s = ctx.screen;
      s.fillScreen(TFT_BLACK);

      const int logo_x = (240 - YOMOGI_240X240_WIDTH) / 2;
      const int logo_y = 70;
      s.setSwapBytes(true);   // matches tetris_rendering.h's use of the same image data
      s.pushImage(logo_x, logo_y, YOMOGI_240X240_WIDTH, YOMOGI_240X240_HEIGHT,
                  yomogi_240x240, YOMOGI_240X240_TRANSPARENT);

      uint16_t light_brown = s.color565(200, 140, 90);
      s.setTextDatum(MC_DATUM);
      s.setTextColor(light_brown, TFT_BLACK);
      s.drawString("YomoGame", 120, logo_y + YOMOGI_240X240_HEIGHT + 35, 4);

      play_chime(ctx);
    }

    Scene* tick(Context& ctx) override {
      if (millis() - started_ >= 2000) {
        ctx.audio.set_source(nullptr, nullptr);
        return new SelectScene();
      }
      return this;
    }

    void exit(Context& ctx) override {
      ctx.audio.set_source(nullptr, nullptr);
      if (chime_) { free(chime_); chime_ = nullptr; }
    }

  private:
    unsigned long started_{0};

    int16_t* chime_{nullptr};
    uint32_t chime_len_{0};      // samples
    volatile uint32_t chime_pos_{0};

    void play_chime(Context& ctx) {
      if (!LittleFS.begin(false)) {
        Serial.println("[boot] LittleFS mount failed");
        return;
      }
      File f = LittleFS.open("/boot/pikororon.raw", "r");
      if (!f) {
        Serial.println("[boot] pikororon.raw not found");
        return;
      }
      size_t bytes = f.size();
      chime_ = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!chime_) { f.close(); return; }
      f.read((uint8_t*)chime_, bytes);
      f.close();
      chime_len_ = bytes / 2;
      chime_pos_ = 0;
      ctx.audio.set_source(&fill_trampoline, this);
    }

    static void fill_trampoline(void* ctx, int16_t* buf, int frames) {
      static_cast<BootScene*>(ctx)->fill(buf, frames);
    }

    // Runs on the core-0 audio task. Plays the chime once, then silence.
    void fill(int16_t* buf, int frames) {
      uint32_t pos = chime_pos_;
      int n = 0;
      if (chime_ && pos < chime_len_) {
        uint32_t avail = chime_len_ - pos;
        n = (uint32_t)frames < avail ? frames : (int)avail;
        memcpy(buf, chime_ + pos, n * sizeof(int16_t));
        chime_pos_ = pos + n;
      }
      for (int i = n; i < frames; i++) buf[i] = 0;
    }
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
