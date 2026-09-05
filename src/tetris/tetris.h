#pragma once

#include <atomic>
#include <optional>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <TFT_eSPI.h>

#include <input.h>
#include <audio.h>

#include "../scene.h"

#include "tetris_common.h"
#include "tetris_buffer.h"
#include "tetris_logic.h"
#include "tetris_rendering.h"
#include "tetris_sound.h"

namespace yomogame {
namespace tetris {

enum class MenuFocusedItem { MODE, STARTING_LEVEL, GARBAGE_LINES };

class Play {
  public:
    TripleBuffer tb;
    std::atomic<bool> running{true};
    SemaphoreHandle_t done_sem;
    GameLogic logic;
    GameRenderer renderer;
    TaskHandle_t logic_task{nullptr};

    Play(GameMode mode, int level, Sound& sound, int garbage_lines,
         input::Buttons& buttons, TFT_eSPI& screen, DisplayParameters& dp) :
      done_sem(xSemaphoreCreateBinary()),
      logic(tb, mode, level, sound, garbage_lines, buttons, done_sem),
      renderer(tb, screen, dp, running) {}

    ~Play() { vSemaphoreDelete(done_sem); }

    void begin() {
      renderer.setup_screen();
      xTaskCreatePinnedToCore(logic_trampoline, "logic", 8192, this, 2, &logic_task, 1);
    }

    void render_frame() { renderer.render_frame(); }

    bool finished() { return xSemaphoreTake(done_sem, 0) == pdTRUE; }

    GameResult result() { return *logic.final_result; }

  private:
    static void logic_trampoline(void* p) { static_cast<Play*>(p)->logic.main_loop(); }
};

class TetrisScene : public Scene {
  public:
    void enter(Context& ctx) override {
      sound_ = new Sound(ctx.audio);
      sound_->begin();
      enter_menu(ctx);
    }

    Scene* tick(Context& ctx) override {
      switch (state_) {
        case State::MENU:      if (Scene* s = tick_menu(ctx)) return s; break;
        case State::COUNTDOWN: tick_countdown(ctx); break;
        case State::PLAYING:   tick_playing(ctx);   break;
        case State::RESULT:    tick_result(ctx);    break;
      }
      return this;
    }

    void exit(Context& ctx) override {
      delete play_; play_ = nullptr;
      ctx.audio.set_source(nullptr, nullptr);
      delete sound_; sound_ = nullptr;
    }

  private:
    enum class State { MENU, COUNTDOWN, PLAYING, RESULT };

    State state_{State::MENU};
    DisplayParameters dp_ = disp_param_240x240;

    Sound* sound_{nullptr};
    Play* play_{nullptr};

    input::ButtonState prev_{};

    GameMode selected_mode_{GameMode::L99999};
    int selected_level_{1};
    int selected_garbage_{0};
    MenuFocusedItem focused_{MenuFocusedItem::MODE};
    bool menu_dirty_{true};

    unsigned long countdown_started_{0};
    int countdown_step_drawn_{-1};

    std::optional<GameResult> last_result_;
    unsigned long result_ready_at_{0};
    bool result_drawn_{false};

    void enter_menu(Context& ctx) {
      ctx.screen.fillScreen(TFT_BLACK);
      render_menu_title(ctx.screen, dp_.menu_title_x_center_in_sprite, dp_.menu_title_y_in_sprite, 2);
      render_menu_msg(ctx.screen, dp_.menu_msg_x_center_in_sprite, dp_.menu_msg_y_in_sprite, 2);
      focused_ = MenuFocusedItem::MODE;
      menu_dirty_ = true;
      prev_ = ctx.buttons.get();
      state_ = State::MENU;
    }

    void draw_menu(Context& ctx) {
      TFT_eSPI& s = ctx.screen;
      render_menu_label(s, "Mode", dp_.menu_mode_label_x_in_sprite, dp_.menu_mode_label_y_in_sprite, 2, focused_ == MenuFocusedItem::MODE);
      render_menubox(s, "99999", dp_.menu_mode_l99999_x_in_sprite, dp_.menu_mode_value_y_in_sprite, dp_.menu_mode_value_width, dp_.menu_mode_value_height, 2, focused_ == MenuFocusedItem::MODE, selected_mode_ == GameMode::L99999);
      render_menubox(s, "150", dp_.menu_mode_l150_x_in_sprite, dp_.menu_mode_value_y_in_sprite, dp_.menu_mode_value_width, dp_.menu_mode_value_height, 2, focused_ == MenuFocusedItem::MODE, selected_mode_ == GameMode::L150);
      render_menubox(s, "40", dp_.menu_mode_l40_x_in_sprite, dp_.menu_mode_value_y_in_sprite, dp_.menu_mode_value_width, dp_.menu_mode_value_height, 2, focused_ == MenuFocusedItem::MODE, selected_mode_ == GameMode::L40);

      render_menu_label(s, "Starting Level", dp_.menu_level_label_x_in_sprite, dp_.menu_level_label_y_in_sprite, 2, focused_ == MenuFocusedItem::STARTING_LEVEL);
      render_menubox(s, "1", dp_.menu_level_1_x_in_sprite, dp_.menu_level_value_y_in_sprite, dp_.menu_level_value_width, dp_.menu_level_value_height, 2, focused_ == MenuFocusedItem::STARTING_LEVEL, selected_level_ == 1);
      render_menubox(s, "10", dp_.menu_level_10_x_in_sprite, dp_.menu_level_value_y_in_sprite, dp_.menu_level_value_width, dp_.menu_level_value_height, 2, focused_ == MenuFocusedItem::STARTING_LEVEL, selected_level_ == 10);
      render_menubox(s, "20", dp_.menu_level_20_x_in_sprite, dp_.menu_level_value_y_in_sprite, dp_.menu_level_value_width, dp_.menu_level_value_height, 2, focused_ == MenuFocusedItem::STARTING_LEVEL, selected_level_ == 20);

      render_menu_label(s, "Garbage Lines", dp_.menu_garbage_label_x_in_sprite, dp_.menu_garbage_label_y_in_sprite, 2, focused_ == MenuFocusedItem::GARBAGE_LINES);
      render_menubox(s, "0", dp_.menu_garbage_0_x_in_sprite, dp_.menu_garbage_value_y_in_sprite, dp_.menu_garbage_value_width, dp_.menu_garbage_value_height, 2, focused_ == MenuFocusedItem::GARBAGE_LINES, selected_garbage_ == 0);
      render_menubox(s, "6", dp_.menu_garbage_6_x_in_sprite, dp_.menu_garbage_value_y_in_sprite, dp_.menu_garbage_value_width, dp_.menu_garbage_value_height, 2, focused_ == MenuFocusedItem::GARBAGE_LINES, selected_garbage_ == 6);
      render_menubox(s, "12", dp_.menu_garbage_12_x_in_sprite, dp_.menu_garbage_value_y_in_sprite, dp_.menu_garbage_value_width, dp_.menu_garbage_value_height, 2, focused_ == MenuFocusedItem::GARBAGE_LINES, selected_garbage_ == 12);
    }

    // Returns non-null to switch away (back to the game-select screen).
    Scene* tick_menu(Context& ctx) {
      if (menu_dirty_) {
        draw_menu(ctx);
        menu_dirty_ = false;
      }

      input::ButtonState btns = ctx.buttons.get();

      if (btns.A && !prev_.A) {
        enter_countdown(ctx);
        return nullptr;
      }
      if (btns.B && !prev_.B) {
        return make_select_scene();
      }

      auto before = std::make_tuple(focused_, selected_mode_, selected_level_, selected_garbage_);

      if (btns.UP && !prev_.UP) {
        if (focused_ == MenuFocusedItem::STARTING_LEVEL) focused_ = MenuFocusedItem::MODE;
        else if (focused_ == MenuFocusedItem::GARBAGE_LINES) focused_ = MenuFocusedItem::STARTING_LEVEL;

      } else if (btns.DOWN && !prev_.DOWN) {
        if (focused_ == MenuFocusedItem::MODE) focused_ = MenuFocusedItem::STARTING_LEVEL;
        else if (focused_ == MenuFocusedItem::STARTING_LEVEL) focused_ = MenuFocusedItem::GARBAGE_LINES;

      } else if (btns.LEFT && !prev_.LEFT) {
        if (focused_ == MenuFocusedItem::MODE) {
          if (selected_mode_ == GameMode::L150) selected_mode_ = GameMode::L99999;
          else if (selected_mode_ == GameMode::L40) selected_mode_ = GameMode::L150;
        } else if (focused_ == MenuFocusedItem::STARTING_LEVEL) {
          if (selected_level_ == 10) selected_level_ = 1;
          else if (selected_level_ == 20) selected_level_ = 10;
        } else if (focused_ == MenuFocusedItem::GARBAGE_LINES) {
          if (selected_garbage_ == 6) selected_garbage_ = 0;
          else if (selected_garbage_ == 12) selected_garbage_ = 6;
        }

      } else if (btns.RIGHT && !prev_.RIGHT) {
        if (focused_ == MenuFocusedItem::MODE) {
          if (selected_mode_ == GameMode::L99999) selected_mode_ = GameMode::L150;
          else if (selected_mode_ == GameMode::L150) selected_mode_ = GameMode::L40;
        } else if (focused_ == MenuFocusedItem::STARTING_LEVEL) {
          if (selected_level_ == 1) selected_level_ = 10;
          else if (selected_level_ == 10) selected_level_ = 20;
        } else if (focused_ == MenuFocusedItem::GARBAGE_LINES) {
          if (selected_garbage_ == 0) selected_garbage_ = 6;
          else if (selected_garbage_ == 6) selected_garbage_ = 12;
        }
      }
      prev_ = btns;

      if (before != std::make_tuple(focused_, selected_mode_, selected_level_, selected_garbage_)) {
        menu_dirty_ = true;
        sound_->sound_cursor();
      }
      return nullptr;
    }

    void enter_countdown(Context& ctx) {
      ctx.screen.fillScreen(TFT_BLACK);
      countdown_started_ = millis();
      countdown_step_drawn_ = -1;
      state_ = State::COUNTDOWN;
    }

    void tick_countdown(Context& ctx) {
      int step = (int)((millis() - countdown_started_) / 700); // 0,1,2 -> 3 == done
      if (step >= 3) { enter_playing(ctx); return; }
      if (step != countdown_step_drawn_) {
        countdown_step_drawn_ = step;
        sound_->sound_countdown();
        const char* s = (step == 0) ? "3" : (step == 1) ? "2" : "1";
        ctx.screen.fillScreen(TFT_BLACK);
        ctx.screen.setTextColor(TFT_WHITE, TFT_BLACK);
        ctx.screen.setTextDatum(MC_DATUM);
        ctx.screen.drawString(s, dp_.menu_sprite_width / 2, dp_.menu_sprite_height / 2, 2);
      }
    }

    void enter_playing(Context& ctx) {
      ctx.screen.fillScreen(TFT_BLACK);
      play_ = new Play(selected_mode_, selected_level_, *sound_, selected_garbage_,
                       ctx.buttons, ctx.screen, dp_);
      play_->begin();
      sound_->start_bgm();
      state_ = State::PLAYING;
    }

    void tick_playing(Context& ctx) {
      play_->render_frame();
      if (!play_->finished()) return;

      sound_->stop_bgm();
      GameResult r = play_->result();
      play_->render_frame();
      vTaskDelay(pdMS_TO_TICKS(5));
      delete play_;
      play_ = nullptr;

      if (r.code == GameResultCode::Cancel) {
        enter_menu(ctx);
        return;
      }
      last_result_ = r;
      result_drawn_ = false;
      result_ready_at_ = millis() + 500;
      state_ = State::RESULT;
    }

    void tick_result(Context& ctx) {
      if (millis() < result_ready_at_) return;

      if (!result_drawn_) {
        if (last_result_->code == GameResultCode::Clear) sound_->sound_success();
        else if (last_result_->code == GameResultCode::Fail) sound_->sound_fail();
        ctx.screen.fillScreen(TFT_BLACK);
        draw_result(ctx.screen, *last_result_);
        result_drawn_ = true;
        prev_ = ctx.buttons.get();
        return;
      }

      input::ButtonState btns = ctx.buttons.get();
      if (btns.B && !prev_.B) { enter_menu(ctx); return; }
      prev_ = btns;
    }

    void draw_result(TFT_eSPI& screen, const GameResult& result) {
      TFT_eSprite s(&screen);
      if (!s.createSprite(dp_.result_sprite_width, dp_.result_sprite_height)) {
        Serial.println("[tetris] result sprite alloc failed");
        return;
      }

      s.drawRect(0, 0, dp_.result_sprite_width, dp_.result_sprite_height, TFT_WHITE);
      s.fillRect(1, 1, dp_.result_sprite_width - 2, dp_.result_sprite_height - 2, TFT_BLACK);

      s.setTextColor(TFT_WHITE, TFT_BLACK);
      s.setTextDatum(TL_DATUM);
      s.drawString("Score", dp_.result_label_x_in_sprite, dp_.result_score_y_in_sprite, 1);
      s.drawString("Time", dp_.result_label_x_in_sprite, dp_.result_time_y_in_sprite, 1);
      s.drawString("Lines", dp_.result_label_x_in_sprite, dp_.result_lines_y_in_sprite, 1);
      s.drawString("Level", dp_.result_label_x_in_sprite, dp_.result_level_y_in_sprite, 1);
      s.drawString("Tetrises", dp_.result_label_x_in_sprite, dp_.result_tetris_y_in_sprite, 1);
      s.drawString("T-Spins", dp_.result_label_x_in_sprite, dp_.result_tspins_y_in_sprite, 1);
      s.drawString("Max Combos", dp_.result_label_x_in_sprite, dp_.result_maxcombos_y_in_sprite, 1);
      s.drawString("Holds", dp_.result_label_x_in_sprite, dp_.result_holds_y_in_sprite, 1);
      s.drawString("TPM", dp_.result_label_x_in_sprite, dp_.result_tpm_y_in_sprite, 1);
      s.drawString("LPM", dp_.result_label_x_in_sprite, dp_.result_lpm_y_in_sprite, 1);

      std::string code_str = result.code == GameResultCode::Clear ? "Clear!!" : result.code == GameResultCode::Fail ? "Fail..." : "Canceled";
      uint16_t result_color = result.code == GameResultCode::Clear ? TFT_GREEN : result.code == GameResultCode::Fail ? TFT_ORANGE : TFT_WHITE;

      s.setTextColor(result_color, TFT_BLACK);
      s.setTextDatum(TC_DATUM);
      s.drawString(code_str.c_str(), dp_.result_sprite_width / 2, dp_.result_result_y_in_sprite, 1);

      s.setTextColor(TFT_WHITE, TFT_BLACK);
      s.setTextDatum(TR_DATUM);
      s.drawString(std::to_string(result.score).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_score_y_in_sprite, 1);

      int hours = result.elapsed_ms / (1000 * 60 * 60);
      int minutes = result.elapsed_ms / (1000 * 60);
      int seconds = (result.elapsed_ms / 1000) % 60;
      int centis = (result.elapsed_ms % 1000) / 10;
      char time[12];
      snprintf(time, sizeof(time), "%02d:%02d:%02d:%02d", hours, minutes, seconds, centis);
      s.drawString(time, dp_.result_value_x_right_in_sprite, dp_.result_time_y_in_sprite, 1);

      s.drawString(std::to_string(result.removed_lines).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_lines_y_in_sprite, 1);
      s.drawString(std::to_string(result.level_at_last).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_level_y_in_sprite, 1);
      s.drawString(std::to_string(result.tetris_count).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_tetris_y_in_sprite, 1);
      s.drawString(std::to_string(result.tspins).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_tspins_y_in_sprite, 1);
      s.drawString(std::to_string(result.max_combos).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_maxcombos_y_in_sprite, 1);
      s.drawString(std::to_string(result.holds).c_str(), dp_.result_value_x_right_in_sprite, dp_.result_holds_y_in_sprite, 1);

      char tpm_str[6];
      char lpm_str[6];
      snprintf(tpm_str, sizeof(tpm_str), "%.1f", result.tpm);
      snprintf(lpm_str, sizeof(lpm_str), "%.1f", result.lpm);
      s.drawString(tpm_str, dp_.result_value_x_right_in_sprite, dp_.result_tpm_y_in_sprite, 1);
      s.drawString(lpm_str, dp_.result_value_x_right_in_sprite, dp_.result_lpm_y_in_sprite, 1);

      s.setTextColor(TFT_GREEN, TFT_BLACK);
      s.setTextDatum(TC_DATUM);
      s.drawString("Press B for menu", dp_.result_sprite_width / 2, dp_.result_msg_y_in_sprite, 1);

      s.pushSprite(dp_.result_sprite_x, dp_.result_sprite_y);
      s.deleteSprite();
    }

    void render_menubox(TFT_eSPI& sprite, std::string str, int x, int y, int width, int height, uint8_t font, boolean focused, boolean selected) {
      uint16_t grid_color = TFT_DARKGREY;
      uint16_t bg_color = TFT_BLACK;
      uint16_t char_color = TFT_DARKGREY;

      if (focused && selected) {
        grid_color = TFT_WHITE; bg_color = TFT_ORANGE; char_color = TFT_BLACK;
      } else if (focused) {
        grid_color = TFT_WHITE; bg_color = TFT_BLACK; char_color = TFT_WHITE;
      } else if (selected) {
        grid_color = TFT_DARKGREY; bg_color = TFT_DARKGREY; char_color = TFT_WHITE;
      }

      sprite.drawRect(x, y, width, height, grid_color);
      sprite.fillRect(x + 1, y + 1, width - 2, height - 2, bg_color);
      sprite.setTextColor(char_color, bg_color);
      sprite.setTextDatum(MC_DATUM);
      sprite.drawString(str.c_str(), x + (width / 2), y + (height / 2), font);
    }

    void render_menu_label(TFT_eSPI& sprite, std::string str, int x, int y, uint8_t font, boolean focused) {
      uint16_t char_color = focused ? TFT_ORANGE : TFT_DARKGREY;
      sprite.setTextColor(char_color, TFT_BLACK);
      sprite.setTextDatum(TL_DATUM);
      sprite.drawString(str.c_str(), x, y, font);
    }

    void render_menu_title(TFT_eSPI& sprite, int x, int y, uint8_t font) {
      sprite.setTextColor(TFT_CYAN, TFT_BLACK);
      sprite.setTextDatum(TC_DATUM);
      sprite.drawString("YomoTetris", x, y, font);
    }

    void render_menu_msg(TFT_eSPI& sprite, int x, int y, uint8_t font) {
      sprite.setTextColor(TFT_GREEN, TFT_BLACK);
      sprite.setTextDatum(TC_DATUM);
      sprite.drawString("A: play   B: back", x, y, font);
    }
};

} // namespace tetris
} // namespace yomogame
