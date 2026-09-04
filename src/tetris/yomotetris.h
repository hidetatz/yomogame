#pragma once

#include <atomic>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <TFT_eSPI.h>

#include <input.h>
#include <audio.h>
#include <volume_overlay.h>

#include "tetris_buffer.h"
#include "tetris_common.h"
#include "tetris_logic.h"
#include "tetris_rendering.h"
#include "tetris_sound.h"

class Game {
  public:
    TripleBuffer tb;
    SemaphoreHandle_t logic_done_sem;
    std::atomic<bool> logic_running{true};
    GameLogic logic;
    GameRenderer renderer;
    TaskHandle_t logic_task_handle;
    TaskHandle_t render_task_handle;

    Game(GameMode mode, int level, Sound& sound, volui::VolumeOverlay& vol_overlay, int garbage_lines, input::Buttons &buttons, TFT_eSPI &screen, DisplayParameters &params) :
      logic_done_sem(xSemaphoreCreateBinary()),
      logic(tb, mode, level, sound, garbage_lines, buttons, logic_done_sem),
      renderer(tb, screen, params, vol_overlay, logic_running),
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

      // wait for rendering finishes
      unsigned long wait_started = millis();
      while (tb.rendered_seq < tb.published_seq) {
        if (millis() - wait_started > 500) break; // timeout
        vTaskDelay(pdMS_TO_TICKS(2));
      }

      logic_running = false;

      return *logic.final_result;
    }

    static void logic_task_trampoline(void* param) {
      static_cast<Game*>(param)->logic.main_loop();
    }

    static void render_task_trampoline(void* param) {
      static_cast<Game*>(param)->renderer.rendering_loop_60fps();
    }
};

enum class MenuFocusedItem {
  MODE, STARTING_LEVEL, GARBAGE_LINES
};

class YomoTetris {
  public:
    GameMode selected_mode{GameMode::L99999};
    int selected_starting_level{1};
    int selected_garbage_lines{0};
    MenuFocusedItem focused_item{MenuFocusedItem::MODE};
    DisplayParameters dp;
    Sound sound;
    input::Buttons buttons;
    TFT_eSPI &screen;
    TFT_eSprite menu_sprite;
    TFT_eSprite result_sprite;
    volui::VolumeOverlay vol_overlay;

    YomoTetris(audio::Audio& audio, input::Buttons buttons, TFT_eSPI &screen, DisplayParameters params) :
      dp(params),
      sound(audio),
      buttons(buttons),
      screen(screen),
      menu_sprite(&screen),
      result_sprite(&screen),
      vol_overlay(audio, 50, 10, 5, 230)
      {
        menu_sprite.createSprite(dp.menu_sprite_width, dp.menu_sprite_height);
        result_sprite.createSprite(dp.result_sprite_width, dp.result_sprite_height);
      }

    void render_menubox(TFT_eSprite& sprite, std::string str, int x, int y, int width, int height, uint8_t font, boolean focused, boolean selected) {
      // neither focused nor selected
      uint16_t grid_color = TFT_DARKGREY;
      uint16_t bg_color = TFT_BLACK;
      uint16_t char_color = TFT_DARKGREY;

      if (focused && selected) {
        grid_color = TFT_WHITE;
        bg_color = TFT_ORANGE;
        char_color = TFT_BLACK;
      } else if (focused) {
        grid_color = TFT_WHITE;
        bg_color = TFT_BLACK;
        char_color = TFT_WHITE;
      } else if (selected) {
        grid_color = TFT_DARKGREY;
        bg_color = TFT_DARKGREY;
        char_color = TFT_WHITE;
      }

      sprite.drawRect(x, y, width, height, grid_color);
      sprite.fillRect(x+1, y+1, width-2, height-2, bg_color);
      sprite.setTextColor(char_color, bg_color);
      sprite.setTextDatum(MC_DATUM);
      sprite.drawString(str.c_str(), x + (width / 2), y + (height / 2), font);
    }

    void render_menu_label(TFT_eSprite& sprite, std::string str, int x, int y, uint8_t font, boolean focused) {
      uint16_t char_color = focused ? TFT_ORANGE : TFT_DARKGREY;
      sprite.setTextColor(char_color, TFT_BLACK);
      sprite.setTextDatum(TL_DATUM);
      sprite.drawString(str.c_str(), x, y, font);
    }

    void render_menu_title(TFT_eSprite& sprite, int x, int y, uint8_t font) {
      sprite.setTextColor(TFT_CYAN, TFT_BLACK);
      sprite.setTextDatum(TC_DATUM);
      sprite.drawString("YomoTetris", x, y, font);
    }

    void render_menu_msg(TFT_eSprite& sprite, int x, int y, uint8_t font) {
      sprite.setTextColor(TFT_GREEN, TFT_BLACK);
      sprite.setTextDatum(TC_DATUM);
      sprite.drawString("PRESS A TO START", x, y, font);
    }

    void menu() {
      render_menu_title(menu_sprite, dp.menu_title_x_center_in_sprite, dp.menu_title_y_in_sprite, 2);
      render_menu_msg(menu_sprite, dp.menu_msg_x_center_in_sprite, dp.menu_msg_y_in_sprite, 2);

      input::ButtonState prev_state = buttons.get();

      MenuFocusedItem focused_item = MenuFocusedItem::MODE;

      while (true) {
        render_menu_label(menu_sprite, "Mode", dp.menu_mode_label_x_in_sprite, dp.menu_mode_label_y_in_sprite, 2, focused_item == MenuFocusedItem::MODE);

        render_menubox(menu_sprite, "99999", dp.menu_mode_l99999_x_in_sprite,    dp.menu_mode_value_y_in_sprite, dp.menu_mode_value_width, dp.menu_mode_value_height, 2, focused_item == MenuFocusedItem::MODE, selected_mode == GameMode::L99999);
        render_menubox(menu_sprite, "150", dp.menu_mode_l150_x_in_sprite,    dp.menu_mode_value_y_in_sprite, dp.menu_mode_value_width, dp.menu_mode_value_height, 2, focused_item == MenuFocusedItem::MODE, selected_mode == GameMode::L150);
        render_menubox(menu_sprite, "40",  dp.menu_mode_l40_x_in_sprite,     dp.menu_mode_value_y_in_sprite, dp.menu_mode_value_width, dp.menu_mode_value_height, 2, focused_item == MenuFocusedItem::MODE, selected_mode == GameMode::L40);

        render_menu_label(menu_sprite, "Starting Level", dp.menu_level_label_x_in_sprite,   dp.menu_level_label_y_in_sprite, 2,   focused_item == MenuFocusedItem::STARTING_LEVEL);
        render_menubox(menu_sprite, "1",  dp.menu_level_1_x_in_sprite,  dp.menu_level_value_y_in_sprite, dp.menu_level_value_width, dp.menu_level_value_height, 2, focused_item == MenuFocusedItem::STARTING_LEVEL, selected_starting_level == 1);
        render_menubox(menu_sprite, "10", dp.menu_level_10_x_in_sprite, dp.menu_level_value_y_in_sprite, dp.menu_level_value_width, dp.menu_level_value_height, 2, focused_item == MenuFocusedItem::STARTING_LEVEL, selected_starting_level == 10);
        render_menubox(menu_sprite, "20", dp.menu_level_20_x_in_sprite, dp.menu_level_value_y_in_sprite, dp.menu_level_value_width, dp.menu_level_value_height, 2, focused_item == MenuFocusedItem::STARTING_LEVEL, selected_starting_level == 20);

        render_menu_label(menu_sprite, "Garbage Lines",  dp.menu_garbage_label_x_in_sprite, dp.menu_garbage_label_y_in_sprite, 2, focused_item == MenuFocusedItem::GARBAGE_LINES);
        render_menubox(menu_sprite, "0",  dp.menu_garbage_0_x_in_sprite,  dp.menu_garbage_value_y_in_sprite, dp.menu_garbage_value_width, dp.menu_garbage_value_height, 2, focused_item == MenuFocusedItem::GARBAGE_LINES, selected_garbage_lines == 0);
        render_menubox(menu_sprite, "6",  dp.menu_garbage_6_x_in_sprite,  dp.menu_garbage_value_y_in_sprite, dp.menu_garbage_value_width, dp.menu_garbage_value_height, 2, focused_item == MenuFocusedItem::GARBAGE_LINES, selected_garbage_lines == 6);
        render_menubox(menu_sprite, "12", dp.menu_garbage_12_x_in_sprite, dp.menu_garbage_value_y_in_sprite, dp.menu_garbage_value_width, dp.menu_garbage_value_height, 2, focused_item == MenuFocusedItem::GARBAGE_LINES, selected_garbage_lines == 12);

        input::ButtonState btns = buttons.get();
        if (btns.A && !prev_state.A) return;

        if (btns.UP && !prev_state.UP) {
          if (focused_item == MenuFocusedItem::STARTING_LEVEL) {
            sound.sound_cursor();
            focused_item = MenuFocusedItem::MODE;

          } else if (focused_item == MenuFocusedItem::GARBAGE_LINES) {
            sound.sound_cursor();
            focused_item = MenuFocusedItem::STARTING_LEVEL;
          }

        } else if (btns.DOWN && !prev_state.DOWN) {
          if (focused_item == MenuFocusedItem::MODE) {
            sound.sound_cursor();
            focused_item = MenuFocusedItem::STARTING_LEVEL;
          } else if (focused_item == MenuFocusedItem::STARTING_LEVEL) {
            sound.sound_cursor();
            focused_item = MenuFocusedItem::GARBAGE_LINES;
          }

        } else if (btns.LEFT && !prev_state.LEFT) {
          if (focused_item == MenuFocusedItem::MODE) {
            if (selected_mode == GameMode::L150) {
              sound.sound_cursor();
              selected_mode = GameMode::L99999;
            } else if (selected_mode == GameMode::L40) {
              sound.sound_cursor();
              selected_mode = GameMode::L150;
            }

          } else if (focused_item == MenuFocusedItem::STARTING_LEVEL) {
            if (selected_starting_level == 10) {
              sound.sound_cursor();
              selected_starting_level = 1;
            } else if (selected_starting_level == 20) {
              sound.sound_cursor();
              selected_starting_level = 10;
            }

          } else if (focused_item == MenuFocusedItem::GARBAGE_LINES) {
            if (selected_garbage_lines == 6) {
              sound.sound_cursor();
              selected_garbage_lines = 0;
            } else if (selected_garbage_lines == 12) {
              sound.sound_cursor();
              selected_garbage_lines = 6;
            }
          }
          
        } else if (btns.RIGHT && !prev_state.RIGHT) {
          if (focused_item == MenuFocusedItem::MODE) {
            if (selected_mode == GameMode::L99999) {
              sound.sound_cursor();
              selected_mode = GameMode::L150;
            } else if (selected_mode == GameMode::L150) {
              sound.sound_cursor();
              selected_mode = GameMode::L40;
            }

          } else if (focused_item == MenuFocusedItem::STARTING_LEVEL) {
            if (selected_starting_level == 1) {
              sound.sound_cursor();
              selected_starting_level = 10;
            } else if (selected_starting_level == 10) {
              sound.sound_cursor();
              selected_starting_level = 20;
            }

          } else if (focused_item == MenuFocusedItem::GARBAGE_LINES) {
            if (selected_garbage_lines == 0) {
              sound.sound_cursor();
              selected_garbage_lines = 6;
            } else if (selected_garbage_lines == 6) {
              sound.sound_cursor();
              selected_garbage_lines = 12;
            }
          }
        }
        prev_state = btns;

        menu_sprite.pushSprite(dp.menu_sprite_x, dp.menu_sprite_y);

        vol_overlay.tick(screen, TFT_BLACK);

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
      std::string code_str = result.code == GameResultCode::Clear ? "Clear!!" : result.code == GameResultCode::Fail ? "Fail..." : "Canceled";
      uint16_t result_color = result.code == GameResultCode::Clear ? TFT_GREEN : result.code == GameResultCode::Fail ? TFT_ORANGE : TFT_WHITE;

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
        input::ButtonState btns = buttons.get();
        if (btns.B) return;
        delay(20);
      }
    }

    void start() {
      sound.begin();
      while (true) {
        screen.fillScreen(TFT_BLACK);
        menu();
        Game* game = new Game(selected_mode, selected_starting_level, sound, vol_overlay, selected_garbage_lines, buttons, screen, dp);

        // countdown
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setTextDatum(MC_DATUM);

        sound.sound_countdown();
        screen.drawString("3", dp.menu_sprite_width / 2, dp.menu_sprite_height / 2, 2);
        delay(700);

        sound.sound_countdown();
        screen.drawString("2", dp.menu_sprite_width / 2, dp.menu_sprite_height / 2, 2);
        delay(700);

        sound.sound_countdown();
        screen.drawString("1", dp.menu_sprite_width / 2, dp.menu_sprite_height / 2, 2);
        delay(700);

        sound.start_bgm();
        GameResult result = game->start();
        sound.stop_bgm();

        delete game;
        if (result.code != GameResultCode::Cancel) {
          delay(500);
          if (result.code == GameResultCode::Clear) sound.sound_success();
          else if (result.code == GameResultCode::Fail) sound.sound_fail();
          show_result(result);
        }
        delay(100);
      }
    }
};
