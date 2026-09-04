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
#include "../frame_pusher.h"

extern "C" {
#define PEANUT_GB_HEADER_ONLY 1
#include "peanut_gb.h"
#include "minigb_apu.h"
void gb_bind_apu(struct minigb_apu_ctx* apu);
}

namespace yomogame {
namespace gb {

class GbScene;
static GbScene* g_active_gb = nullptr;

// Pick a .gb ROM from SD, then run Peanut-GB on a core-1 task. DMG only; the
// Game Boy Color palette is not emulated (DMG-compatible GBC games run in
// grayscale/green).
class GbScene : public Scene {
  public:
    void enter(Context& ctx) override {
      g_active_gb = this;
      screen_  = &ctx.screen;
      overlay_ = &ctx.overlay;
      buttons_ = &ctx.buttons;

      ctx.screen.fillScreen(TFT_BLACK);
      ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
      ctx.screen.setTextDatum(MC_DATUM);

      if (!SD.begin(SD_CS_PIN, TFT_eSPI::getSPIinstance(), 10000000)) {
        ctx.screen.drawString("SD MOUNT FAILED", 120, 120, 1);
        state_ = State::DEAD;
        return;
      }
      scan_roms();
      if (rom_count_ == 0) {
        ctx.screen.drawString("NO .gb ON SD", 120, 120, 1);
        state_ = State::DEAD;
        return;
      }
      selected_ = 0;
      dirty_ = true;
      prev_ = ctx.buttons.get();
      state_ = State::PICK;
    }

    Scene* tick(Context& ctx) override {
      if (state_ == State::PICK) tick_pick(ctx);
      return this;
    }

    void exit(Context& ctx) override {
      ctx.audio.set_source(nullptr, nullptr);
      g_active_gb = nullptr;
    }

    bool owns_overlay() override { return state_ == State::RUN; }

  private:
    static constexpr int SD_CS_PIN = 13;
    static constexpr int GBW = LCD_WIDTH;              // 160
    static constexpr int GBH = LCD_HEIGHT;             // 144
    static constexpr int OUTW = GBW * 3 / 2;           // 240 (1.5x, aspect-correct)
    static constexpr int OUTH = GBH * 3 / 2;           // 216
    static constexpr int OFFX = (240 - OUTW) / 2;      // 0
    static constexpr int OFFY = (240 - OUTH) / 2;      // 12
    static constexpr int kMaxRoms = 24;
    static constexpr int kMono = AUDIO_SAMPLES;        // per-frame mono samples
    static constexpr int kStereo = AUDIO_SAMPLES_TOTAL;

    enum class State { PICK, RUN, DEAD };
    State state_{State::DEAD};

    TFT_eSPI* screen_{nullptr};
    volui::VolumeOverlay* overlay_{nullptr};
    input::Buttons* buttons_{nullptr};

    char roms_[kMaxRoms][40];
    int rom_count_{0};
    int selected_{0};
    bool dirty_{true};
    input::ButtonState prev_{};

    struct gb_s gb_;
    struct minigb_apu_ctx apu_;
    uint8_t* rom_{nullptr};
    uint8_t* cart_ram_{nullptr};
    uint32_t rom_size_{0};
    uint32_t save_size_{0};
    char sav_path_[64]{};
    volatile bool sram_dirty_{false};
    volatile uint32_t sram_dirty_at_{0};
    uint32_t saved_hash_{0};
    uint16_t pal_[4]{};        // shade -> big-endian RGB565
    FramePusher pusher_;
    StreamBufferHandle_t audio_ring_{nullptr};
    TaskHandle_t emu_task_{nullptr};
    int16_t last_sample_{0};

    // APU scratch: kept off the emulator task's stack.
    int16_t apu_stereo_[kStereo];
    int16_t apu_mono_[kMono];

    /* ---- SD / PICK ---- */

    void scan_roms() {
      rom_count_ = 0;
      File root = SD.open("/");
      if (!root) return;
      for (File f = root.openNextFile(); f && rom_count_ < kMaxRoms; f = root.openNextFile()) {
        if (!f.isDirectory()) {
          String n = f.name();
          if (n.endsWith(".gb") || n.endsWith(".GB") || n.endsWith(".gbc") || n.endsWith(".GBC")) {
            strncpy(roms_[rom_count_], f.name(), sizeof(roms_[0]) - 1);
            roms_[rom_count_][sizeof(roms_[0]) - 1] = '\0';
            rom_count_++;
          }
        }
        f.close();
      }
      root.close();
    }

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

    /* ---- Peanut-GB callbacks (priv == this) ---- */

    static uint8_t cb_rom(struct gb_s* g, const uint_fast32_t a) {
      return static_cast<GbScene*>(g->direct.priv)->rom_[a];
    }
    static uint8_t cb_ram_r(struct gb_s* g, const uint_fast32_t a) {
      return static_cast<GbScene*>(g->direct.priv)->cart_ram_[a];
    }
    static void cb_ram_w(struct gb_s* g, const uint_fast32_t a, const uint8_t v) {
      GbScene* self = static_cast<GbScene*>(g->direct.priv);
      self->cart_ram_[a] = v;
      self->sram_dirty_ = true;
      self->sram_dirty_at_ = millis();
    }
    static void cb_err(struct gb_s*, const enum gb_error_e e, const uint16_t a) {
      Serial.printf("[gb] fatal error %d @ %04x\n", (int)e, a);
      for (;;) vTaskDelay(portMAX_DELAY);   // gb_error must not return
    }
    static void cb_line(struct gb_s* g, const uint8_t* px, const uint_fast8_t line) {
      GbScene* self = static_cast<GbScene*>(g->direct.priv);
      uint16_t* dst = self->pusher_.back() + (int)line * GBW;
      for (int x = 0; x < GBW; x++) dst[x] = self->pal_[px[x] & 3];
    }

    /* ---- audio source (runs on the core-0 audio task) ---- */

    static void audio_fill(void* ctx, int16_t* buf, int frames) {
      GbScene* self = static_cast<GbScene*>(ctx);
      size_t want = (size_t)frames * sizeof(int16_t);
      size_t got = self->audio_ring_
                     ? xStreamBufferReceive(self->audio_ring_, buf, want, 0) : 0;
      int n = got / sizeof(int16_t);
      int16_t hold = n > 0 ? buf[n - 1] : self->last_sample_;
      for (int i = n; i < frames; i++) buf[i] = hold;
      self->last_sample_ = buf[frames - 1];
    }

    /* ---- RUN ---- */

    static uint16_t mk565(uint8_t r, uint8_t g, uint8_t b) {
      uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      return (uint16_t)((c << 8) | (c >> 8));   // big-endian (matches FramePusher)
    }

    void start_emu(Context& ctx) {
      char path[64];
      snprintf(path, sizeof(path), "/%s", roms_[selected_]);

      File f = SD.open(path, FILE_READ);
      if (!f) { fail(ctx, "ROM OPEN FAILED"); return; }
      rom_size_ = f.size();
      // ROM is read on every opcode fetch: prefer fast internal RAM, fall back
      // to PSRAM for large carts.
      rom_ = (uint8_t*)heap_caps_malloc(rom_size_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!rom_) rom_ = (uint8_t*)heap_caps_malloc(rom_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!rom_) { f.close(); fail(ctx, "ROM ALLOC FAILED"); return; }
      f.read(rom_, rom_size_);
      f.close();

      {
        char title[17] = {0};
        for (int i = 0; i < 16; i++) { uint8_t c = rom_[0x134 + i]; title[i] = (c >= 32 && c < 127) ? c : ' '; }
        uint8_t cgb = rom_[0x143];   // 0x80 = CGB-enhanced, 0xC0 = CGB-only
        Serial.printf("[gb] '%s'  CGB=%02x (%s)  MBC=%02x  size=%u\n",
                      title, cgb,
                      cgb == 0xC0 ? "GBC-ONLY - not supported" :
                      cgb == 0x80 ? "GBC game, DMG mode"       : "DMG",
                      rom_[0x147], (unsigned)rom_size_);
        if (cgb == 0xC0) { fail(ctx, "GBC-ONLY ROM - DMG ONLY"); return; }
      }

      enum gb_init_error_e e = gb_init(&gb_, &cb_rom, &cb_ram_r, &cb_ram_w, &cb_err, this);
      if (e != GB_INIT_NO_ERROR) {
        Serial.printf("[gb] gb_init error %d\n", (int)e);
        fail(ctx, e == GB_INIT_CARTRIDGE_UNSUPPORTED ? "UNSUPPORTED CART" : "GB INIT FAILED");
        return;
      }

      save_size_ = gb_get_save_size(&gb_);
      cart_ram_ = (uint8_t*)heap_caps_malloc(save_size_ ? save_size_ : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (cart_ram_ && save_size_) memset(cart_ram_, 0, save_size_);

      // <rom>.sav next to the ROM: load prior save, arm autosave.
      if (save_size_) {
        make_sav_path(path);
        File sf = SD.open(sav_path_, FILE_READ);
        if (sf) {
          uint32_t n = sf.size() < save_size_ ? sf.size() : save_size_;
          sf.read(cart_ram_, n);
          sf.close();
          Serial.printf("[gb] loaded %s (%u bytes)\n", sav_path_, (unsigned)n);
        }
        saved_hash_ = hash(cart_ram_, save_size_);
      }

      gb_init_lcd(&gb_, &cb_line);
      minigb_apu_audio_init(&apu_);
      gb_bind_apu(&apu_);

      pal_[0] = mk565(0xE0, 0xF8, 0xD0);   // DMG green, lightest -> darkest
      pal_[1] = mk565(0x88, 0xC0, 0x70);
      pal_[2] = mk565(0x34, 0x68, 0x56);
      pal_[3] = mk565(0x08, 0x18, 0x20);

      audio_ring_ = xStreamBufferCreate(4096, 1);
      ctx.audio.set_source(&audio_fill, this);

      screen_->fillScreen(TFT_BLACK);   // wipe the ROM picker
      pusher_.begin(*screen_, overlay_, GBW, GBH, OUTW, OUTH, OFFX, OFFY);
      state_ = State::RUN;
      xTaskCreatePinnedToCore(&trampoline, "gb", 20480, this, 2, &emu_task_, 1);
    }

    void fail(Context& ctx, const char* msg) {
      ctx.screen.fillScreen(TFT_BLACK);
      ctx.screen.setTextDatum(MC_DATUM);
      ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
      ctx.screen.drawString(msg, 120, 120, 1);
      state_ = State::DEAD;
    }

    /* ---- battery save (<rom>.sav on SD) ---- */

    void make_sav_path(const char* rom_path) {
      strncpy(sav_path_, rom_path, sizeof(sav_path_) - 1);
      sav_path_[sizeof(sav_path_) - 1] = '\0';
      char* dot = strrchr(sav_path_, '.');
      char* slash = strrchr(sav_path_, '/');
      if (dot && (!slash || dot > slash)) *dot = '\0';
      strncat(sav_path_, ".sav", sizeof(sav_path_) - strlen(sav_path_) - 1);
    }

    static uint32_t hash(const uint8_t* p, uint32_t n) {
      uint32_t h = 2166136261u;                    // FNV-1a
      for (uint32_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
      return h;
    }

    // Runs on the emulator task. Writes cart RAM to SD only when it actually
    // changed and the game's write burst has settled (~1s of quiet). The SD
    // write shares the TFT SPI bus, so the display stutters for a few frames.
    void maybe_save() {
      if (!save_size_ || !sram_dirty_) return;
      if (millis() - sram_dirty_at_ < 1000) return;   // wait for the burst to finish
      sram_dirty_ = false;

      uint32_t h = hash(cart_ram_, save_size_);
      if (h == saved_hash_) return;                    // nothing new on disk

      SD.remove(sav_path_);                            // rewrite from scratch
      File f = SD.open(sav_path_, FILE_WRITE);
      if (!f) { Serial.printf("[gb] save open failed: %s\n", sav_path_); return; }
      size_t w = f.write(cart_ram_, save_size_);
      f.close();
      if (w == save_size_) { saved_hash_ = h; Serial.printf("[gb] saved %s\n", sav_path_); }
      else Serial.printf("[gb] save short write %u/%u\n", (unsigned)w, (unsigned)save_size_);
    }

    static void trampoline(void* p) { static_cast<GbScene*>(p)->emu_loop(); }

    void emu_loop() {
      for (;;) {
        // Rate-limit input polling to >=12ms so Button::read()'s 10ms debounce
        // always has a window, even when the loop runs fast (startup / after an
        // SD save drains the audio ring).
        uint32_t now = millis();
        if (now - btn_last_ms_ >= 12) { btn_cache_ = buttons_->get(); btn_last_ms_ = now; }
        const input::ButtonState& b = btn_cache_;

        uint8_t jp = 0xFF;
        if (b.A)      jp &= ~JOYPAD_A;
        if (b.B)      jp &= ~JOYPAD_B;
        if (b.SELECT) jp &= ~JOYPAD_SELECT;
        if (b.START)  jp &= ~JOYPAD_START;
        if (b.RIGHT)  jp &= ~JOYPAD_RIGHT;
        if (b.LEFT)   jp &= ~JOYPAD_LEFT;
        if (b.UP)     jp &= ~JOYPAD_UP;
        if (b.DOWN)   jp &= ~JOYPAD_DOWN;
        gb_.direct.joypad = jp;

        gb_run_frame(&gb_);
        pusher_.present();

        minigb_apu_audio_callback(&apu_, apu_stereo_);
        for (int i = 0; i < kMono; i++)
          apu_mono_[i] = (int16_t)(((int32_t)apu_stereo_[2 * i] + apu_stereo_[2 * i + 1]) / 2);
        // Blocking send: when the ring is full this paces the emulator to the
        // audio drain rate (= native Game Boy speed).
        xStreamBufferSend(audio_ring_, apu_mono_, sizeof(apu_mono_), portMAX_DELAY);

        maybe_save();

        // The ring-send above yields when the emulator is keeping up; this is
        // just WDT insurance for when it is not.
        if ((++yield_ctr_ & 7) == 0) vTaskDelay(1);
      }
    }
    uint32_t yield_ctr_{0};
    input::ButtonState btn_cache_{};
    uint32_t btn_last_ms_{0};
};

} // namespace gb
} // namespace yomogame
