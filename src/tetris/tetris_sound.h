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

struct Command {
  enum Type : uint8_t { PlayMenuCursorMoveSFX, PlayClearLines123SFX, StartBGM, StopBGM } type;
};

class SFX {
  public:
    int16_t* pcm;
    uint32_t length{0};

    SFX(std::string filepath) {
      File f = LittleFS.open(filepath.c_str(), "r");
      size_t bytes = f.size();
      pcm = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      f.read((uint8_t*)pcm, bytes);
      length = bytes / 2;
      f.close();
    }
};

struct Voice {const int16_t* pcm; uint32_t len; uint32_t pos;};

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

      // load sfx on ram
      SFX menu_move_cursor = SFX("/tetris/sfx_menu_move_cursor.raw");
      SFX clear_lines_123 = SFX("/tetris/sfx_clear_lines_123.raw");

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
            case Command::PlayMenuCursorMoveSFX:
              enqueue_voice(menu_move_cursor);
              break;
            case Command::PlayClearLines123SFX:
              enqueue_voice(clear_lines_123);
              break;
            case Command::StartBGM:
              if (bgm) bgm.close();
              bgm = LittleFS.open("/tetris/iwashiro_dokudoku_dog.raw", "r");
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

        // mix
        float fade_target = want_bgm ? 1.0f : 0.0f;
        for (int i = 0; i < 256; i++) {
          // fade in
          if (fade < fade_target) { fade += fade_step; if (fade > fade_target) fade = fade_target; }
          // fade out
          else if (fade > fade_target) { fade -= fade_step; if (fade < fade_target) fade = fade_target; }

          // apply fade
          int32_t acc = ((int32_t)bgm_chunk[i] * fade);

          for (int j = 0; j < 4; j++) {
            Voice& v = voices[j];
            if (!v.pcm) continue;
            acc += v.pcm[v.pos++];
            if (v.pos >= v.len) v.pcm = nullptr;
          }

          if (acc > 32767) acc = 32767;
          if (acc < -32768) acc = -32768;

          i2s_buf[2 * i]     = (int16_t)acc;
          i2s_buf[2 * i + 1] = (int16_t)acc;
        }

        if (!want_bgm && fade == 0.0f && bgm) bgm.close();

        audio.send_buffer(i2s_buf, 512);
      }
    }

    void sound_menu_cursor_move() { send(Command::PlayMenuCursorMoveSFX); }
    void sound_clear_lines_123() { send(Command::PlayClearLines123SFX); }
    void start_bgm() { send(Command::StartBGM); }
    void stop_bgm() { send(Command::StopBGM); }

  private:
    QueueHandle_t queue = nullptr;
    audio::Audio& audio;
    Voice voices[4] = {};

    void send(Command::Type t) { Command c{t}; xQueueSend(queue, &c, 0); }
    void enqueue_voice(SFX& sfx) {
      int pick = -1;

      // find empty voice slot
      for (int i = 0; i < 4; i++) {
        if (!voices[i].pcm) {pick = i; break; }
      }

      // if no empty slot, stop the biggest pos voice and use it
      if (pick < 0) {
        pick = 0;
        for (int i = 1; i < 4; i++)
          if (voices[i].pos > voices[pick].pos) pick = i;
      }
      voices[pick] = {sfx.pcm, sfx.length, 0};
    }
};
