#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <stdlib.h>
#include <strings.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>
#include <volume_overlay.h>

#include "../scene.h"
#include "../frame_pusher.h"
#include "hw_config.h"
#include "nes_bridge.h"

// from the emulator core / bridge
extern "C" int nofrendo_main(int argc, char* argv[]);
void nes_bind_buttons(input::Buttons* b);

namespace yomogame {
namespace nes {

class NesScene;
static NesScene* g_active_nes = nullptr;

// A single scene: pick a ROM off the SD card, then run Nofrendo on a dedicated
// core-1 task. The emulator owns rendering/audio production; this scene wires it
// to the shell's screen, audio engine and volume overlay.
class NesScene : public Scene {
  public:
    void enter(Context& ctx) override {
      g_active_nes = this;
      screen_  = &ctx.screen;
      overlay_ = &ctx.overlay;
      nes_bind_buttons(&ctx.buttons);

      ctx.screen.fillScreen(TFT_BLACK);
      ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
      ctx.screen.setTextDatum(MC_DATUM);

      if (!mount_sd()) {
        ctx.screen.drawString("SD MOUNT FAILED", 120, 120, 1);
        state_ = State::DEAD;
        return;
      }
      scan_roms();
      if (rom_count_ == 0) {
        ctx.screen.drawString("NO .nes ON SD", 120, 120, 1);
        state_ = State::DEAD;
        return;
      }

      selected_ = 0;
      dirty_ = true;
      prev_ = ctx.buttons.get();
      state_ = State::PICK;
    }

    Scene* tick(Context& ctx) override {
      switch (state_) {
        case State::PICK: if (Scene* s = tick_pick(ctx)) return s; break;
        case State::RUN:  break;   // emulator task owns everything
        case State::DEAD: break;
      }
      return this;
    }

    void exit(Context& ctx) override {
      // Only reachable from PICK (B returns to the game-select screen); once
      // RUN starts the emulator task, this scene is never exited -- that task
      // cannot be safely stopped, so tear it down here first if that changes.
      ctx.audio.set_source(nullptr, nullptr);
      g_active_nes = nullptr;
    }

    bool owns_overlay() override { return state_ == State::RUN; }

    // --- bridge entry points (called from osd.cpp on the emulator task) ---

    uint16_t* emu_fb() { return pusher_.back(); }
    void blit(const uint16_t*) { pusher_.present(); }

    void audio_push(const int16_t* mono, int samples) {
      if (audio_ring_) {
        xStreamBufferSend(audio_ring_, mono, (size_t)samples * sizeof(int16_t), pdMS_TO_TICKS(20));
      }
    }

    void post_frame() {}   // overlay is drawn inside blit() (DMA ordering)

  private:
    enum class State { PICK, RUN, DEAD };

    static constexpr int kMaxRoms = 24;

    State state_{State::DEAD};
    TFT_eSPI* screen_{nullptr};
    volui::VolumeOverlay* overlay_{nullptr};

    char roms_[kMaxRoms][40];
    int rom_count_{0};
    int selected_{0};
    bool dirty_{true};
    input::ButtonState prev_{};

    char rom_path_[64];
    TaskHandle_t emu_task_{nullptr};
    StreamBufferHandle_t audio_ring_{nullptr};
    audio::Audio* audio_{nullptr};
    FramePusher pusher_;

    /* ---- SD ---- */

    bool mount_sd() {
      // Share TFT_eSPI's SPI instance (same FSPI bus, MISO already attached on
      // GPIO42 via -D TFT_MISO=42). SPIClass::beginTransaction serialises SD and
      // TFT access; in practice they never overlap (ROM is read fully into PSRAM
      // before the first frame, SD untouched afterwards).
      return SD.begin(SD_CS, TFT_eSPI::getSPIinstance(), 10000000);
    }

    void scan_roms() {
      rom_count_ = 0;
      File root = SD.open("/");
      if (!root) return;
      for (File f = root.openNextFile(); f && rom_count_ < kMaxRoms; f = root.openNextFile()) {
        if (!f.isDirectory()) {
          String n = f.name();
          if (n.endsWith(".nes") || n.endsWith(".NES")) {
            strncpy(roms_[rom_count_], f.name(), sizeof(roms_[0]) - 1);
            roms_[rom_count_][sizeof(roms_[0]) - 1] = '\0';
            rom_count_++;
          }
        }
        f.close();
      }
      root.close();

      if (rom_count_ > 1) {
        qsort(roms_, rom_count_, sizeof(roms_[0]), [](const void* a, const void* b) {
          return strcasecmp((const char*)a, (const char*)b);
        });
      }
    }

    /* ---- PICK ---- */

    // Returns non-null to switch away (back to the game-select screen).
    Scene* tick_pick(Context& ctx) {
      if (dirty_) { draw_pick(ctx); dirty_ = false; }

      input::ButtonState b = ctx.buttons.get();
      if (b.A && !prev_.A) { start_emu(ctx); prev_ = b; return nullptr; }
      if (b.B && !prev_.B) { return make_select_scene(); }
      if (b.DOWN && !prev_.DOWN && selected_ < rom_count_ - 1) { selected_++; dirty_ = true; }
      if (b.UP   && !prev_.UP   && selected_ > 0)              { selected_--; dirty_ = true; }
      prev_ = b;
      return nullptr;
    }

    void draw_pick(Context& ctx) {
      TFT_eSPI& s = ctx.screen;
      s.fillScreen(TFT_BLACK);
      s.setTextDatum(TC_DATUM);
      s.setTextColor(TFT_GOLD, TFT_BLACK);
      s.drawString("SELECT ROM", 120, 10, 2);

      s.setTextDatum(ML_DATUM);   // middle-left: vertically centers each row's text in its bar
      int top = selected_ < 6 ? 0 : selected_ - 5;
      const int bar_h = 18;
      for (int i = 0; i < 6 && top + i < rom_count_; i++) {
        int idx = top + i;
        int y = 45 + i * 28;   // vertical center of this row
        bool sel = idx == selected_;
        s.fillRect(6, y - bar_h / 2, 228, bar_h, sel ? TFT_MAROON : TFT_BLACK);
        s.setTextColor(TFT_WHITE, sel ? TFT_MAROON : TFT_BLACK);
        s.drawString(roms_[idx], 12, y, 1);
      }
      s.setTextDatum(TC_DATUM);
      s.setTextColor(TFT_GOLD, TFT_BLACK);
      s.drawString("A: play   B: back", 120, 210, 2);
    }

    /* ---- RUN ---- */

    void start_emu(Context& ctx) {
      snprintf(rom_path_, sizeof(rom_path_), "/%s", roms_[selected_]);

      audio_ = &ctx.audio;
      audio_ring_ = xStreamBufferCreate(8192, 1);   // ~185ms slack @22050
      ctx.audio.set_source(&NesScene::audio_fill, this);

      pusher_.begin(*screen_, overlay_, 240, 240, 240, 240, 0, 0);   // core-0 display task, no scaling
      state_ = State::RUN;

      // priority 2 so the emulator preempts the shell/volume tasks; it yields
      // via vTaskDelay(1) each frame in osd.cpp vid_flush.
      xTaskCreatePinnedToCore(&NesScene::emu_trampoline, "nes", 16384, this, 2, &emu_task_, 1);
    }

    static void emu_trampoline(void* param) {
      NesScene* self = static_cast<NesScene*>(param);
      char* argv[] = { (char*)"nes", (char*)"-sound", self->rom_path_ };
      nofrendo_main(3, argv);   // never returns
      vTaskDelete(nullptr);
    }

    // Runs on the core-0 audio task: drain the ring. On underrun hold the last
    // sample (softer than a hard drop to zero).
    static void audio_fill(void* ctx, int16_t* buf, int frames) {
      NesScene* self = static_cast<NesScene*>(ctx);
      size_t want = (size_t)frames * sizeof(int16_t);
      size_t got = self->audio_ring_
                     ? xStreamBufferReceive(self->audio_ring_, buf, want, 0)
                     : 0;
      int n = got / sizeof(int16_t);
      int16_t hold = n > 0 ? buf[n - 1] : self->last_sample_;
      for (int i = n; i < frames; i++) buf[i] = hold;
      self->last_sample_ = buf[frames - 1];
    }

    int16_t last_sample_{0};
};

} // namespace nes
} // namespace yomogame

// --- C bridge implementations (single TU: pulled in via yomogame.h) ---

extern "C" uint16_t* nes_framebuffer(void) {
  return yomogame::nes::g_active_nes ? yomogame::nes::g_active_nes->emu_fb() : nullptr;
}
extern "C" void nes_blit_frame(const uint16_t* fb) {
  if (yomogame::nes::g_active_nes) yomogame::nes::g_active_nes->blit(fb);
}
extern "C" void nes_audio_push(const int16_t* mono, int samples) {
  if (yomogame::nes::g_active_nes) yomogame::nes::g_active_nes->audio_push(mono, samples);
}
extern "C" void nes_post_frame(void) {
  if (yomogame::nes::g_active_nes) yomogame::nes::g_active_nes->post_frame();
}
