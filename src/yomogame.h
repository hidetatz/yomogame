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

// VBAT_SENSE is a 100k/100k divider off VSYS_SW (hardware/circuit-design.md
// sec 1.3), read on this ADC1-capable pin. Must match main.cpp's pinVbatSense.
const int pinVbatSense = 8;

// Battery percentage readout shown in the corner of the game-select screen.
// Redraws into its own sprite so the periodic re-read doesn't flicker the
// rest of the menu.
class BatteryIndicator {
  public:
    void tick(TFT_eSPI& s, bool force = false) {
      if (!adc_ready_) {
        analogReadResolution(12);
        analogSetPinAttenuation(pinVbatSense, ADC_11db);
        adc_ready_ = true;
      }

      unsigned long now = millis();
      bool due = !sprite_ || now - last_read_ >= 3000;
      if (due) { last_read_ = now; last_pct_ = sample_percent(); }
      else if (!force) return; // cached value hasn't changed; menu didn't just redraw over us

      if (!sprite_) {
        sprite_ = new TFT_eSprite(&s);
        sprite_->createSprite(kWidth, kHeight);
      }
      draw(last_pct_);
      sprite_->pushSprite(kX, kY);
    }

  private:
    static constexpr int kWidth = 56, kHeight = 14;
    static constexpr int kX = 240 - kWidth - 4, kY = 4; // top-right corner
    static constexpr int kSamples = 16; // oversample to smooth out ADC noise

    TFT_eSprite* sprite_{nullptr};
    bool adc_ready_{false};
    unsigned long last_read_{0};
    int last_pct_{0};

    int sample_percent() {
      long sum = 0;
      for (int i = 0; i < kSamples; i++) sum += analogRead(pinVbatSense);
      float raw_avg = sum / (float)kSamples;
      float v_bat = (raw_avg / 4095.0f * 3.3f) * 2.0f; // undo the 100k/100k divider
      return (int)constrain((v_bat - 3.3f) / (4.2f - 3.3f) * 100.0f, 0.0f, 100.0f);
    }

    void draw(int pct) {
      sprite_->fillSprite(TFT_BLACK);

      const int body_w = 20, body_h = 10, body_x = 0, body_y = (kHeight - body_h) / 2;
      sprite_->drawRect(body_x, body_y, body_w, body_h, TFT_WHITE);
      sprite_->fillRect(body_x + body_w, body_y + 3, 2, body_h - 6, TFT_WHITE); // terminal nub

      int fill_w = (body_w - 4) * pct / 100;
      uint16_t fill_color = pct <= 20 ? TFT_RED : TFT_GREEN;
      if (fill_w > 0) sprite_->fillRect(body_x + 2, body_y + 2, fill_w, body_h - 4, fill_color);

      char buf[6];
      snprintf(buf, sizeof(buf), "%d%%", pct);
      sprite_->setTextDatum(ML_DATUM);
      sprite_->setTextColor(TFT_WHITE, TFT_BLACK);
      sprite_->drawString(buf, body_x + body_w + 6, kHeight / 2, 1);
    }
};

class SelectScene : public Scene {
  public:
    void enter(Context& ctx) override {
      dirty_ = true;
      prev_ = ctx.buttons.get();
    }

    Scene* tick(Context& ctx) override {
      bool just_redrew = dirty_;
      if (dirty_) { draw(ctx); dirty_ = false; }
      battery_.tick(ctx.screen, just_redrew);

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
    BatteryIndicator battery_;
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
