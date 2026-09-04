#pragma once

#include <audio.h>

#include <Arduino.h>
#include <string.h>

#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct Command {
  enum Type : uint8_t {
    PlayCursorSFX, PlayCountdownSFX, PlayHardDropSFX, PlayHoldSFX,
    PlayClearLines123SFX, PlayTetrisSFX, PlayPauseSFX, PlayResumeSFX,
    PlayCancelSFX, PlaySuccessSFX, PlayFailSFX, StartBGM, StopBGM
  } type;
};

class SFX {
  public:
    int16_t* pcm{nullptr};
    uint32_t length{0};

    void load(const char* filepath) {
      File f = LittleFS.open(filepath, "r");
      if (!f) {
        Serial.printf("file %s not found\n", filepath);
        abort();
      }
      size_t bytes = f.size();
      pcm = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      f.read((uint8_t*)pcm, bytes);
      length = bytes / 2;
      f.close();
    }
};

struct Voice { const int16_t* pcm; uint32_t len; uint32_t pos; };

class Sound {
  public:
    Sound(audio::Audio& audio) : audio(audio) {
      if (!LittleFS.begin(false)) {
        Serial.println("[audio] LittleFS mount failed — run: pio run -t uploadfs");
      }
      queue = xQueueCreate(24, sizeof(Command));
      fade_step = 1.0f / (audio.sample_rate * fade_ms / 1000.0f);
    }

    void begin() {
      cursor.load("/tetris/sfx_cursor.raw");
      countdown.load("/tetris/sfx_countdown.raw");
      hard_drop.load("/tetris/sfx_hard_drop.raw");
      hold.load("/tetris/sfx_hold.raw");
      clear_lines_123.load("/tetris/sfx_clear_lines_123.raw");
      tetris.load("/tetris/sfx_tetris.raw");
      pause.load("/tetris/sfx_pause.raw");
      resume.load("/tetris/sfx_resume.raw");
      cancel.load("/tetris/sfx_cancel.raw");
      success.load("/tetris/sfx_success.raw");
      fail.load("/tetris/sfx_fail.raw");

      audio.set_source(&Sound::fill_trampoline, this);
    }

    void sound_cursor()          { send(Command::PlayCursorSFX); }
    void sound_countdown()       { send(Command::PlayCountdownSFX); }
    void sound_hard_drop()       { send(Command::PlayHardDropSFX); }
    void sound_hold()            { send(Command::PlayHoldSFX); }
    void sound_clear_lines_123() { send(Command::PlayClearLines123SFX); }
    void sound_tetris()          { send(Command::PlayTetrisSFX); }
    void sound_pause()           { send(Command::PlayPauseSFX); }
    void sound_resume()          { send(Command::PlayResumeSFX); }
    void sound_cancel()          { send(Command::PlayCancelSFX); }
    void sound_success()         { send(Command::PlaySuccessSFX); }
    void sound_fail()            { send(Command::PlayFailSFX); }
    void start_bgm()             { send(Command::StartBGM); }
    void stop_bgm()              { send(Command::StopBGM); }

  private:
    audio::Audio& audio;
    QueueHandle_t queue = nullptr;
    Voice voices[4] = {};

    File bgm;
    bool want_bgm = false;
    int16_t bgm_chunk[audio::Audio::kFrames];
    const int fade_ms = 150;
    float fade_step = 0.0f;
    float fade = 0.0f;

    SFX cursor, countdown, hard_drop, hold, clear_lines_123, tetris;
    SFX pause, resume, cancel, success, fail;

    static void fill_trampoline(void* ctx, int16_t* buf, int frames) {
      static_cast<Sound*>(ctx)->fill(buf, frames);
    }

    void fill(int16_t* buf, int frames) {
      Command c;
      while (xQueueReceive(queue, &c, 0) == pdTRUE) {
        switch (c.type) {
          case Command::PlayCursorSFX:        enqueue_voice(cursor); break;
          case Command::PlayCountdownSFX:     enqueue_voice(countdown); break;
          case Command::PlayHardDropSFX:      enqueue_voice(hard_drop); break;
          case Command::PlayHoldSFX:          enqueue_voice(hold); break;
          case Command::PlayClearLines123SFX: enqueue_voice(clear_lines_123); break;
          case Command::PlayTetrisSFX:        enqueue_voice(tetris); break;
          case Command::PlayPauseSFX:         enqueue_voice(pause); break;
          case Command::PlayResumeSFX:        enqueue_voice(resume); break;
          case Command::PlayCancelSFX:        enqueue_voice(cancel); break;
          case Command::PlaySuccessSFX:       enqueue_voice(success); break;
          case Command::PlayFailSFX:          enqueue_voice(fail); break;
          case Command::StartBGM: {
            if (bgm) bgm.close();
            bgm = LittleFS.open("/tetris/iwashiro_dokudoku_dog.raw", "r");
            want_bgm = (bool)bgm;
            fade = 0.0f;
            int16_t probe[8] = {0};
            int pn = bgm ? bgm.read((uint8_t*)probe, sizeof(probe)) : -1;
            if (bgm) bgm.seek(0);
            Serial.printf("[bgm] open=%d size=%d read=%d [%d %d %d %d]\n",
                          (int)want_bgm, bgm ? (int)bgm.size() : -1, pn,
                          probe[0], probe[1], probe[2], probe[3]);
            break;
          }
          case Command::StopBGM: want_bgm = false; break;
        }
      }

      if (bgm) {
        int got = bgm.read((uint8_t*)bgm_chunk, sizeof(bgm_chunk)) / 2;
        while (got < frames) {
          bgm.seek(0);
          int n = bgm.read((uint8_t*)(bgm_chunk + got), (frames - got) * 2) / 2;
          if (n <= 0) break;
          got += n;
        }
        for (int i = got; i < frames; i++) bgm_chunk[i] = 0;
      } else {
        memset(bgm_chunk, 0, sizeof(bgm_chunk));
      }

      float fade_target = want_bgm ? 1.0f : 0.0f;
      for (int i = 0; i < frames; i++) {
        if (fade < fade_target)      { fade += fade_step; if (fade > fade_target) fade = fade_target; }
        else if (fade > fade_target) { fade -= fade_step; if (fade < fade_target) fade = fade_target; }

        int32_t acc = (int32_t)(bgm_chunk[i] * fade);

        for (int j = 0; j < 4; j++) {
          Voice& v = voices[j];
          if (!v.pcm) continue;
          acc += v.pcm[v.pos++];
          if (v.pos >= v.len) v.pcm = nullptr;
        }

        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        buf[i] = (int16_t)acc;
      }

      if (!want_bgm && fade == 0.0f && bgm) bgm.close();
    }

    void send(Command::Type t) { Command c{t}; xQueueSend(queue, &c, 0); }

    void enqueue_voice(SFX& sfx) {
      int pick = -1;
      for (int i = 0; i < 4; i++) {
        if (!voices[i].pcm) { pick = i; break; }
      }
      if (pick < 0) {
        pick = 0;
        for (int i = 1; i < 4; i++)
          if (voices[i].pos > voices[pick].pos) pick = i;
      }
      voices[pick] = {sfx.pcm, sfx.length, 0};
    }
};
