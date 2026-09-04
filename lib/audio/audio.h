#pragma once

#include "input.h"

#include <atomic>
#include <algorithm>
#include <string.h>

#include <Arduino.h>
#include <driver/i2s.h>
#include <Preferences.h>

namespace audio {

class Audio {
  public:
    using SourceFn = void (*)(void* ctx, int16_t* buf, int frames);

    static constexpr int kFrames = 256;
    uint32_t sample_rate{22050};

    Audio(int pin_BCLK, int pin_LRC, int pin_DIN, input::VolumeButtons& vol) :
      pin_BCLK(pin_BCLK), pin_LRC(pin_LRC), pin_DIN(pin_DIN), vol(vol) {}

    void begin() {
      xTaskCreatePinnedToCore(audio_task_trampoline, "audio", 8192, this, 10, nullptr, 0);
    }

    bool initialized() const { return initialized_.load(); }

    void set_source(SourceFn fn, void* ctx) {
      portENTER_CRITICAL(&src_mux);
      src = fn;
      src_ctx = ctx;
      portEXIT_CRITICAL(&src_mux);
    }

    int current_volume() const { return master_volume.load(); }
    int max_volume() const { return volume_max; }
    uint32_t volume_generation() const { return vol_generation.load(); }

  private:
    int pin_BCLK;
    int pin_LRC;
    int pin_DIN;

    input::VolumeButtons& vol;

    int dma_buf_count{8};
    int dma_buf_len{256};

    SourceFn src{nullptr};
    void* src_ctx{nullptr};
    portMUX_TYPE src_mux = portMUX_INITIALIZER_UNLOCKED;

    std::atomic<int> master_volume{5};
    const int volume_max{16};
    float vol_gains[17]{
      0.0000f, 0.0056f, 0.0079f, 0.0112f, 0.0158f, 0.0224f, 0.0316f, 0.0447f,
      0.0631f, 0.0891f, 0.1259f, 0.1778f, 0.2512f, 0.3548f, 0.5012f, 0.7079f, 1.0000f
    };

    Preferences prefs;
    std::atomic<uint32_t> vol_generation{0};
    std::atomic<bool> initialized_{false};

    static void audio_task_trampoline(void* param) { static_cast<Audio*>(param)->audio_task(); }
    static void volume_monitoring_task_trampoline(void* param) { static_cast<Audio*>(param)->volume_monitoring_task(); }

    void audio_task() {
      install_i2s();

      prefs.begin("audio", false);
      master_volume.store(std::clamp(prefs.getInt("volume", 5), 0, volume_max));

      xTaskCreatePinnedToCore(volume_monitoring_task_trampoline, "volume_monitoring_task", 3072, this, 1, nullptr, 1);
      initialized_.store(true);

      int16_t mono[kFrames];
      int16_t stereo[kFrames * 2];

      while (true) {
        SourceFn f;
        void* c;
        portENTER_CRITICAL(&src_mux);
        f = src;
        c = src_ctx;
        portEXIT_CRITICAL(&src_mux);

        if (f) f(c, mono, kFrames);
        else   memset(mono, 0, sizeof(mono));

        float gain = vol_gains[master_volume.load()];
        for (int i = 0; i < kFrames; i++) {
          int32_t v = (int32_t)(mono[i] * gain);
          if (v > 32767) v = 32767;
          if (v < -32768) v = -32768;
          stereo[2 * i] = stereo[2 * i + 1] = (int16_t)v;
        }

        size_t written = 0;
        i2s_write(I2S_NUM_0, stereo, sizeof(stereo), &written, portMAX_DELAY); // blocks
      }
    }

    void install_i2s() {
      i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = sample_rate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = dma_buf_count,
        .dma_buf_len          = dma_buf_len,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0,
      };
      if (i2s_driver_install(I2S_NUM_0, &cfg, dma_buf_count, nullptr) != ESP_OK) {
        Serial.println("[audio] i2s_driver_install failed");
        return;
      }
      i2s_pin_config_t pins = {
        .bck_io_num   = pin_BCLK,
        .ws_io_num    = pin_LRC,
        .data_out_num = pin_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
      };
      i2s_set_pin(I2S_NUM_0, &pins);
      i2s_zero_dma_buffer(I2S_NUM_0);
    }

    void set_volume(int v) {
      int clamped = std::clamp(v, 0, volume_max);
      if (clamped == master_volume.load()) return;
      master_volume.store(clamped);
      vol_generation.fetch_add(1, std::memory_order_relaxed);
      prefs.putInt("volume", clamped);
    }

    void volume_monitoring_task() {
      input::VolumeButtonState prev;
      while (true) {
        input::VolumeButtonState btns = vol.get();
        if (btns.UP   && !prev.UP)   set_volume(master_volume.load() + 1);
        if (btns.DOWN && !prev.DOWN) set_volume(master_volume.load() - 1);
        prev = btns;
        vTaskDelay(pdMS_TO_TICKS(30));
      }
    }
};

} // namespace audio
