#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>
#include <volume_overlay.h>

#include "../scene.h"
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
        case State::PICK: tick_pick(ctx); break;
        case State::RUN:  break;   // emulator task owns everything
        case State::DEAD: break;
      }
      return this;
    }

    void exit(Context& ctx) override {
      // The emulator task cannot be safely stopped; this scene is never exited
      // in the current flow. If that changes, tear the task down here first.
      ctx.audio.set_source(nullptr, nullptr);
      g_active_nes = nullptr;
    }

    bool owns_overlay() override { return state_ == State::RUN; }

    // --- bridge entry points (called from osd.cpp on the emulator task) ---

    // The buffer osd_blit renders into this frame.
    uint16_t* emu_fb() { return fb_[emu_idx_]; }

    // Called on the emulator task once the frame is in fb_[emu_idx_]. Hand it to
    // the display task and flip to the other buffer. Drop the frame if the
    // previous push is still running.
    void blit(const uint16_t*) {
      if (!fb_[0] || !fb_[1] || disp_busy_) return;
      disp_idx_ = emu_idx_;
      emu_idx_ ^= 1;
      disp_busy_ = true;
      xSemaphoreGive(frame_sem_);
    }

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

    uint16_t* fb_[2]{nullptr, nullptr};
    int emu_idx_{0};                 // touched only by the emulator task
    int disp_idx_{0};                // set by emu, read by disp (handoff via sem)
    volatile bool disp_busy_{false};
    SemaphoreHandle_t frame_sem_{nullptr};
    TaskHandle_t disp_task_{nullptr};

    static void disp_trampoline(void* p) { static_cast<NesScene*>(p)->disp_loop(); }

    void disp_loop() {
      for (;;) {
        xSemaphoreTake(frame_sem_, portMAX_DELAY);
        screen_->setSwapBytes(false);
        screen_->pushImage(0, 0, 240, 240, fb_[disp_idx_]);
        if (overlay_) overlay_->tick(*screen_, TFT_BLACK);
        disp_busy_ = false;
      }
    }

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
    }

    /* ---- PICK ---- */

    void tick_pick(Context& ctx) {
      if (dirty_) { draw_pick(ctx); dirty_ = false; }

      input::ButtonState b = ctx.buttons.get();
      if (b.A && !prev_.A) { start_emu(ctx); prev_ = b; return; }
      if (b.DOWN && !prev_.DOWN && selected_ < rom_count_ - 1) { selected_++; dirty_ = true; }
      if (b.UP   && !prev_.UP   && selected_ > 0)              { selected_--; dirty_ = true; }
      prev_ = b;
    }

    void draw_pick(Context& ctx) {
      TFT_eSPI& s = ctx.screen;
      s.fillScreen(TFT_BLACK);
      s.setTextDatum(TC_DATUM);
      s.setTextColor(TFT_CYAN, TFT_BLACK);
      s.drawString("SELECT ROM", 120, 10, 2);

      s.setTextDatum(TL_DATUM);
      int top = selected_ < 6 ? 0 : selected_ - 5;
      for (int i = 0; i < 6 && top + i < rom_count_; i++) {
        int idx = top + i;
        int y = 45 + i * 28;
        bool sel = idx == selected_;
        s.fillRect(6, y - 3, 228, 24, sel ? TFT_ORANGE : TFT_BLACK);
        s.setTextColor(sel ? TFT_BLACK : TFT_WHITE, sel ? TFT_ORANGE : TFT_BLACK);
        s.drawString(roms_[idx], 12, y, 1);
      }
      s.setTextDatum(TC_DATUM);
      s.setTextColor(TFT_GREEN, TFT_BLACK);
      s.drawString("A: play", 120, 220, 1);
    }

    /* ---- RUN ---- */

    void start_emu(Context& ctx) {
      snprintf(rom_path_, sizeof(rom_path_), "/%s", roms_[selected_]);

      audio_ = &ctx.audio;
      audio_ring_ = xStreamBufferCreate(8192, 1);   // ~185ms slack @22050
      ctx.audio.set_source(&NesScene::audio_fill, this);

      // Display task on core 0 does the ~27ms blocking frame push so the
      // emulator (core 1) never stalls on it. Double-buffered so the emulator
      // renders one frame while the other is being pushed (no copy).
      fb_[0] = (uint16_t*)heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      fb_[1] = (uint16_t*)heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      frame_sem_ = xSemaphoreCreateBinary();
      xTaskCreatePinnedToCore(&NesScene::disp_trampoline, "nes_disp", 4096, this, 2, &disp_task_, 0);

      ctx.screen.fillScreen(TFT_BLACK);
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
