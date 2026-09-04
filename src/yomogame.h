#pragma once

#include <Arduino.h>

#include <TFT_eSPI.h>

#include "audio.h"
#include "input.h"
#include "volume_overlay.h"

namespace yomogame {

  struct Context {
    TFT_eSPI& screen;
    audio::Player& audio;
    input::Buttons& buttons;
  };

  class Scene {
    public:
      virtual ~Scene() = default;
      virtual void enter(Context&) {}
      virtual Scene* tick(Context&) = 0;
      virtual void exit(Context&) {}
  };

  class YomoGameMenuScene : public Scene {
    public:
      void enter(Context& ctx) override {}

      Scene* tick(Context& ctx) override {
        ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
        ctx.screen.setTextDatum(TC_DATUM);
        ctx.screen.drawString("menu!", 120, 120, 2);
        return this;
      }

      void exit(Context& ctx) override {}
  };

  class YomoGameBootScene : public Scene {
    public:
      void enter(Context& ctx) override {
        display_started_at = millis();
      }

      Scene* tick(Context& ctx) override {
        unsigned long now = millis();
        if (now - display_started_at < 1500) {
          ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
          ctx.screen.setTextDatum(TC_DATUM);
          ctx.screen.drawString("booting...", 120, 120, 2);
          return this;
        }

        return new YomoGameMenuScene();
      }

      void exit(Context& ctx) override {}

    private:
      unsigned long display_started_at{0};
  };
  
  class YomoGame {
    public:
      YomoGame(TFT_eSPI& screen, audio::Player& audio, input::Buttons& buttons) :
        ctx{screen, audio, buttons} {}

      void start() {
        ctx.audio.begin(); // start audio play loop thread on core 0
        volui::VolumeOverlay overlay(ctx.audio, 60, 10, 10, 230);

        Scene* cur_scene = new YomoGameBootScene();
        cur_scene->enter(ctx);

        TickType_t last = xTaskGetTickCount();

        while (true) {
          Scene* next = cur_scene->tick(ctx);

          overlay.tick(ctx.screen, TFT_BLACK);

          // next will never be nullptr
          if (next != cur_scene) {
            cur_scene->exit(ctx);
            delete cur_scene;
            cur_scene = next;
            ctx.screen.fillScreen(TFT_BLACK);
            cur_scene->enter(ctx);
          }

          vTaskDelayUntil(&last, pdMS_TO_TICKS(16));
        }
      }
      
    private:
      Context ctx;
  };
  
} // namespace yomogame
