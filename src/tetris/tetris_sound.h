#pragma once

#include <audio.h>

#include <Arduino.h>
#include <atomic>
#include <math.h>
#include <string.h>

#include <driver/i2s.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

constexpr const char* BGM_PATH = "/bgm.raw";

struct Command {
  enum Type : uint8_t { StartBGM, StopBGM } type;
};

class Sound {
  public:
    Sound(audio::Audio& audio) : audio(audio) {
      if (!LittleFS.begin(false)) {
        Serial.println("[audio] LittleFS mount failed — run: pio run -t uploadfs");
      }
      queue = xQueueCreate(24, sizeof(Command));
    }

    void begin() {
      xTaskCreatePinnedToCore(audio_consumer_loop_trampoline, "audio_consumer_loop", 8192, this, 8, nullptr, 0);
    }

    static void audio_consumer_loop_trampoline(void* param) {
      static_cast<Sound*>(param)->audio_consumer_loop();
    }

    void audio_consumer_loop() {
      audio.init();

      File bgm;
      bool want_bgm = false;

      int16_t bgm_chunk[256];
      int16_t i2s_buf[512];

      int fade_ms = 150;
      float fade_step = 1.0f / (audio.sample_rate * fade_ms / 1000.0f);
      float fade = 0.0f;

      while (true) {
        Command c;
        while (xQueueReceive(queue, &c, 0) == pdTRUE) {
          switch (c.type) {
            case Command::StartBGM:
              if (bgm) bgm.close();
              bgm = LittleFS.open(BGM_PATH, "r");
              want_bgm = (bool)bgm;
              fade = 0.0;
              if (!want_bgm) Serial.println("[audio] bgm open failed (uploadfs?)");
              break;
            case Command::StopBGM: want_bgm = false; break;
          }
        }

        if (bgm) {
          int got = bgm.read((uint8_t*)bgm_chunk, sizeof(bgm_chunk)) / 2;
          while (got < 256) {
            bgm.seek(0);
            int n = bgm.read((uint8_t*)(bgm_chunk + got), (256 - got) * 2) / 2;
            if (n <= 0) break;
            got += n;
          }
          for (int i = got; i < 256; i++) bgm_chunk[i] = 0;
        } else {
          memset(bgm_chunk, 0, sizeof(bgm_chunk));
        }

        // apply fade
        float fade_target = want_bgm ? 1.0f : 0.0f;
        for (int i = 0; i < 256; i++) {
          // fade in
          if (fade < fade_target) { fade += fade_step; if (fade > fade_target) fade = fade_target; }
          // fade out
          else if (fade > fade_target) { fade -= fade_step; if (fade < fade_target) fade = fade_target; }

          int32_t s = ((int32_t)bgm_chunk[i] * fade);
          i2s_buf[2 * i]     = (int16_t)s;
          i2s_buf[2 * i + 1] = (int16_t)s;
        }

        if (!want_bgm && fade == 0.0f && bgm) bgm.close();

        audio.send_buffer(i2s_buf, 512);
      }
    }

    void start_bgm() { send(Command::StartBGM); }
    void stop_bgm() { send(Command::StopBGM); }

  private:
    QueueHandle_t queue = nullptr;
    audio::Audio& audio;

    void send(Command::Type t) { Command c{t}; xQueueSend(queue, &c, 0); }
};
