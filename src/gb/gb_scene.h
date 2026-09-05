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

// Pick a .gb ROM from SD, then run Peanut-GB on a core-1 task. DMG only.
// GBC-enhanced carts (CGB=0x80) fall back to their own DMG-compatible mode
// and run fine in the fixed DMG-green palette below; CGB-only carts
// (CGB=0xC0) are rejected outright, since they rely on hardware this core
// doesn't emulate and would otherwise hang or glitch instead of failing
// cleanly.
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
      if (state_ == State::PICK) { if (Scene* s = tick_pick(ctx)) return s; }
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

    // gb_s holds WRAM/VRAM and is touched on every single emulated
    // instruction/pixel; it must live in fast internal RAM. As a plain
    // embedded member, GbScene's total size can cross ESP32 Arduino's
    // PSRAM-auto-routing threshold and land the whole object -- gb_s
    // included -- in slow PSRAM. Allocating it separately with
    // MALLOC_CAP_INTERNAL keeps it off PSRAM regardless of GbScene's size.
    struct gb_s* gb_{nullptr};
    struct minigb_apu_ctx apu_;
    uint8_t* rom_{nullptr};
    uint8_t* cart_ram_{nullptr};
    uint32_t rom_size_{0};
    uint32_t save_size_{0};
    char sav_path_[64]{};
    volatile bool sram_dirty_{false};
    volatile uint32_t sram_dirty_at_{0};
    volatile uint32_t sram_write_bytes_{0};   // writes in the current RAM-enabled window
    volatile uint8_t  sram_banks_touched_{0}; // bitmask of cart_ram_bank written this window
    bool prev_en_ram_{false};
    uint32_t saved_hash_{0};

    // A real in-game save writes a large burst (a good fraction of the cart's
    // SRAM, and/or to a bank other than 0) and then disables cart RAM. Smaller
    // bank-0-only bursts are scratch use (e.g. Pokemon Gen 1 sprite
    // decompression) and are not saved. The byte threshold scales with the
    // cart's save size (see save_burst_threshold()): fixed at 4096 for large
    // multi-bank saves (Pokemon), but proportionally smaller for carts with
    // little SRAM (e.g. MBC2's ~512 bytes), which would otherwise never clear
    // a fixed 4096-byte bar and always fall back to the 2-minute safety flush.
    static constexpr uint32_t kSaveBurstBytesMax = 4096;
    static constexpr uint32_t kSaveBurstBytesMin = 64;
    static constexpr uint32_t kSaveFallbackMs = 120000;   // last-resort flush

    uint32_t save_burst_threshold() const {
      uint32_t t = save_size_ / 4;
      if (t < kSaveBurstBytesMin) t = kSaveBurstBytesMin;
      if (t > kSaveBurstBytesMax) t = kSaveBurstBytesMax;
      if (t > save_size_) t = save_size_;   // never unreachable on tiny carts
      return t;
    }
    uint16_t pal_[4]{};        // DMG shade -> big-endian RGB565
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
      File root = SD.open("/gb");
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

      if (rom_count_ > 1) {
        qsort(roms_, rom_count_, sizeof(roms_[0]), [](const void* a, const void* b) {
          return strcasecmp((const char*)a, (const char*)b);
        });
      }
    }

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
      s.setTextColor(TFT_PURPLE, TFT_BLACK);
      s.drawString("SELECT ROM", 120, 10, 2);
      s.setTextDatum(ML_DATUM);   // middle-left: vertically centers each row's text in its bar
      int top = selected_ < 6 ? 0 : selected_ - 5;
      const int bar_h = 18;
      for (int i = 0; i < 6 && top + i < rom_count_; i++) {
        int idx = top + i;
        int y = 45 + i * 28;   // vertical center of this row
        bool sel = idx == selected_;
        s.fillRect(6, y - bar_h / 2, 228, bar_h, sel ? TFT_LIGHTGREY : TFT_BLACK);
        s.setTextColor(sel ? TFT_BLACK : TFT_LIGHTGREY, sel ? TFT_LIGHTGREY : TFT_BLACK);
        s.drawString(roms_[idx], 12, y, 1);
      }
      s.setTextDatum(TC_DATUM);
      s.setTextColor(TFT_PURPLE, TFT_BLACK);
      s.drawString("A: play   B: back", 120, 210, 2);
    }

    /* ---- Peanut-GB callbacks (priv == this) ----
     *
     * These run on the emulator task, cb_rom/cb_ram_r/cb_ram_w up to millions
     * of times per second (every ROM/RAM access from the CPU core, which is
     * itself built at -O3). This whole file, though, is pulled into the same
     * translation unit as tetris/audio (via yomogame.h) at the project's
     * default optimization -- a blanket #pragma GCC optimize here would leak
     * into that unrelated code exactly like the -O2-caused Tetris regression
     * we hit earlier, so these are opted in individually via the `optimize`
     * function attribute instead, which is scoped to just the function. */

    static uint8_t cb_rom(struct gb_s* g, const uint_fast32_t a) __attribute__((optimize("O2"))) {
      return static_cast<GbScene*>(g->direct.priv)->rom_[a];
    }
    static uint8_t cb_ram_r(struct gb_s* g, const uint_fast32_t a) __attribute__((optimize("O2"))) {
      return static_cast<GbScene*>(g->direct.priv)->cart_ram_[a];
    }
    static void cb_ram_w(struct gb_s* g, const uint_fast32_t a, const uint8_t v) __attribute__((optimize("O2"))) {
      GbScene* self = static_cast<GbScene*>(g->direct.priv);
      self->cart_ram_[a] = v;
      self->sram_dirty_ = true;
      self->sram_dirty_at_ = millis();
      if (self->sram_write_bytes_ < 0x40000) self->sram_write_bytes_++;
      self->sram_banks_touched_ |= (uint8_t)(1u << (g->cart_ram_bank & 7));
    }
    static void cb_err(struct gb_s*, const enum gb_error_e e, const uint16_t a) {
      Serial.printf("[gb] fatal error %d @ %04x\n", (int)e, a);
      for (;;) vTaskDelay(portMAX_DELAY);   // gb_error must not return
    }
    static void cb_line(struct gb_s* g, const uint8_t* px, const uint_fast8_t line) __attribute__((optimize("O2"))) {
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
      snprintf(path, sizeof(path), "/gb/%s", roms_[selected_]);

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

      gb_ = (struct gb_s*)heap_caps_malloc(sizeof(struct gb_s), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!gb_) { fail(ctx, "GB ALLOC FAILED"); return; }

      enum gb_init_error_e e = gb_init(gb_, &cb_rom, &cb_ram_r, &cb_ram_w, &cb_err, this);
      if (e != GB_INIT_NO_ERROR) {
        Serial.printf("[gb] gb_init error %d\n", (int)e);
        fail(ctx, e == GB_INIT_CARTRIDGE_UNSUPPORTED ? "UNSUPPORTED CART" : "GB INIT FAILED");
        return;
      }

      save_size_ = gb_get_save_size(gb_);
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

      gb_init_lcd(gb_, &cb_line);
      minigb_apu_audio_init(&apu_);
      gb_bind_apu(&apu_);

      pal_[0] = mk565(0xE0, 0xF8, 0xD0);   // DMG green, lightest -> darkest
      pal_[1] = mk565(0x88, 0xC0, 0x70);
      pal_[2] = mk565(0x34, 0x68, 0x56);
      pal_[3] = mk565(0x08, 0x18, 0x20);

      // ~185ms of slack @ 22050Hz mono (same size NES uses): absorbs the emu
      // task's occasional heavier frames (ROM bank switches) without an
      // audible underrun, without adding much input-to-audio lag.
      audio_ring_ = xStreamBufferCreate(8192, 1);
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

    // Runs on the emulator task, once per frame. Persists cart RAM to SD only
    // when the game finishes a real save: it disables cart RAM after a large
    // write burst (several KB, or any bank past 0). Bank-0-only scratch bursts
    // are ignored. A 2-minute fallback covers games that never toggle the
    // enable bit. The SD write shares the TFT SPI bus (brief display stutter).
    void poll_save() {
      if (!save_size_) return;

      const bool en = gb_->enable_cart_ram;
      if (en && !prev_en_ram_) {                 // RAM-enabled window opened
        sram_write_bytes_ = 0;
        sram_banks_touched_ = 0;
      } else if (!en && prev_en_ram_) {          // window closed -> maybe a save
        const bool looks_like_save = (sram_banks_touched_ & ~1u) != 0 ||
                                     sram_write_bytes_ >= save_burst_threshold();
        if (looks_like_save) commit_save("in-game save");
      }
      prev_en_ram_ = en;

      if (sram_dirty_ && millis() - sram_dirty_at_ > kSaveFallbackMs)
        commit_save("fallback");
    }

    void commit_save(const char* why) {
      sram_dirty_ = false;
      uint32_t h = hash(cart_ram_, save_size_);
      if (h == saved_hash_) return;                    // nothing new on disk

      SD.remove(sav_path_);                            // rewrite from scratch
      File f = SD.open(sav_path_, FILE_WRITE);
      if (!f) { Serial.printf("[gb] save open failed: %s\n", sav_path_); return; }
      size_t w = f.write(cart_ram_, save_size_);
      f.close();
      if (w == save_size_) { saved_hash_ = h; Serial.printf("[gb] saved %s (%s)\n", sav_path_, why); }
      else Serial.printf("[gb] save short write %u/%u\n", (unsigned)w, (unsigned)save_size_);
    }

    static void trampoline(void* p) { static_cast<GbScene*>(p)->emu_loop(); }

    void emu_loop() __attribute__((optimize("O2"))) {
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
        gb_->direct.joypad = jp;

        gb_run_frame(gb_);
        pusher_.present();

        minigb_apu_audio_callback(&apu_, apu_stereo_);
        for (int i = 0; i < kMono; i++)
          apu_mono_[i] = (int16_t)(((int32_t)apu_stereo_[2 * i] + apu_stereo_[2 * i + 1]) / 2);
        // Blocking send: when the ring is full this paces the emulator to the
        // audio drain rate (= native Game Boy speed).
        xStreamBufferSend(audio_ring_, apu_mono_, sizeof(apu_mono_), portMAX_DELAY);

        poll_save();

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
