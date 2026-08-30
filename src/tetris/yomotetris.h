#pragma once

#include <atomic>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <TFT_eSPI.h>

#include <input.h>

#include "tetris_common.h"
#include "tetris_buffer.h"
#include "tetris_logic.h"
#include "tetris_rendering.h"

class Game {
  public:
    TripleBuffer tb;
    SemaphoreHandle_t logic_done_sem;
    std::atomic<bool> logic_running{true};
    GameLogic logic;
    GameRenderer renderer;
    TaskHandle_t logic_task_handle;
    TaskHandle_t render_task_handle;

    Game(GameMode mode, Input &input, TFT_eSPI &screen, DisplayParameters &params) :
      logic_done_sem(xSemaphoreCreateBinary()),
      logic(tb, mode, input, logic_done_sem),
      renderer(tb, screen, params, logic_running, mode == GameMode::Endless),
      logic_task_handle(nullptr),
      render_task_handle(nullptr)
      {}

    ~Game() {
      vSemaphoreDelete(logic_done_sem);
    }

    GameResult start() {
      renderer.setup_screen();

      xTaskCreatePinnedToCore(logic_task_trampoline, "logic", 8192, this, 2, &logic_task_handle, 1);
      xTaskCreatePinnedToCore(render_task_trampoline, "render", 8192, this, 1, &render_task_handle, 1);

      xSemaphoreTake(logic_done_sem, portMAX_DELAY);

      logic_running = false;
      vTaskDelay(pdMS_TO_TICKS(50)); // wait for rendering thread terminates

      return *logic.final_result;
    }

    static void logic_task_trampoline(void* param) {
      static_cast<Game*>(param)->logic.main_loop();
    }

    static void render_task_trampoline(void* param) {
      static_cast<Game*>(param)->renderer.rendering_loop_60fps();
    }
};

class YomoTetris {
  public:
    GameMode cur_focus_mode;
    DisplayParameters dp;
    Input input;
    TFT_eSPI &screen;
    TFT_eSprite menu_sprite;
    TFT_eSprite result_sprite;

    YomoTetris(Input input, TFT_eSPI &screen, DisplayParameters params) :
      cur_focus_mode(GameMode::Endless),
      dp(params),
      input(input),
      screen(screen),
      menu_sprite(&screen),
      result_sprite(&screen) {
        menu_sprite.createSprite(dp.menu_sprite_width, dp.menu_sprite_height);
        result_sprite.createSprite(dp.result_sprite_width, dp.result_sprite_height);
      }

    void render_menubox(TFT_eSprite& sprite, std::string str, int x, int y, int width, int height, uint8_t font, boolean selected) {
      uint16_t grid_color = TFT_WHITE;
      uint16_t bg_color = TFT_BLACK;
      uint16_t char_color = TFT_WHITE;
      if (selected) {
        grid_color = TFT_WHITE;
        bg_color = TFT_WHITE;
        char_color = TFT_BLACK;
      }

      sprite.drawRect(x, y, width, height, grid_color);
      sprite.fillRect(x+1, y+1, width-2, height-2, bg_color);
      sprite.setTextColor(char_color, bg_color);
      sprite.setTextDatum(MC_DATUM);
      sprite.drawString(str.c_str(), x + (width / 2), y + (height / 2), font);
    }

    void menu() {
      boolean was_up = false;
      boolean was_down = false;
      while (true) {
        render_menubox(menu_sprite, "Endless",   dp.menu_mode_x_in_sprite, dp.menu_mode_endless_y_in_sprite, dp.menu_mode_width, dp.menu_mode_height, 2, cur_focus_mode == GameMode::Endless);
        render_menubox(menu_sprite, "40 Lines",  dp.menu_mode_x_in_sprite, dp.menu_mode_l40_y_in_sprite,     dp.menu_mode_width, dp.menu_mode_height, 2, cur_focus_mode == GameMode::L40);
        render_menubox(menu_sprite, "150 Lines", dp.menu_mode_x_in_sprite, dp.menu_mode_l150_y_in_sprite,    dp.menu_mode_width, dp.menu_mode_height, 2, cur_focus_mode == GameMode::L150);
        render_menubox(menu_sprite, "999 Lines", dp.menu_mode_x_in_sprite, dp.menu_mode_l999_y_in_sprite,    dp.menu_mode_width, dp.menu_mode_height, 2, cur_focus_mode == GameMode::L999);

        ButtonState btns = input.get();
        if (btns.A) return;

        if (btns.UP && !was_up) {
          if (cur_focus_mode == GameMode::Endless) cur_focus_mode = GameMode::L999;
          else if (cur_focus_mode == GameMode::L40) cur_focus_mode = GameMode::Endless;
          else if (cur_focus_mode == GameMode::L150) cur_focus_mode = GameMode::L40;
          else if (cur_focus_mode == GameMode::L999) cur_focus_mode = GameMode::L150;
        } else if (btns.DOWN && !was_down) {
          if (cur_focus_mode == GameMode::Endless) cur_focus_mode = GameMode::L40;
          else if (cur_focus_mode == GameMode::L40) cur_focus_mode = GameMode::L150;
          else if (cur_focus_mode == GameMode::L150) cur_focus_mode = GameMode::L999;
          else if (cur_focus_mode == GameMode::L999) cur_focus_mode = GameMode::Endless;
        }

        was_up = btns.UP;
        was_down = btns.DOWN;

        menu_sprite.pushSprite(dp.menu_sprite_x, dp.menu_sprite_y);

        delay(10);
      }
    }

    void show_result(GameResult result) {
      result_sprite.drawRect(0, 0, dp.result_sprite_width, dp.result_sprite_height, TFT_WHITE);
      result_sprite.fillRect(1, 1, dp.result_sprite_width-2, dp.result_sprite_height-2, TFT_BLACK);

      // render labels
      result_sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      result_sprite.setTextDatum(TL_DATUM);

      result_sprite.drawString("Score"     , dp.result_label_x_in_sprite, dp.result_score_y_in_sprite    , 1);
      result_sprite.drawString("Time"      , dp.result_label_x_in_sprite, dp.result_time_y_in_sprite     , 1);
      result_sprite.drawString("Lines"     , dp.result_label_x_in_sprite, dp.result_lines_y_in_sprite    , 1);
      result_sprite.drawString("Level"     , dp.result_label_x_in_sprite, dp.result_level_y_in_sprite    , 1);
      result_sprite.drawString("Tetrises"  , dp.result_label_x_in_sprite, dp.result_tetris_y_in_sprite   , 1);
      result_sprite.drawString("T-Spins"   , dp.result_label_x_in_sprite, dp.result_tspins_y_in_sprite   , 1);
      result_sprite.drawString("Max Combos", dp.result_label_x_in_sprite, dp.result_maxcombos_y_in_sprite, 1);
      result_sprite.drawString("Holds"     , dp.result_label_x_in_sprite, dp.result_holds_y_in_sprite    , 1);
      result_sprite.drawString("TPM"       , dp.result_label_x_in_sprite, dp.result_tpm_y_in_sprite      , 1);
      result_sprite.drawString("LPM"       , dp.result_label_x_in_sprite, dp.result_lpm_y_in_sprite      , 1);

      // render values
      std::string code_str = result.mode == GameMode::Endless ? "Result" : result.code == GameResultCode::Clear ? "Clear!!" : result.code == GameResultCode::Fail ? "Fail..." : "Canceled";
      uint16_t result_color = result.mode == GameMode::Endless ? TFT_WHITE : result.code == GameResultCode::Clear ? TFT_GREEN : result.code == GameResultCode::Fail ? TFT_ORANGE : TFT_WHITE;

      // code is center with a specific color
      result_sprite.setTextColor(result_color, TFT_BLACK);
      result_sprite.setTextDatum(TC_DATUM);
      result_sprite.drawString(code_str.c_str()                            , dp.result_sprite_width / 2, dp.result_result_y_in_sprite   , 1);

      // others are white, right aligned
      result_sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      result_sprite.setTextDatum(TR_DATUM);
      result_sprite.drawString(std::to_string(result.score).c_str()        , dp.result_value_x_right_in_sprite, dp.result_score_y_in_sprite    , 1);

      int hours = result.elapsed_ms / (1000 * 60 * 60);
      int minutes = result.elapsed_ms / (1000 * 60);
      int seconds = (result.elapsed_ms / 1000) % 60;
      int centis = (result.elapsed_ms % 1000) / 10;
      char time[12];
      snprintf(time, sizeof(time), "%02d:%02d:%02d:%02d", hours, minutes, seconds, centis);
      result_sprite.drawString(time, dp.result_value_x_right_in_sprite, dp.result_time_y_in_sprite     , 1);

      result_sprite.drawString(std::to_string(result.removed_lines).c_str(), dp.result_value_x_right_in_sprite, dp.result_lines_y_in_sprite    , 1);
      result_sprite.drawString(std::to_string(result.level_at_last).c_str(), dp.result_value_x_right_in_sprite, dp.result_level_y_in_sprite    , 1);
      result_sprite.drawString(std::to_string(result.tetris_count).c_str() , dp.result_value_x_right_in_sprite, dp.result_tetris_y_in_sprite   , 1);
      result_sprite.drawString(std::to_string(result.tspins).c_str()       , dp.result_value_x_right_in_sprite, dp.result_tspins_y_in_sprite   , 1);
      result_sprite.drawString(std::to_string(result.max_combos).c_str()   , dp.result_value_x_right_in_sprite, dp.result_maxcombos_y_in_sprite, 1);
      result_sprite.drawString(std::to_string(result.holds).c_str()        , dp.result_value_x_right_in_sprite, dp.result_holds_y_in_sprite    , 1);

      char tpm_str[6];
      char lpm_str[6];
      snprintf(tpm_str, sizeof(tpm_str), "%.1f", result.tpm);
      snprintf(lpm_str, sizeof(lpm_str), "%.1f", result.lpm);

      result_sprite.drawString(tpm_str, dp.result_value_x_right_in_sprite, dp.result_tpm_y_in_sprite, 1);
      result_sprite.drawString(lpm_str, dp.result_value_x_right_in_sprite, dp.result_lpm_y_in_sprite, 1);

      // message

      result_sprite.setTextColor(TFT_GREEN, TFT_BLACK);
      result_sprite.setTextDatum(TC_DATUM);
      result_sprite.drawString("Press B for menu", dp.result_sprite_width / 2, dp.result_msg_y_in_sprite, 1);

      result_sprite.pushSprite(dp.result_sprite_x, dp.result_sprite_y);

      while (true) {
        ButtonState btns = input.get();
        if (btns.B) return;
        delay(20);
      }
    }

    void start() {
      while (true) {
        screen.fillScreen(TFT_BLACK);
        menu();

        // wait for A button is released
        while (true) {
          ButtonState btns = input.get();
          if (!btns.A) break;
          delay(10);
        }

        Game* game = new Game(cur_focus_mode, input, screen, dp);
        GameResult result = game->start();
        delete game;
        delay(1000);
        show_result(result);
        delay(100);
      }
    }
};
