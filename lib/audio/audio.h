#pragma once

#include "input.h"

#include <atomic>
#include <algorithm>

#include <Arduino.h>
#include <driver/i2s.h>

namespace audio {

class Audio {
  public:
    uint32_t sample_rate{22050};

    Audio(int pin_BCLK, int pin_LRC, int pin_DIN, input::VolumeButtons& vol) :
    pin_BCLK(pin_BCLK),
    pin_LRC(pin_LRC),
    pin_DIN(pin_DIN),
    vol(vol) {}

    void init() {
      /* configure i2s */

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
        .bck_io_num = pin_BCLK,
        .ws_io_num = pin_LRC,
        .data_out_num = pin_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE,
      };
      i2s_set_pin(I2S_NUM_0, &pins);
      i2s_zero_dma_buffer(I2S_NUM_0);

      /* start volume monitoring loop */
      xTaskCreatePinnedToCore(volume_monitoring_task_trampoline, "volume_monitoring_task", 3072, this, 1, nullptr, 1);
    }

    int send_buffer(int16_t *buffer, int length) {
      float gain = vol_gains[master_volume.load()];
      for (int i = 0; i < length; i++) {
        int32_t v = (int32_t)(buffer[i] * gain);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        buffer[i] = (int16_t)v;
      }
      size_t written = 0;
      i2s_write(I2S_NUM_0, buffer, length * sizeof(uint16_t), &written, portMAX_DELAY);
      return written;
    }

  private:
    int pin_BCLK;
    int pin_LRC;
    int pin_DIN;

    input::VolumeButtons& vol;
    float vol_gains[9]{0.0f, 0.0178f, 0.0316f, 0.0562f, 0.100f, 0.178f, 0.316f, 0.562f, 1.0f};

    int dma_buf_count{8};
    int dma_buf_len{256};

    std::atomic<int> master_volume{3};
    const int volume_max{8};

    static void volume_monitoring_task_trampoline(void* param) {
      static_cast<Audio*>(param)->volume_monitoring_task();
    }

    void set_volume(int v) { master_volume.store(std::clamp(v, 0, volume_max)); }

    void volume_monitoring_task() {
      input::VolumeButtonState prev;
      while (true) {
        input::VolumeButtonState btns = vol.get();
        if (btns.UP   && !prev.UP) set_volume(master_volume.load() + 1);
        if (btns.DOWN && !prev.DOWN) set_volume(master_volume.load() - 1);
        prev = btns;
        vTaskDelay(pdMS_TO_TICKS(30));
      }
    }
};
  
} // namespace audio
