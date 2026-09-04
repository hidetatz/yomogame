#pragma once

#include <atomic>
#include <string>
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
 
#include <TFT_eSPI.h>

#include <yomogi.h>

#include "tetris_common.h"
#include "tetris_buffer.h"

struct DisplayParameters {
  // menu and result
  const int menu_sprite_x;
  const int menu_sprite_y;
  const int menu_sprite_width;
  const int menu_sprite_height;
  const int menu_title_x_center_in_sprite;
  const int menu_title_y_in_sprite;
  const int menu_mode_label_x_in_sprite;
  const int menu_mode_label_y_in_sprite;
  const int menu_mode_l99999_x_in_sprite;
  const int menu_mode_l150_x_in_sprite;
  const int menu_mode_l40_x_in_sprite;
  const int menu_mode_value_y_in_sprite;
  const int menu_mode_value_width;
  const int menu_mode_value_height;
  const int menu_level_label_x_in_sprite;
  const int menu_level_label_y_in_sprite;
  const int menu_level_1_x_in_sprite;
  const int menu_level_10_x_in_sprite;
  const int menu_level_20_x_in_sprite;
  const int menu_level_value_y_in_sprite;
  const int menu_level_value_width;
  const int menu_level_value_height;
  const int menu_garbage_label_x_in_sprite;
  const int menu_garbage_label_y_in_sprite;
  const int menu_garbage_0_x_in_sprite;
  const int menu_garbage_6_x_in_sprite;
  const int menu_garbage_12_x_in_sprite;
  const int menu_garbage_value_y_in_sprite;
  const int menu_garbage_value_width;
  const int menu_garbage_value_height;
  const int menu_msg_x_center_in_sprite;
  const int menu_msg_y_in_sprite;

  const int result_sprite_x;
  const int result_sprite_y;
  const int result_sprite_width;
  const int result_sprite_height;
  const int result_label_x_in_sprite;
  const int result_value_x_right_in_sprite;
  const int result_result_y_in_sprite;
  const int result_score_y_in_sprite;
  const int result_time_y_in_sprite;
  const int result_lines_y_in_sprite;
  const int result_level_y_in_sprite;
  const int result_tetris_y_in_sprite;
  const int result_tspins_y_in_sprite;
  const int result_maxcombos_y_in_sprite;
  const int result_holds_y_in_sprite;
  const int result_tpm_y_in_sprite;
  const int result_lpm_y_in_sprite;
  const int result_msg_y_in_sprite;

  // hold label
  const int hold_label_font;
  const int hold_label_x_center;
  const int hold_label_y;

  // hold box
  const int hold_box_width;
  const int hold_box_height;
  const int hold_box_x;
  const int hold_box_y;

  // hold sprite
  const int hold_sprite_width;
  const int hold_sprite_height;
  const int hold_sprite_x;
  const int hold_sprite_y;
  const int hold_mino_block_size;
  const int hold_mino_bevel;

  const int stats_font;
  const int stats_label_x;
  const int score_time_x_right;

  // score
  const int score_label_y;
  const int score_y;

  // time
  const int time_label_y;
  const int time_y;

  // stats

  // stats labels
  const int goal_label_y;
  const int level_label_y;
  const int tetris_label_y;
  const int tspin_label_y;
  const int combo_label_y;
  const int tpm_label_y;
  const int lpm_label_y;

  const std::string goal_label;
  const std::string level_label;
  const std::string tetris_label;
  const std::string tspin_label;
  const std::string combo_label;
  const std::string tpm_label;
  const std::string lpm_label;

  // stats values
  const int stats_x_right_in_sprite;
  const int stats_sprite_width;
  const int stats_sprite_height;
  const int stats_sprite_x;
  const int stats_sprite_y;

  const int goal_y_in_sprite;
  const int level_y_in_sprite;
  const int tetris_y_in_sprite;
  const int tspin_y_in_sprite;
  const int combo_y_in_sprite;
  const int tpm_y_in_sprite;
  const int lpm_y_in_sprite;

  // board box
  const int board_box_width;
  const int board_box_height;
  const int board_box_x;
  const int board_box_y;

  // board
  const int board_sprite_width;
  const int board_sprite_height;
  const int board_sprite_x;
  const int board_sprite_y;
  const int board_mino_block_size;
  const int board_mino_bevel;

  // next minos label
  const int next_minos_label_font;
  const int next_minos_label_x_center;
  const int next_minos_label_y;

  // next minos box
  const int next_minos_box_width;
  const int next_minos_box_height;
  const int next_minos_box_x;
  const int next_minos_box_y;

  // next minos sprite
  const int next_minos_sprite_width;
  const int next_minos_sprite_height;
  const int next_minos_sprite_x;
  const int next_minos_sprite_y;
  const int next_minos_mino_block_size;
  const int next_minos_mino_bevel;
  const int next_minos_mino_y0_in_sprite;
  const int next_minos_mino_y1_in_sprite;
  const int next_minos_mino_y2_in_sprite;
  const int next_minos_mino_y3_in_sprite;
  const int next_minos_mino_y4_in_sprite;
  const int next_minos_mino_y5_in_sprite;

  const int pause_sprite_width;
  const int pause_sprite_height;
  const int pause_sprite_x;
  const int pause_sprite_y;
  const int pause_sprite_title_y_in_sprite;
  const int pause_sprite_resume_y_in_sprite;
  const int pause_sprite_quit_y_in_sprite;

  // yomogi image
  const int yomogi_width;
  const int yomogi_height;
  const int yomogi_x;
  const int yomogi_y;
  const uint16_t* yomogi_image;
  const uint16_t yomogi_transparent_color;
};

const DisplayParameters disp_param_240x240 {
  // menu
  .menu_sprite_x = 0,
  .menu_sprite_y = 0,
  .menu_sprite_width = 240,
  .menu_sprite_height = 240,

  .menu_title_x_center_in_sprite = 120,
  .menu_title_y_in_sprite = 13,

  .menu_mode_label_x_in_sprite = 15,
  .menu_mode_label_y_in_sprite = 43,
  .menu_mode_l99999_x_in_sprite = 15,
  .menu_mode_l150_x_in_sprite = 88,
  .menu_mode_l40_x_in_sprite = 161,
  .menu_mode_value_y_in_sprite = 63,
  .menu_mode_value_width = 64,
  .menu_mode_value_height = 23,

  .menu_level_label_x_in_sprite = 15,
  .menu_level_label_y_in_sprite = 97,
  .menu_level_1_x_in_sprite = 15,
  .menu_level_10_x_in_sprite = 88,
  .menu_level_20_x_in_sprite = 161,
  .menu_level_value_y_in_sprite = 117,
  .menu_level_value_width = 64,
  .menu_level_value_height = 23,

  .menu_garbage_label_x_in_sprite = 15,
  .menu_garbage_label_y_in_sprite = 153,
  .menu_garbage_0_x_in_sprite = 15,
  .menu_garbage_6_x_in_sprite = 88,
  .menu_garbage_12_x_in_sprite = 161,
  .menu_garbage_value_y_in_sprite = 173,
  .menu_garbage_value_width = 64,
  .menu_garbage_value_height = 23,
  .menu_msg_x_center_in_sprite = 120,
  .menu_msg_y_in_sprite = 210,

  .result_sprite_x = 45,
  .result_sprite_y = 35,
  .result_sprite_width = 150,
  .result_sprite_height = 169,
  .result_label_x_in_sprite = 15,
  .result_value_x_right_in_sprite = 135,
  .result_result_y_in_sprite = 10,
  .result_score_y_in_sprite = 23,
  .result_time_y_in_sprite = 35,
  .result_lines_y_in_sprite = 48,
  .result_level_y_in_sprite = 61,
  .result_tetris_y_in_sprite = 74,
  .result_tspins_y_in_sprite = 87,
  .result_maxcombos_y_in_sprite = 100,
  .result_holds_y_in_sprite = 113,
  .result_tpm_y_in_sprite = 126,
  .result_lpm_y_in_sprite = 139,
  .result_msg_y_in_sprite = 152,

  // hold label
  .hold_label_font = 2,
  .hold_label_x_center = 32,
  .hold_label_y = 9,

  // hold box 1:1
  .hold_box_width = 42,
  .hold_box_height = 42,
  .hold_box_x = 11,
  .hold_box_y = 27,

  // hold sprite 2:1
  .hold_sprite_width = 32,
  .hold_sprite_height = 16,
  .hold_sprite_x = 16,
  .hold_sprite_y = 40,
  .hold_mino_block_size = 8,
  .hold_mino_bevel = 2,

  .stats_font = 1,
  .stats_label_x = 6,
  .score_time_x_right = 59,

  // score
  .score_label_y = 80,
  .score_y = 93,

  // time
  .time_label_y = 106,
  .time_y = 119,

  // stats

  // stats labels

  .goal_label_y = 132,
  .level_label_y = 145,
  .tetris_label_y = 158,
  .tspin_label_y = 171,
  .combo_label_y = 184,
  .tpm_label_y = 197,
  .lpm_label_y = 210,

  .goal_label = "GOL",
  .level_label = "LVL",
  .tetris_label = "TET",
  .tspin_label = "TSP",
  .combo_label = "REN",
  .tpm_label = "TPM",
  .lpm_label = "LPM",

  // stats values
  .stats_x_right_in_sprite = 32,
  .stats_sprite_width = 32,
  .stats_sprite_height = 85,
  .stats_sprite_x = 27,
  .stats_sprite_y = 132,

  .goal_y_in_sprite = 0,
  .level_y_in_sprite = 13,
  .tetris_y_in_sprite = 26,
  .tspin_y_in_sprite = 39,
  .combo_y_in_sprite = 52,
  .tpm_y_in_sprite = 65,
  .lpm_y_in_sprite = 78,

  // board box
  .board_box_width = 112,
  .board_box_height = 222,
  .board_box_x = 64,
  .board_box_y = 9,

  // board
  .board_sprite_width = 110,
  .board_sprite_height = 220,
  .board_sprite_x = 65,
  .board_sprite_y = 10,
  .board_mino_block_size = 11,
  .board_mino_bevel = 2,

  // next minos label
  .next_minos_label_font = 2,
  .next_minos_label_x_center = 208,
  .next_minos_label_y = 9,

  // next minos box
  .next_minos_box_width = 42,
  .next_minos_box_height = 140,
  .next_minos_box_x = 187,
  .next_minos_box_y = 27,

  // next minos sprite
  .next_minos_sprite_width = 32,
  .next_minos_sprite_height = 126,
  .next_minos_sprite_x = 192,
  .next_minos_sprite_y = 34,
  .next_minos_mino_block_size = 8,
  .next_minos_mino_bevel = 2,
  .next_minos_mino_y0_in_sprite = 0,
  .next_minos_mino_y1_in_sprite = 22,
  .next_minos_mino_y2_in_sprite = 44,
  .next_minos_mino_y3_in_sprite = 66,
  .next_minos_mino_y4_in_sprite = 88,
  .next_minos_mino_y5_in_sprite = 110,

  // pause
  .pause_sprite_width = 100,
  .pause_sprite_height = 88,
  .pause_sprite_x = 70,
  .pause_sprite_y = 76,
  .pause_sprite_title_y_in_sprite = 10,
  .pause_sprite_resume_y_in_sprite = 36,
  .pause_sprite_quit_y_in_sprite = 62,

  .yomogi_width = YOMOGI_240X240_WIDTH,
  .yomogi_height = YOMOGI_240X240_HEIGHT,
  .yomogi_x = 183,
  .yomogi_y = 187,
  .yomogi_image = yomogi_240x240,
  .yomogi_transparent_color = YOMOGI_240X240_TRANSPARENT
};

class GameRenderer {
  public:
    TripleBuffer &tb;
    DisplayParameters &dp;
    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;
    TFT_eSprite next_minos_sprite;
    TFT_eSprite hold_sprite;
    TFT_eSprite stats_sprite;
    TFT_eSprite pause_sprite;
    std::atomic<bool> &running;

    GameRenderer(TripleBuffer &tb, TFT_eSPI &screen, DisplayParameters &dp, std::atomic<bool> &running) :
      tb(tb),
      dp(dp),
      bgcolor(TFT_BLACK),
      screen(screen),
      board_sprite(&screen),
      next_minos_sprite(&screen),
      hold_sprite(&screen),
      stats_sprite(&screen),
      pause_sprite(&screen),
      running(running)
      {}

    void setup_screen() {
      screen.fillScreen(bgcolor);

      /* hold area */
      hold_sprite.createSprite(dp.hold_sprite_width, dp.hold_sprite_height);
      render_square(dp.hold_box_x, dp.hold_box_y, dp.hold_box_width, dp.hold_box_height);
      render_centered_label("Hold", dp.hold_label_x_center, dp.hold_label_y, dp.hold_label_font);

      /* score area */
      render_left_label("Score", dp.stats_label_x, dp.score_label_y, dp.stats_font);

      /* time area */
      render_left_label("Time", dp.stats_label_x, dp.time_label_y, dp.stats_font);

      /* stats labels area */
      stats_sprite.createSprite(dp.stats_sprite_width, dp.stats_sprite_height);
      render_left_label(dp.goal_label,   dp.stats_label_x, dp.goal_label_y,   dp.stats_font);
      render_left_label(dp.level_label,  dp.stats_label_x, dp.level_label_y,  dp.stats_font);
      render_left_label(dp.tetris_label, dp.stats_label_x, dp.tetris_label_y, dp.stats_font);
      render_left_label(dp.tspin_label,  dp.stats_label_x, dp.tspin_label_y,  dp.stats_font);
      render_left_label(dp.combo_label,  dp.stats_label_x, dp.combo_label_y,  dp.stats_font);
      render_left_label(dp.tpm_label,    dp.stats_label_x, dp.tpm_label_y,    dp.stats_font);
      render_left_label(dp.lpm_label,    dp.stats_label_x, dp.lpm_label_y,    dp.stats_font);

      /* board area */
      board_sprite.createSprite(dp.board_sprite_width, dp.board_sprite_height);
      render_square(dp.board_box_x, dp.board_box_y, dp.board_box_width, dp.board_box_height);

      /* next_minos area */
      next_minos_sprite.createSprite(dp.next_minos_sprite_width, dp.next_minos_sprite_height);
      render_square(dp.next_minos_box_x, dp.next_minos_box_y, dp.next_minos_box_width, dp.next_minos_box_height);
      render_centered_label("Next", dp.next_minos_label_x_center, dp.next_minos_label_y, dp.next_minos_label_font);

      pause_sprite.createSprite(dp.pause_sprite_width, dp.pause_sprite_height);

      /* yomogi area */
      screen.setSwapBytes(true);
      screen.pushImage(dp.yomogi_x, dp.yomogi_y, dp.yomogi_width, dp.yomogi_height, dp.yomogi_image, dp.yomogi_transparent_color);
    }

    void render_frame() {
      GameSnapshot& snap = tb.read_buf();
      if (snap.valid) render_from_snapshot(snap);
    }

    /*
     * rendering
     */

    void render_square(int top_left_x, int top_left_y, int width, int height) {
      screen.drawFastHLine(top_left_x,             top_left_y,              width,      TFT_WHITE); // top left to right
      screen.drawFastHLine(top_left_x,             top_left_y + height - 1, width,      TFT_WHITE); // bottom left to right
      screen.drawFastVLine(top_left_x,             top_left_y + 1,          height - 2, TFT_WHITE); // top left to down
      screen.drawFastVLine(top_left_x + width - 1, top_left_y + 1,          height - 2, TFT_WHITE); // top right to down
    }

    /* string */

    void render_centered_label(std::string string, int x, int y, uint8_t font) {
      screen.setTextColor(TFT_WHITE, bgcolor);
      screen.setTextDatum(TC_DATUM);
      screen.drawString(string.c_str(), x, y, font);
    }

    void render_left_label(std::string string, int x, int y, uint8_t font) {
      screen.setTextColor(TFT_WHITE, bgcolor);
      screen.setTextDatum(TL_DATUM);
      screen.drawString(string.c_str(), x, y, font);
    }

    void render_right_label(std::string string, int x, int y, uint8_t font) {
      screen.setTextColor(TFT_WHITE, bgcolor);
      screen.setTextDatum(TR_DATUM);
      screen.drawString(string.c_str(), x, y, font);
    }

    void render_right_label_sprite(TFT_eSprite& sprite, std::string string, int x, int y, uint8_t font) {
      sprite.setTextColor(TFT_WHITE, bgcolor);
      sprite.setTextDatum(TR_DATUM);
      sprite.drawString(string.c_str(), x, y, font);
    }

    /* minos and blocks */

    void render_empty_block(TFT_eSprite& sprite, int row, int col, int block_size) {
      sprite.fillRect(col * block_size, row * block_size, block_size, block_size, bgcolor);
    }

    void render_block(TFT_eSprite& sprite, int row, int col, int x_offset, int y_offset, int block_size, int bevel_size, uint16_t base_color, uint16_t lighter_color, uint16_t darker_color) {
      sprite.fillRect(x_offset + col * block_size,              y_offset + row * block_size,              block_size,                block_size,                darker_color);
      sprite.fillRect(x_offset + col * block_size,              y_offset + row * block_size,              block_size - bevel_size,   block_size - bevel_size,   lighter_color);
      sprite.fillRect(x_offset + col * block_size + bevel_size, y_offset + row * block_size + bevel_size, block_size - bevel_size*2, block_size - bevel_size*2, base_color);
    }

    void render_ghost_block(TFT_eSprite& sprite, int row, int col, int block_size, uint16_t color) {
      // render doubled lines for visibility
      sprite.drawRect(col * block_size,     row * block_size,     block_size,     block_size,     color);
      sprite.drawRect(col * block_size + 1, row * block_size + 1, block_size - 2, block_size - 2, color);
    }

    void render_mino_on_sprite_with_offset(TFT_eSprite& sprite, Mino m, int x_offset, int y_offset, int block_size, int bevel_size) {
      for (int i = 0; i < 4; i++) render_block(sprite, m.positions[i].row, m.positions[i].col, x_offset, y_offset, block_size, bevel_size, m.color.base, m.color.lighter, m.color.darker);
    }

    void render_mino_on_next_minos(TFT_eSprite& sprite, Mino m, int x_offset, int y_offset) {
      render_mino_on_sprite_with_offset(sprite, m, x_offset, y_offset, dp.next_minos_mino_block_size, dp.next_minos_mino_bevel);
    }

    void render_mino_on_hold_box(Mino m, int x_offset, int y_offset) {
      render_mino_on_sprite_with_offset(hold_sprite, m, x_offset, y_offset, dp.hold_mino_block_size, dp.hold_mino_bevel);
    }

    void render_mino_data_on_sprite(TFT_eSprite& sprite, const std::array<BlockPos,4>& positions, const BlockColor& c, int x_offset, int y_offset, int block_size, int bevel_size) {
      for (int i = 0; i < 4; i++) {
        render_block(sprite, positions[i].row, positions[i].col, x_offset, y_offset, block_size, bevel_size, c.base, c.lighter, c.darker);
      }
    }

    void render_ghost_from_data(const std::array<BlockPos,4>& positions, uint16_t color, int hard_drop_distance) {
      for (int i = 0; i < 4; i++) {
        render_ghost_block(board_sprite, positions[i].row + hard_drop_distance, positions[i].col, dp.board_mino_block_size, color);
      }
    }

    void render_pause_modal(PauseOption selected) {
      pause_sprite.fillRect(0, 0, dp.pause_sprite_width, dp.pause_sprite_height, TFT_BLACK);
      pause_sprite.drawRect(0, 0, dp.pause_sprite_width, dp.pause_sprite_height, TFT_WHITE);

      pause_sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      pause_sprite.setTextDatum(TC_DATUM);
      pause_sprite.drawString("PAUSE", dp.pause_sprite_width / 2, dp.pause_sprite_title_y_in_sprite, 2);

      uint16_t resume_color = (selected == PauseOption::RESUME) ? TFT_YELLOW : TFT_WHITE;
      uint16_t quit_color    = (selected == PauseOption::QUIT) ? TFT_YELLOW : TFT_WHITE;

      pause_sprite.setTextColor(resume_color, TFT_BLACK);
      pause_sprite.drawString("Resume", dp.pause_sprite_width / 2, dp.pause_sprite_resume_y_in_sprite, 2);

      pause_sprite.setTextColor(quit_color, TFT_BLACK);
      pause_sprite.drawString("Quit to Menu", dp.pause_sprite_width / 2, dp.pause_sprite_quit_y_in_sprite, 2);

      pause_sprite.pushSprite(dp.pause_sprite_x, dp.pause_sprite_y);
    }

    void render_from_snapshot(const GameSnapshot& snap) {
      if (snap.is_paused) {
        render_pause_modal(snap.pause_selected);
      } else {
        // hold
        hold_sprite.fillSprite(bgcolor);
        if (snap.has_hold_mino) {
          Mino m = Mino::for_hold_box(snap.hold_type);
          int x_offset = m.is_I() ? 0 : m.is_O() ? dp.hold_mino_block_size : dp.hold_mino_block_size / 2;
          int y_offset = m.is_I() ? (dp.hold_mino_block_size / 2) : dp.hold_mino_block_size;
          m.color = BlockColor{snap.hold_color.base, snap.hold_color.lighter, snap.hold_color.darker};
          render_mino_on_hold_box(m, x_offset, y_offset);
          hold_sprite.pushSprite(dp.hold_sprite_x, dp.hold_sprite_y);
        }

        // score
        render_right_label(std::to_string(snap.score), dp.score_time_x_right, dp.score_y, dp.stats_font);

        /* time */
        int minutes = snap.elapsed_ms / (1000 * 60);
        int seconds = (snap.elapsed_ms / 1000) % 60;
        int centis  = (snap.elapsed_ms % 1000) / 10;
        char time_str[9];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", minutes, seconds, centis);
        render_right_label(time_str, dp.score_time_x_right, dp.time_y, dp.stats_font);

        /* stats */
        stats_sprite.fillRect(0, 0, dp.stats_sprite_width, dp.stats_sprite_height, bgcolor);
        char tpm_str[6], lpm_str[6];
        snprintf(tpm_str, sizeof(tpm_str), "%.1f", snap.tpm);
        snprintf(lpm_str, sizeof(lpm_str), "%.1f", snap.lpm);

        render_right_label_sprite(stats_sprite, std::to_string(snap.goal),                          dp.stats_x_right_in_sprite, dp.goal_y_in_sprite,   dp.stats_font);
        render_right_label_sprite(stats_sprite, std::to_string(snap.level),                         dp.stats_x_right_in_sprite, dp.level_y_in_sprite,  dp.stats_font);
        render_right_label_sprite(stats_sprite, std::to_string(snap.tetris_count),                  dp.stats_x_right_in_sprite, dp.tetris_y_in_sprite, dp.stats_font);
        render_right_label_sprite(stats_sprite, std::to_string(snap.tspins),                        dp.stats_x_right_in_sprite, dp.tspin_y_in_sprite,  dp.stats_font);
        render_right_label_sprite(stats_sprite, std::to_string(snap.combos < 0 ? 0 : snap.combos),  dp.stats_x_right_in_sprite, dp.combo_y_in_sprite,  dp.stats_font);
        render_right_label_sprite(stats_sprite, tpm_str,                                            dp.stats_x_right_in_sprite, dp.tpm_y_in_sprite,    dp.stats_font);
        render_right_label_sprite(stats_sprite, lpm_str,                                            dp.stats_x_right_in_sprite, dp.lpm_y_in_sprite,    dp.stats_font);
        stats_sprite.pushSprite(dp.stats_sprite_x, dp.stats_sprite_y);

        /* board */
        board_sprite.fillSprite(bgcolor);
        for (int row = 0; row < 20; row++) {
          for (int col = 0; col < 10; col++) {
            if (!snap.blocks[row][col].has_value()) {
              render_empty_block(board_sprite, row, col, dp.board_mino_block_size);
            } else {
              auto& c = *snap.blocks[row][col];
              render_block(board_sprite, row, col, 0, 0, dp.board_mino_block_size, dp.board_mino_bevel, c.base, c.lighter, c.darker);
            }
          }
        }
        if (snap.has_cur_mino) {
          render_ghost_from_data(snap.cur_mino_block_pos, snap.cur_mino_color.base, snap.hard_drop_distance);
          render_mino_data_on_sprite(board_sprite, snap.cur_mino_block_pos, snap.cur_mino_color, 0, 0, dp.board_mino_block_size, dp.board_mino_bevel);
        }
        board_sprite.pushSprite(dp.board_sprite_x, dp.board_sprite_y);

        next_minos_sprite.fillSprite(bgcolor);
        for (int i = 0; i < 6; i++) {
          Mino m = Mino::for_next_minos(snap.next_types[i]);
          int x_offset = m.is_I() ? 0 : m.is_O() ? dp.next_minos_mino_block_size : dp.next_minos_mino_block_size / 2;
          int y_offset = m.is_I() ? (dp.next_minos_mino_block_size / 2) : dp.next_minos_mino_block_size;
          int y_in_sprite =
            i == 0 ? dp.next_minos_mino_y0_in_sprite :
            i == 1 ? dp.next_minos_mino_y1_in_sprite :
            i == 2 ? dp.next_minos_mino_y2_in_sprite :
            i == 3 ? dp.next_minos_mino_y3_in_sprite :
            i == 4 ? dp.next_minos_mino_y4_in_sprite : dp.next_minos_mino_y5_in_sprite;
          render_mino_on_next_minos(next_minos_sprite, m, x_offset, y_in_sprite + y_offset);
        }
        next_minos_sprite.pushSprite(dp.next_minos_sprite_x, dp.next_minos_sprite_y);
      }
    }
};

