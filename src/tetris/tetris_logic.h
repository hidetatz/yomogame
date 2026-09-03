#pragma once
 
#include <array>
#include <cmath>
#include <optional>
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
 
#include <Arduino.h>
 
#include <input.h>

#include "tetris_common.h"
#include "tetris_buffer.h"
#include "tetris_sound.h"

const int horizontal_move_first_wait_ms = 300;
const int horizontal_move_auto_repeating_wait_ms = 50;
const int lockdown_wait_ms = 500;
const int lockdown_reset_move_limit = 15;

enum class TSpinKind {
  TSPIN, TSPIN_MINI, NONE
};

class GameLogic {
  public:
    GameMode mode;
    int starting_level{1};

    Sound& sound;

    Board board;
    std::array<MinoType, 7> cur_bag;
    std::array<MinoType, 7> next_bag;
    int mino_idx{0};

    std::optional<Mino> hold_mino;

    // parameters
    int last_kick_index{0};
    boolean was_last_move_rotation{false};
    unsigned long last_moved_at{0};
    unsigned long last_horizontally_moved_at{0};
    unsigned long free_fall_timer{0};
    double free_fall_progress{0.0};
    boolean lockdown_judging{false};
    int move_cnt_while_lockdown_judging{0};
    boolean hold_once_tried{false};
    boolean in_b2b{false};

    // pause
    boolean paused{false};
    PauseOption pause_selected{PauseOption::RESUME};

    // stats
    unsigned long game_started_at{0};
    int mino_placed{0};
    int score{0};
    int removed_lines{0};
    int tetris_count{0};
    int tspins{0};
    int combos{-1}; // combos starts count when 2 consecutive clear happens, and it is counted as "1 combo", so it's good to start with -1
    int hold_count{0};
    int max_combos{0};

    // input
    input::Buttons &buttons;

    // buffer
    TripleBuffer &triple_buffer;

    // semaphore
    SemaphoreHandle_t done_sem;

    // result
    std::optional<GameResult> final_result;

  public:
    GameLogic(TripleBuffer &tb, GameMode mode, int level, Sound& sound, int garbage_lines, input::Buttons &buttons, SemaphoreHandle_t logic_done_sem) :
      mode(mode),
      starting_level(level),
      sound(sound),
      board(garbage_lines),
      cur_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      next_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      buttons(buttons),
      triple_buffer(tb),
      done_sem(logic_done_sem)
      {
        shuffle_bag(cur_bag);
        shuffle_bag(next_bag);
      }

    void shuffle_bag(std::array<MinoType, 7>& bag) {
      for (int i = 0; i < 7; i++) {
        int r = random(i, 7);
        MinoType temp = bag[i];
        bag[i] = bag[r];
        bag[r] = temp;
      }
    }

    Mino next_mino() {
      MinoType next = cur_bag[mino_idx];
      mino_idx++;
      if (mino_idx == 7) {
        cur_bag = next_bag;
        shuffle_bag(next_bag);
        mino_idx = 0;
      }
      return Mino::for_board(next);
    }

    MinoType next_mino_type(int offset) {
      int idx = mino_idx + offset;
      if (idx < 7) return cur_bag[idx];
      return next_bag[idx - 7];
    }

    double current_gravity_g() {
      int level = current_level();
      double frames = pow(60.0, (20.0 - level) / 19.0);
      double g = 1.0 / frames;
      return std::min(g, 20.0);
    }

    void apply_free_fall(unsigned long now) {
      unsigned long elapsed_ms = now - free_fall_timer;
      if (elapsed_ms == 0) return;

      double g = current_gravity_g();
      double cells_per_ms = g * 60.0 / 1000.0;
      free_fall_progress += elapsed_ms * cells_per_ms;
      free_fall_timer = now;

      int cells_to_drop = (int)free_fall_progress;
      if (cells_to_drop <= 0) return;
      free_fall_progress -= cells_to_drop;

      for (int i = 0; i < cells_to_drop; i++) {
        if (!try_move(MoveDirection::DOWN, 1, now)) {
          free_fall_progress = 0.0;
          break;
        }
      }
    }

    boolean try_place_mino(Mino m, unsigned long now) {
      if (!board.mino_placable(m)) return false;
      board.place_mino(m);
      free_fall_timer = now;
      free_fall_progress = 0.0;
      last_moved_at = now;
      lockdown_judging = false;
      move_cnt_while_lockdown_judging = 0;
      return true;
    }

    boolean try_move(MoveDirection dir, int distance, unsigned long now) {
      if (!board.can_move_mino(dir, distance)) return false;
      board.move_mino(dir, distance);
      if (lockdown_judging) {
        move_cnt_while_lockdown_judging++;
      }
      if (dir == MoveDirection::RIGHT || dir == MoveDirection::LEFT) {
        last_horizontally_moved_at = now;
      }
      if (dir == MoveDirection::DOWN) {
        free_fall_timer = now;
      }
      last_moved_at = now;
      was_last_move_rotation = false;
      return true;
    }

    boolean try_rotate(RotateDirection dir, unsigned long now) {
      auto [rotated, kick_index] = board.rotate(dir);
      if (!rotated) return false;
      last_kick_index = kick_index;
      if (lockdown_judging) move_cnt_while_lockdown_judging++;
      last_moved_at = now;
      was_last_move_rotation = true;
      return true;
    }

    TSpinKind check_tspin() {
      if (!board.cur_mino->is_T()) return TSpinKind::NONE;
      if (!was_last_move_rotation) return TSpinKind::NONE;
      // A and B are front side corner
      int A_row = 0;
      int A_col = 0;
      int B_row = 0;
      int B_col = 0;
      int C_row = 0;
      int C_col = 0;
      int D_row = 0;
      int D_col = 0;

      Pivot p = board.cur_mino->pivot;
      if (board.cur_mino->cur_direction == MinoDirection::NORTH) {
        A_row = p.row-1;
        A_col = p.col-1;
        B_row = p.row-1;
        B_col = p.col+1;
        C_row = p.row+1;
        C_col = p.col-1;
        D_row = p.row+1;
        D_col = p.col+1;
      } else if (board.cur_mino->cur_direction == MinoDirection::EAST) {
        C_row = p.row-1;
        C_col = p.col-1;
        A_row = p.row-1;
        A_col = p.col+1;
        D_row = p.row+1;
        D_col = p.col-1;
        B_row = p.row+1;
        B_col = p.col+1;
      } else if (board.cur_mino->cur_direction == MinoDirection::SOUTH) {
        D_row = p.row-1;
        D_col = p.col-1;
        C_row = p.row-1;
        C_col = p.col+1;
        B_row = p.row+1;
        B_col = p.col-1;
        A_row = p.row+1;
        A_col = p.col+1;
      } else {
        B_row = p.row-1;
        B_col = p.col-1;
        D_row = p.row-1;
        D_col = p.col+1;
        A_row = p.row+1;
        A_col = p.col-1;
        C_row = p.row+1;
        C_col = p.col+1;
      }

      boolean A_filled = !board.block_placable_at(A_row, A_col);
      boolean B_filled = !board.block_placable_at(B_row, B_col);
      boolean C_filled = !board.block_placable_at(C_row, C_col);
      boolean D_filled = !board.block_placable_at(D_row, D_col);

      int filled_count = 0;
      if (A_filled) filled_count++;
      if (B_filled) filled_count++;
      if (C_filled) filled_count++;
      if (D_filled) filled_count++;

      if (filled_count < 3) return TSpinKind::NONE;

      if ((A_filled && B_filled) || last_kick_index == 4) {
        return TSpinKind::TSPIN;
      }

      return TSpinKind::TSPIN_MINI;
    }

    int current_goal() {
      int goal = mode == GameMode::L40 ? 40 : mode == GameMode::L150 ? 150 : 99999;
      return goal - removed_lines;
    }

    boolean lock_mino_and_clear_lines() {
      // T-spin check
      TSpinKind tspin = check_tspin();
      if (tspin == TSpinKind::TSPIN || tspin == TSpinKind::TSPIN_MINI) tspins++;
      last_kick_index = 0; // reset last_kick_index for the next check

      if (!board.lockdown_mino()) return false;
      mino_placed++;
      if (hold_once_tried) hold_once_tried = false;

      // delete rows with animation
      auto [count, rows] = board.deletable_rows();

      int base_score = 0;
      if (tspin == TSpinKind::TSPIN) {
        if (count == 0) base_score = 400;
        else if (count == 1) base_score = 800;
        else if (count == 2) base_score = 1200;
        else if (count == 3) base_score = 1600;
      } else if (tspin == TSpinKind::TSPIN_MINI) {
        if (count == 0) base_score = 100;
        else if (count == 1) base_score = 200;
        else if (count == 2) base_score = 400;
      } else { // no tspin
        if (count == 1) base_score = 100;
        else if (count == 2) base_score = 300;
        else if (count == 3) base_score = 500;
        else if (count == 4) base_score = 800;
      }

      int cur_level = current_level();

      if (count == 0) {
        // if mino locked but no lines cleared, cancel combo
        combos = -1;

        // when no lines cleared, B2B, REN, Perfect check are not needed
        score += cur_level * base_score;
        return true;
      }

      if (count != 4) sound.sound_clear_lines_123();

      for (int i = 0; i < 3; i++) {
        // delete animation
        // render flashed (white) lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->flash();
          }
        }
        publish_snapshot();
        vTaskDelay(pdMS_TO_TICKS(30));
        // render original lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->stop_flash();
          }
        }
        publish_snapshot();
        vTaskDelay(pdMS_TO_TICKS(30));
      }
      board.clear_lines(rows, count);

      // if some lines cleared, record combos
      combos++;
      if (combos > max_combos) max_combos = combos;

      boolean b2b_eligible = (count == 4 || (tspin == TSpinKind::TSPIN_MINI || tspin == TSpinKind::TSPIN));
      boolean b2b_bonus = in_b2b && b2b_eligible;
      if (b2b_bonus) {
        base_score = base_score * 3 / 2;
      }

      int combo_score = 50 * combos * cur_level; // at this line, combos are never -1 so this is ok

      int perfect_bonus = 0;
      if (board.is_perfect()) {
        if (count == 1) perfect_bonus = 800;
        else if (count == 2) perfect_bonus = 1200;
        else if (count == 3) perfect_bonus = 1800;
        else if (count == 4 && b2b_bonus) perfect_bonus = 3200;
        else if (count == 4) perfect_bonus = 2000;
      }

      score += base_score * cur_level;
      score += combo_score;
      score += perfect_bonus * cur_level;

      removed_lines += count;
      if (count == 4) tetris_count++;
      in_b2b = b2b_eligible;

      return true;
    }

    int current_level() {
      return starting_level + (removed_lines / 10);
    }

    double current_tpm() {
      unsigned long elapsed_ms = millis() - game_started_at;
      double elapsed_min = elapsed_ms / 60000.0;
      return mino_placed / elapsed_min;
    }

    double current_lpm() {
      unsigned long elapsed_ms = millis() - game_started_at;
      double elapsed_min = elapsed_ms / 60000.0;
      return removed_lines / elapsed_min;
    }

    GameResult game_fail() {
      return game_stats(GameResultCode::Fail);
    }

    GameResult game_cancel() {
      return game_stats(GameResultCode::Cancel);
    }

    GameResult game_clear() {
      return game_stats(GameResultCode::Clear);
    }

    GameResult game_stats(GameResultCode code) {
      return GameResult(
        code,
        mode,
        mino_placed,
        millis() - game_started_at,
        score,
        removed_lines,
        current_level(),
        tetris_count,
        tspins,
        max_combos,
        hold_count,
        current_tpm(),
        current_lpm()
      );
    }

    int free_fall_ms() {
      int cur_level = current_level();

      if (cur_level == 1) return 1000;
      if (cur_level == 2) return 806;
      if (cur_level == 3) return 651;
      if (cur_level == 4) return 524;
      if (cur_level == 5) return 423;
      if (cur_level == 6) return 341;
      if (cur_level == 7) return 275;
      if (cur_level == 8) return 221;
      if (cur_level == 9) return 178;
      if (cur_level == 10) return 144;
      if (cur_level == 11) return 116;
      if (cur_level == 12) return 93;
      if (cur_level == 13) return 75;
      if (cur_level == 14) return 60;
      if (cur_level == 15) return 49;
      if (cur_level == 16) return 39;
      if (cur_level == 17) return 31;
      if (cur_level == 18) return 25;
      if (cur_level == 19) return 20;
      if (cur_level == 20) return 16;
      if (cur_level == 21) return 13;
      if (cur_level == 22) return 10;
      if (cur_level == 23) return 8;
      if (cur_level == 24) return 7;
      if (cur_level == 25) return 5;
      if (cur_level == 26) return 4;
      if (cur_level == 27) return 3;
      if (cur_level == 28) return 2;
      if (cur_level == 29) return 2;
      if (cur_level == 30) return 1;
      if (cur_level == 31) return 1;
      if (cur_level == 32) return 1;
      if (cur_level == 33) return 1;
      return 0;
    }

    void main_loop() {
      game_started_at = millis();

      unsigned long last_soft_dropped = 0;
      boolean horizontal_auto_repeat_started = false;
      input::ButtonState prev_input = buttons.get();
      boolean hard_dropped = false;

      unsigned long fps_counter = 0;
      unsigned long fps_last_checked = millis();

      while (true) {
        unsigned long now = millis();
        input::ButtonState btns = buttons.get();

        bool failed = false;
        bool quitted = false;
        bool cleared = false;

        if (btns.START && !prev_input.START) {
          paused = !paused;
          if (paused) pause_selected = PauseOption::RESUME;
          else free_fall_timer = now;
        }

        if (paused) {
          if ((btns.UP && !prev_input.UP) || (btns.DOWN && !prev_input.DOWN))
            pause_selected = (pause_selected == PauseOption::RESUME) ? PauseOption::QUIT : PauseOption::RESUME;

          if (btns.A && !prev_input.A) {
            if (pause_selected == PauseOption::QUIT) {
              quitted = true;
            } else {
              paused = false;
              free_fall_timer = now;
            }
          }
        } else {
          // pop mino if needed
          if (!board.cur_mino_exists()) {
            Mino m = next_mino();
            if (!try_place_mino(m, now)) failed = true;
          }

          if (!failed) {
            // check hard drop
            if (btns.UP && !prev_input.UP) {
              int i = 0;
              while (try_move(MoveDirection::DOWN, 1, now)) i++;
              score += 2 * i;
              hard_dropped = true;
            } else {
              // hold
              // because R button does not exist, uses SELECT press as hold
              if (btns.SELECT) {
                if (!hold_once_tried && board.cur_mino_exists()) {
                  hold_once_tried = true;

                  // flash animation
                  for (int i = 0; i < 3; i++) {
                    // render flashed (white) lines
                    board.cur_mino->flash();
                    if (hold_mino.has_value()) hold_mino->flash();
                    publish_snapshot();
                    vTaskDelay(pdMS_TO_TICKS(30));

                    // render original lines
                    board.cur_mino->stop_flash();
                    if (hold_mino.has_value()) hold_mino->stop_flash();
                    publish_snapshot();
                    vTaskDelay(pdMS_TO_TICKS(30));
                  }

                  // temporary save current hold mino
                  std::optional<Mino> temp = hold_mino;

                  // next hold mino is current mino
                  hold_mino = Mino::for_board(board.cur_mino->type);

                  // next mino is holded one if hold exists, else next_mino();
                  Mino next = temp.has_value() ? *temp : next_mino();

                  if (!try_place_mino(next, now)) failed = true;
                  else hold_count++;
                }
              }

              if (!failed) {
                // rotation
                if (btns.A && !prev_input.A) try_rotate(RotateDirection::CLOCKWISE, now);
                else if (btns.B && !prev_input.B) try_rotate(RotateDirection::COUNTER_CLOCKWISE, now);

                // softdrop
                if (btns.DOWN) {
                  // when DOWN button press held, soft drop needs some interval
                  boolean soft_drop_interval_passed = (now - last_soft_dropped) >= free_fall_ms() / 20;

                  // when the previous press was not DOWN, or soft drop interval has passed, soft drop happens
                  if (!prev_input.DOWN || soft_drop_interval_passed) {
                    if (try_move(MoveDirection::DOWN, 1, now)) score += 1;
                    last_soft_dropped = now;
                  }
                }

                apply_free_fall(now);

                // horizontal move
                if (btns.RIGHT || btns.LEFT) {
                  MoveDirection dir = btns.RIGHT ? MoveDirection::RIGHT : MoveDirection::LEFT;

                  if (btns.RIGHT && btns.LEFT) {
                    // on both pressed, do nothing

                  } else if ((btns.RIGHT && !prev_input.RIGHT) || (btns.LEFT && !prev_input.LEFT)) {
                    // when horizontal press changed, just move
                    try_move(dir, 1, now);
                    horizontal_auto_repeat_started = false;

                  } else {
                    // when press held, move after some interval
                    if (!horizontal_auto_repeat_started) {
                      if (now - last_horizontally_moved_at >= horizontal_move_first_wait_ms) {
                        try_move(dir, 1, now);
                        horizontal_auto_repeat_started = true;
                      }
                    } else {
                      if (now - last_horizontally_moved_at >= horizontal_move_auto_repeating_wait_ms) {
                        try_move(dir, 1, now);
                      }
                    }
                  }
                }
              }
            }
          }

          if (!failed) {
            if (board.mino_landed()) {
              if (!lockdown_judging) lockdown_judging = true;
              if (hard_dropped || (now - last_moved_at >= lockdown_wait_ms || move_cnt_while_lockdown_judging >= lockdown_reset_move_limit)) {
                if (hard_dropped) hard_dropped = false;
                if (!lock_mino_and_clear_lines()) failed = true;
              }
            } else if (lockdown_judging) {
              // in case once landed and judge started, but now it's not landed, reset them.
              // this happens when once landed, but moved horizontally, then it's not landed now
              lockdown_judging = false;
              move_cnt_while_lockdown_judging = 0;
            }
            cleared = current_goal() <= 0;
          }
        }

        
        publish_snapshot();
        prev_input = btns;

        if (quitted) { final_result = game_cancel(); break; }
        if (failed)  { final_result = game_fail();  break; }
        if (cleared) { final_result = game_clear(); break; }

        vTaskDelay(1);

        fps_counter++;
        if (now - fps_last_checked >= 1000) {
          Serial.print("Logic Loop FPS: ");
          Serial.println(fps_counter);
          fps_counter = 0;
          fps_last_checked = now;
        }
      }

      xSemaphoreGive(done_sem);
      vTaskDelete(NULL);
    }

    void capture_snapshot(GameSnapshot& snap) {
      snap.valid = true;

      // copy blocks
      for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 10; c++) {
          if (board.block_exists(r, c)) {
            auto& bc = board.blocks[r][c]->color;
            snap.blocks[r][c] = BlockColor{bc.base, bc.lighter, bc.darker};
          } else {
            snap.blocks[r][c] = std::nullopt;
          }
        }
      }

      // copy current mino
      snap.has_cur_mino = board.cur_mino_exists();
      if (snap.has_cur_mino) {
        Mino& m = *board.cur_mino;
        snap.cur_mino_block_pos = m.positions;
        snap.cur_mino_color = m.color;
        snap.hard_drop_distance = board.hard_drop_distance();
      }

      // copy hold
      snap.has_hold_mino = hold_mino.has_value();
      if (snap.has_hold_mino) {
        snap.hold_type = hold_mino->type;
        uint16_t base = hold_mino->color.base;
        snap.hold_color = BlockColor{base, hold_mino->color.lighter, hold_mino->color.darker};
      }

      for (int i = 0; i < 6; i++) snap.next_types[i] = next_mino_type(i);

      snap.score        = score;
      snap.level        = current_level();
      snap.tetris_count = tetris_count;
      snap.tspins       = tspins;
      snap.combos       = combos;
      snap.goal     = current_goal();

      unsigned long elapsed_ms = millis() - game_started_at;
      snap.elapsed_ms = elapsed_ms;
      snap.tpm = elapsed_ms >= 3000 ? current_tpm() : 0;
      snap.lpm = elapsed_ms >= 3000 ? current_lpm() : 0;

      snap.is_paused = paused;
      snap.pause_selected = pause_selected;
    }

    void publish_snapshot() {
      capture_snapshot(triple_buffer.write_buf());
      triple_buffer.publish();
    }
};

