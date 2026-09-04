#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <TFT_eSPI.h>
#include <volume_overlay.h>

namespace yomogame {

// Double-buffered frame handoff for the emulators. The emulator task renders
// into back() (src_w x src_h) then calls present(); a dedicated core-0 task
// does the blocking panel push (optionally nearest-neighbour upscaling to
// dst_w x dst_h) plus the volume overlay, so the emulator never stalls on the
// display. Frames are dropped if the push can't keep up.
//
// Buffers hold big-endian RGB565 (pushed with setSwapBytes(false)).
class FramePusher {
  public:
    // Call once, on the task that will call present(). The whole panel is
    // cleared here so any border stays black.
    bool begin(TFT_eSPI& screen, volui::VolumeOverlay* overlay,
               int src_w, int src_h, int dst_w, int dst_h, int dst_x, int dst_y) {
      screen_ = &screen;
      overlay_ = overlay;
      sw_ = src_w; sh_ = src_h;
      dw_ = dst_w; dh_ = dst_h; x_ = dst_x; y_ = dst_y;
      scale_ = (sw_ != dw_) || (sh_ != dh_);

      size_t bytes = (size_t)sw_ * sh_ * sizeof(uint16_t);
      buf_[0] = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      buf_[1] = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!buf_[0] || !buf_[1]) return false;

      if (scale_) {
        if (dw_ > (int)(sizeof(xmap_) / sizeof(xmap_[0]))) return false;
        for (int ox = 0; ox < dw_; ox++) xmap_[ox] = (int16_t)(ox * sw_ / dw_);
      }

      sem_ = xSemaphoreCreateBinary();
      screen.fillScreen(TFT_BLACK);
      xTaskCreatePinnedToCore(trampoline, "fpush", 4096, this, 2, &task_, 0);
      return true;
    }

    // The buffer to render the next frame into (src_w x src_h).
    uint16_t* back() { return buf_[emu_idx_]; }

    // Hand the just-rendered back buffer to the display task and flip. Drops the
    // frame if the previous push is still running.
    void present() {
      if (!buf_[0] || busy_) return;
      disp_idx_ = emu_idx_;
      emu_idx_ ^= 1;
      busy_ = true;
      xSemaphoreGive(sem_);
    }

  private:
    static constexpr int kMaxDstW = 240;

    TFT_eSPI* screen_{nullptr};
    volui::VolumeOverlay* overlay_{nullptr};
    uint16_t* buf_[2]{nullptr, nullptr};
    int sw_{0}, sh_{0}, dw_{0}, dh_{0}, x_{0}, y_{0};
    bool scale_{false};
    int16_t xmap_[kMaxDstW];
    int emu_idx_{0};                 // touched only by the emulator task
    int disp_idx_{0};                // set by emu, read by disp (handoff via sem)
    volatile bool busy_{false};
    SemaphoreHandle_t sem_{nullptr};
    TaskHandle_t task_{nullptr};

    static void trampoline(void* p) { static_cast<FramePusher*>(p)->loop(); }

    void loop() {
      uint16_t row[kMaxDstW];
      for (;;) {
        xSemaphoreTake(sem_, portMAX_DELAY);
        const uint16_t* src = buf_[disp_idx_];
        screen_->setSwapBytes(false);

        if (!scale_) {
          screen_->pushImage(x_, y_, dw_, dh_, const_cast<uint16_t*>(src));
        } else {
          screen_->startWrite();
          screen_->setAddrWindow(x_, y_, dw_, dh_);
          for (int oy = 0; oy < dh_; oy++) {
            const uint16_t* srow = src + (oy * sh_ / dh_) * sw_;
            for (int ox = 0; ox < dw_; ox++) row[ox] = srow[xmap_[ox]];
            screen_->pushPixels(row, dw_);
          }
          screen_->endWrite();
        }

        if (overlay_) overlay_->tick(*screen_, TFT_BLACK);
        busy_ = false;
      }
    }
};

} // namespace yomogame
