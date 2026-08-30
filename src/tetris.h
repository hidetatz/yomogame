#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <optional>
#include <tuple>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <input.h>
#include <yomogi.h>

/*
 * Mino, Board and Game
 * This does not depend on screen size.
 */

// color of block in tetrimino
struct BlockColor {
  uint16_t base;
  uint16_t lighter;
  uint16_t darker;
};

// block position in a board
class BlockPos {
  public:
    int row;
    int col;
    BlockPos(int row, int col) : row(row), col(col) {}
    BlockPos() : row(0), col(0) {}

    void up(int distance) { row -= distance; }
    void down(int distance) { row += distance; }
    void right(int distance) { col += distance; }
    void left(int distance) { col -= distance; }
};

class Block {
  public:
    std::optional<uint16_t> overridden_color;
    BlockColor color;
    Block(BlockColor color) : color(color) {}

    void override_base_color(uint16_t c) {
      overridden_color = color.base;
      color.base = c;
    }

    void recover_base_color() {
      color.base = *overridden_color;
      overridden_color = std::nullopt;
    }
};

enum class MinoDirection {
  NORTH, EAST, SOUTH, WEST
};

enum class MoveDirection {
  UP, DOWN, LEFT, RIGHT
};

enum class RotateDirection {
  CLOCKWISE, COUNTER_CLOCKWISE
};

enum class MinoType {
  O, I, T, L, J, S, Z
};

class Pivot {
  public:
    float row;
    float col;
    Pivot(float row, float col) : row(row), col(col) {}
    void up(int distance) { row -= distance; }
    void down(int distance) { row += distance; }
    void right(int distance) { col += distance; }
    void left(int distance) { col -= distance; }
};

class Mino {
  public:
    MinoType type;
    BlockColor color;
    std::optional<uint16_t> overridden_color;
    std::array<BlockPos, 4> positions;
    Pivot pivot;
    MinoDirection cur_direction;

    Mino(MinoType type, uint16_t base_color, uint16_t lighter_color, uint16_t darker_color, std::array<BlockPos, 4> positions, Pivot pivot, MinoDirection direction) : color{base_color, lighter_color, darker_color}, positions(positions), pivot(pivot), type(type), cur_direction(direction) {}

    static Mino for_board(MinoType type) {
      return Mino::from_type_row_col(type, 0, type == MinoType::O ? 4 : 3);
    }

    static Mino for_next_minos(MinoType type) {
      return Mino::from_type_row_col(type, 0, 0);
    }

    static Mino for_hold_box(MinoType type) {
      return Mino::from_type_row_col(type, 0, 0);
    }

    static Mino from_type_row_col(MinoType type, int row, int col) {
      if (type == MinoType::O) return Mino(type, 63456, 63468, 38048, /* yellow */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col+1)}}, Pivot(0, 0), MinoDirection::NORTH);
      if (type == MinoType::I) return Mino(type, 1694, 26398, 1010, /* sky blue */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row, col+3)}}, Pivot(row+0.5, col+1.5), MinoDirection::NORTH);
      if (type == MinoType::T) return Mino(type, 40989, 49981, 24593, /* purple */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+1)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::L) return Mino(type, 62242, 62733, 37345, /* orange */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::J) return Mino(type, 8254, 29502, 4114, /* blue */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::S) return Mino(type, 6049, 28589, 3200, /* green */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col+1), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
      /* if (type == MinoType::Z) */ return Mino(type, 63521, 64301, 36864, /* red */ {{BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col), BlockPos(row-1, col+1)}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    void up(int distance) {
      for (int i = 0; i < 4; i++) positions[i].up(distance);
      pivot.up(distance);
    }

    void down(int distance) {
      for (int i = 0; i < 4; i++) positions[i].down(distance);
      pivot.down(distance);
    }

    void right(int distance) {
      for (int i = 0; i < 4; i++) positions[i].right(distance);
      pivot.right(distance);
    }

    void left(int distance) {
      for (int i = 0; i < 4; i++) positions[i].left(distance);
      pivot.left(distance);
    }

    boolean is_O() {
      return type == MinoType::O;
    }

    boolean is_I() {
      return type == MinoType::I;
    }

    boolean is_T() {
      return type == MinoType::T;
    }

    std::array<int, 8> get_rotated_blocks_pos(RotateDirection dir) {
      std::array<int, 8> result;
      for (int i = 0; i < 4; i++) {
        BlockPos p = positions[i];

        float rel_row = p.row - pivot.row;
        float rel_col = p.col - pivot.col;

        float new_rel_row, new_rel_col;
        if (dir == RotateDirection::CLOCKWISE) {
          new_rel_row = rel_col;
          new_rel_col = -rel_row;
        } else {
          new_rel_row = -rel_col;
          new_rel_col = rel_row;
        }

        result[i * 2] = static_cast<int>(std::lround(pivot.row + new_rel_row));
        result[i * 2 + 1] = static_cast<int>(std::lround(pivot.col + new_rel_col));
      }
      return result;
    }

    void rotate(RotateDirection dir) {
      std::array<int, 8> pos = get_rotated_blocks_pos(dir);
      positions[0].row = pos[0];
      positions[0].col = pos[1];
      positions[1].row = pos[2];
      positions[1].col = pos[3];
      positions[2].row = pos[4];
      positions[2].col = pos[5];
      positions[3].row = pos[6];
      positions[3].col = pos[7];
    }

    std::array<int, 2> srs_kick_value(RotateDirection dir, int i) {
      if (i == 0) return {0, 0};
      if (is_I()) return srs_kick_value_I(dir, i);
      return srs_kick_value_TLJSZ(dir, i);
    }

    // srs kick table follows the guideline, it returns {horizontal_move, vertical_move} while
    // - horizontal: negative goes left
    // - vertical  : negative goes **down**
    // In this tetris codebase, the top left is (0, 0), so left and **up** is negative.
    // So this function is not consistent with others, but this table should follow the Tetris guideline
    // for verifiability.
    std::array<int, 2> srs_kick_value_I(RotateDirection dir, int i) {
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 1) return {-2,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 2) return { 1,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 3) return {-2, -1};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 4) return { 1,  2};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return { 2,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return {-1,  2};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return { 2, -1};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 2) return { 2,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 3) return {-1,  2};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 4) return { 2, -1};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return { 2,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return {-1,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 2, -1};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return {-1, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 1) return { 2,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 2) return {-1,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 3) return { 2,  1};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 4) return {-1, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return {-2,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 1, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return {-2,  1};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 2) return {-2,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 3) return { 1, -2};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 4) return {-2,  1};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return {-2,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return { 1,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return {-2, -1};
      /*if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) */ return { 1,  2};
    }

    std::array<int, 2> srs_kick_value_TLJSZ(RotateDirection dir, int i) {
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 2) return {-1,  1};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 3) return { 0, -2};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::CLOCKWISE         && i == 4) return {-1, -2};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return { 1,  1};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 0, -2};
      if (cur_direction == MinoDirection::NORTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return { 1, -2};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 2) return { 1, -1};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 3) return { 0,  2};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::CLOCKWISE         && i == 4) return { 1,  2};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return { 1, -1};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 0,  2};
      if (cur_direction == MinoDirection::EAST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return { 1,  2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 1) return { 1,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 2) return { 1,  1};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 3) return { 0, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::CLOCKWISE         && i == 4) return { 1, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return {-1,  1};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 0, -2};
      if (cur_direction == MinoDirection::SOUTH && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) return {-1, -2};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 2) return {-1, -1};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 3) return { 0,  2};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::CLOCKWISE         && i == 4) return {-1,  2};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 1) return {-1,  0};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 2) return {-1, -1};
      if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 3) return { 0,  2};
      /* if (cur_direction == MinoDirection::WEST  && dir == RotateDirection::COUNTER_CLOCKWISE && i == 4) */ return {-1,  2};
    }

    void override_base_color(uint16_t c) {
      overridden_color = color.base;
      color.base = c;
    }

    void recover_base_color() {
      color.base = *overridden_color;
      overridden_color = std::nullopt;
    }
};

class Board {
  public:
    std::optional<Mino> cur_mino;
    std::optional<Block> blocks[20][10]; // block position is managed by the index in blocks, not BlockPos
    Board() {}

    boolean cur_mino_exists() {
      return cur_mino.has_value();
    }

    boolean block_exists(int row, int col) {
      return blocks[row][col].has_value();
    }

    boolean block_placable_at(int row, int col) {
      if (col < 0 || 10 <= col || 20 <= row) return false;
      if (row < 0) return true;
      return !block_exists(row, col);
    }

    boolean blocks_placable(int row1, int col1, int row2, int col2, int row3, int col3, int row4, int col4) {
      return block_placable_at(row1, col1) && block_placable_at(row2, col2) && block_placable_at(row3, col3) && block_placable_at(row4, col4);
    }


    boolean mino_placable(Mino m) {
      return block_placable_at(m.positions[0].row, m.positions[0].col) &&
             block_placable_at(m.positions[1].row, m.positions[1].col) &&
             block_placable_at(m.positions[2].row, m.positions[2].col) &&
             block_placable_at(m.positions[3].row, m.positions[3].col);
    }

    void place_mino(Mino m) {
      cur_mino = m;
    }

    boolean can_move_mino(MoveDirection dir, int distance) {
      for (int i = 0; i < 4; i++) {
        BlockPos cur_pos = cur_mino->positions[i];
        int new_row = cur_pos.row;
        int new_col = cur_pos.col;
        if (dir == MoveDirection::DOWN) new_row += distance;
        else if (dir == MoveDirection::RIGHT) new_col += distance;
        else if (dir == MoveDirection::LEFT) new_col -= distance;
        if (!block_placable_at(new_row, new_col)) return false;
      }
      return true;
    }

    void move_mino(MoveDirection dir, int distance) {
      if (dir == MoveDirection::UP) cur_mino->up(distance);
      else if (dir == MoveDirection::DOWN) cur_mino->down(distance);
      else if (dir == MoveDirection::RIGHT) cur_mino->right(distance);
      else if (dir == MoveDirection::LEFT) cur_mino->left(distance);
    }

    std::tuple<boolean, int> rotate(RotateDirection dir) {
      if (cur_mino->is_O()) return {false, 0}; // o does not rotate

      // srs
      std::array<int, 8> base_pos = cur_mino->get_rotated_blocks_pos(dir);
      for (int i = 0; i < 5; i++) {
        std::array<int, 2> kick = cur_mino->srs_kick_value(dir, i);
        std::array<int, 8> new_pos = base_pos;
        for (int b = 0; b < 4; b++) {
          new_pos[b * 2] -= kick[1];
          new_pos[b * 2 + 1] += kick[0];
        }
        if (blocks_placable(new_pos[0], new_pos[1], new_pos[2], new_pos[3], new_pos[4], new_pos[5], new_pos[6], new_pos[7])) {
          cur_mino->rotate(dir);
          if (kick[0] > 0) move_mino(MoveDirection::RIGHT, kick[0]);
          else move_mino(MoveDirection::LEFT, -kick[0]);
          if (kick[1] > 0) move_mino(MoveDirection::UP, kick[1]);
          else move_mino(MoveDirection::DOWN, -kick[1]);
          return {true, i};
        }
      }
      return {false, 0};
    }

    boolean mino_landed() {
      return !can_move_mino(MoveDirection::DOWN, 1);
    }

    boolean lockdown_mino() {
      boolean locked_out = false;
      for (int i = 0; i < 4; i++) {
        int row = cur_mino->positions[i].row;
        int col = cur_mino->positions[i].col;
        if (row < 0) {
          locked_out = true;
          continue;
        }
        blocks[row][col] = Block(cur_mino->color);
      }
      cur_mino = std::nullopt;
      return !locked_out;
    }

    // how many down happens on hard drop?
    int hard_drop_distance() {
      int cur_limit = 0;
      for (int distance = 1; distance < 20; distance++) {
        if (!can_move_mino(MoveDirection::DOWN, distance)) break;
        cur_limit = distance;
      }
      return cur_limit;
    }

    boolean is_full_row(int row) {
      for (int col = 0; col < 10; col++) {
        if (!block_exists(row, col)) return false;
      }
      return true;
    }

    boolean is_empty_row(int row) {
      for (int col = 0; col < 10; col++) {
        if (block_exists(row, col)) return false;
      }
      return true;
    }

    std::tuple<int, std::array<int, 4>> deletable_rows() {
      int count = 0;
      std::array<int, 4> rows = {};
      for (int row = 0; row < 20; row++) {
        if (is_full_row(row)) {
          rows[count] = row;
          count++;
        }
      }
      return {count, rows};
    }

    void delete_block(int row, int col) {
      blocks[row][col] = std::nullopt;
    }

    void copy_row(int from, int to) {
      if (from == to) return;
      for (int col = 0; col < 10; col++) {
        blocks[to][col] = blocks[from][col];
      }
    }

    void clear_lines(std::array<int, 4> rows, int count) {
      auto is_deletable = [&](int row) {
        for (int i = 0; i < count; i++) if (rows[i] == row) return true;
        return false;
      };
      int write_row = 19;
      for (int read_row = 19; read_row >= 0; read_row--) {
        if (!is_deletable(read_row)) {
          copy_row(read_row, write_row);
          write_row--;
        }
      }
      for (int row = write_row; row >= 0; row--) {
        for (int col = 0; col < 10; col++) {
          delete_block(row, col);
        }
      }
    }

    boolean is_perfect() {
      for (int row = 0; row < 20; row++) {
        if (!is_empty_row(row)) return false;
      }
      return true;
    }
};


/* triple buffer rendering */

struct MinoRenderData {
  std::array<BlockPos, 4> positions;
  uint16_t base;
  uint16_t lighter;
  uint16_t darker;
};

struct RenderSnapshot {
  bool valid = false;

  // blocks
  std::optional<BlockColor> blocks[20][10];

  // current mino and ghost
  bool has_cur_mino = false;
  MinoRenderData cur_mino;
  int hard_drop_distance = 0;

  // hold
  bool has_hold_mino = false;
  MinoType hold_type = MinoType::T;
  BlockColor hold_color{0, 0, 0};

  // next minos
  std::array<MinoType, 6> next_types{};

  // stats
  int score = 0;
  int level = 1;
  int tetris_count = 0;
  int tspins = 0;
  int combos = -1;
  int top_stat = 0;
  bool is_endless = true;
  double tpm = 0;
  double lpm = 0;
  unsigned long elapsed_ms = 0;
};

class TripleBuffer {
  private:
    RenderSnapshot buffers[3];
    int write_idx;
    int read_idx;
    std::atomic<int> middle_idx;
    std::atomic<bool> has_new{false};

  public:
    TripleBuffer() : write_idx(0), read_idx(1), middle_idx(2) {}

    RenderSnapshot& write_buf() { return buffers[write_idx]; }

    void publish() {
      int new_middle = write_idx;
      int old_middle = middle_idx.exchange(new_middle, std::memory_order_acq_rel);
      write_idx = old_middle;
      has_new.store(true, std::memory_order_release);
    }

    RenderSnapshot& read_buf() {
      if (has_new.exchange(false, std::memory_order_acq_rel)) {
        int old_read = read_idx;
        read_idx = middle_idx.exchange(old_read, std::memory_order_acq_rel);
      }
      return buffers[read_idx];
    }
};


struct DisplayParameters {
  // menu and result
  const int menu_sprite_x;
  const int menu_sprite_y;
  const int menu_sprite_width;
  const int menu_sprite_height;
  const int menu_mode_width;
  const int menu_mode_height;
  const int menu_mode_x_in_sprite;
  const int menu_mode_endless_y_in_sprite;
  const int menu_mode_l40_y_in_sprite;
  const int menu_mode_l150_y_in_sprite;
  const int menu_mode_l999_y_in_sprite;

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

  const int lines_or_goal_label_y;
  const int level_label_y;
  const int tetris_label_y;
  const int tspin_label_y;
  const int combo_label_y;
  const int tpm_label_y;
  const int lpm_label_y;

  const std::string lines_label;
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

  const int lines_or_goal_y_in_sprite;
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
  .menu_mode_width = 180,
  .menu_mode_height = 20,
  .menu_mode_x_in_sprite = 30,
  .menu_mode_endless_y_in_sprite = 20,
  .menu_mode_l40_y_in_sprite = 60,
  .menu_mode_l150_y_in_sprite = 100,
  .menu_mode_l999_y_in_sprite = 140,

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

  .lines_or_goal_label_y = 132,
  .level_label_y = 145,
  .tetris_label_y = 158,
  .tspin_label_y = 171,
  .combo_label_y = 184,
  .tpm_label_y = 197,
  .lpm_label_y = 210,

  .lines_label = "LNS",
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

  .lines_or_goal_y_in_sprite = 0,
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

  .yomogi_width = YOMOGI_240X240_WIDTH,
  .yomogi_height = YOMOGI_240X240_HEIGHT,
  .yomogi_x = 183,
  .yomogi_y = 187,
  .yomogi_image = yomogi_240x240,
  .yomogi_transparent_color = YOMOGI_240X240_TRANSPARENT
};

/*
 * Tetris main logic.
 */

const int horizontal_move_first_wait_ms = 300;
const int horizontal_move_auto_repeating_wait_ms = 50;
const int lockdown_wait_ms = 500;
const int lockdown_reset_move_limit = 15;

enum class TSpinKind {
  TSPIN, TSPIN_MINI, NONE
};

enum class GameMode {
  Endless, L40, L150, L999
};

enum class GameResultCode {
  Cancel, Clear, Fail
};

class GameResult {
  public:
    GameResultCode code;
    GameMode mode;
    int mino_placed;
    unsigned long elapsed_ms;
    int score;
    int removed_lines;
    int level_at_last;
    int tetris_count;
    int tspins;
    int max_combos;
    int holds;
    double tpm;
    double lpm;

  GameResult(GameResultCode code, GameMode mode, int mino_placed, unsigned long elapsed_ms, int score, int removed_lines, int level_at_last, int tetris_count, int tspins, int max_combos, int holds, double tpm, double lpm) :
    code(code),
    mode(mode),
    mino_placed(mino_placed),
    elapsed_ms(elapsed_ms),
    score(score),
    removed_lines(removed_lines),
    level_at_last(level_at_last),
    tetris_count(tetris_count),
    tspins(tspins),
    max_combos(max_combos),
    holds(holds),
    tpm(tpm),
    lpm(lpm) {}
};

class Game {
  public:
    DisplayParameters &dp;

    GameMode mode;

    std::array<MinoType, 7> cur_bag;
    std::array<MinoType, 7> next_bag;
    int mino_idx;

    Board board;

    std::optional<Mino> hold_mino;

    int last_kick_index;
    boolean was_last_move_rotation;
    unsigned long last_moved_at;
    unsigned long last_horizontally_moved_at;
    unsigned long free_fall_timer;
    double free_fall_progress;
    boolean lockdown_judging;
    int move_cnt_while_lockdown_judging;
    boolean hold_once_tried;

    int mino_placed;
    unsigned long game_started_at;
    int score;
    int removed_lines;
    int starting_level;
    int goal;
    int tetris_count;
    int tspins;
    int combos;
    boolean in_b2b;
    int hold_count;
    int max_combos;

    Input &input;
    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;
    TFT_eSprite next_minos_sprite;
    TFT_eSprite hold_sprite;
    TFT_eSprite stats_sprite;

    TripleBuffer triple_buffer;
    volatile bool game_running;
    std::optional<GameResult> final_result;
    SemaphoreHandle_t done_sem;
    TaskHandle_t logic_task_handle;
    TaskHandle_t render_task_handle;

  public:
    Game(Input &input, TFT_eSPI &screen, DisplayParameters &params, GameMode mode) :
      dp(params),

      mode(mode),

      cur_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      next_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      mino_idx(0),

      board(),

      last_kick_index(0),
      was_last_move_rotation(false),
      last_moved_at(0),
      last_horizontally_moved_at(0),
      free_fall_timer(0),
      free_fall_progress(0.0),
      lockdown_judging(false),
      move_cnt_while_lockdown_judging(0),
      hold_once_tried(false),

      mino_placed(0),
      game_started_at(0),
      score(0),
      removed_lines(0),
      starting_level(1),
      goal(mode == GameMode::Endless ? 0 : mode == GameMode::L40 ? 40 : mode == GameMode::L150 ? 150 : 999),
      tetris_count(0),
      tspins(0),
      combos(-1), // combos starts count when 2 consecutive clear happens, and it is counted as "1 combo", so it's good to start with -1
      in_b2b(false),
      hold_count(0),
      max_combos(0),

      input(input),
      bgcolor(TFT_BLACK),
      screen(screen),
      board_sprite(&screen),
      next_minos_sprite(&screen),
      hold_sprite(&screen),
      stats_sprite(&screen),
      game_running(false),
      logic_task_handle(nullptr),
      render_task_handle(nullptr)
      {
        done_sem = xSemaphoreCreateBinary(); // 追加
        shuffle_bag(cur_bag);
        shuffle_bag(next_bag);
      }

    ~Game() {
      vSemaphoreDelete(done_sem);
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
      if (g > 20.0) g = 20.0; // 20Gで頭打ち
      return g;
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

      if (mode != GameMode::Endless) {
        goal -= count;
        if (goal <= 0) goal = 0;
      }

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

      for (int i = 0; i < 3; i++) {
        // delete animation
        // render flashed (white) lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->override_base_color(TFT_WHITE);
          }
        }
        publish_snapshot();
        vTaskDelay(pdMS_TO_TICKS(30));
        // render original lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->recover_base_color();
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
      return starting_level + (removed_lines / 2);
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

    GameResult start() {
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
      std::string lbl = mode == GameMode::Endless ? dp.lines_label : dp.goal_label;
      render_left_label(lbl,             dp.stats_label_x, dp.lines_or_goal_label_y, dp.stats_font);
      render_left_label(dp.level_label,  dp.stats_label_x, dp.level_label_y,         dp.stats_font);
      render_left_label(dp.tetris_label, dp.stats_label_x, dp.tetris_label_y,        dp.stats_font);
      render_left_label(dp.tspin_label, dp.stats_label_x, dp.tspin_label_y,         dp.stats_font);
      render_left_label(dp.combo_label, dp.stats_label_x, dp.combo_label_y,         dp.stats_font);
      render_left_label(dp.tpm_label,    dp.stats_label_x, dp.tpm_label_y,           dp.stats_font);
      render_left_label(dp.lpm_label,    dp.stats_label_x, dp.lpm_label_y,           dp.stats_font);

      /* board area */
      board_sprite.createSprite(dp.board_sprite_width, dp.board_sprite_height);
      render_square(dp.board_box_x, dp.board_box_y, dp.board_box_width, dp.board_box_height);

      /* next_minos area */
      next_minos_sprite.createSprite(dp.next_minos_sprite_width, dp.next_minos_sprite_height);
      render_square(dp.next_minos_box_x, dp.next_minos_box_y, dp.next_minos_box_width, dp.next_minos_box_height);
      render_centered_label("Next", dp.next_minos_label_x_center, dp.next_minos_label_y, dp.next_minos_label_font);

      /* yomogi area */
      screen.setSwapBytes(true);
      screen.pushImage(dp.yomogi_x, dp.yomogi_y, dp.yomogi_width, dp.yomogi_height, dp.yomogi_image, dp.yomogi_transparent_color);

      game_started_at = millis();
      game_running = true;
      final_result = std::nullopt;

      xTaskCreatePinnedToCore(logic_task_trampoline, "tetris_logic", 8192, this, 2, &logic_task_handle, 1);
      xTaskCreatePinnedToCore(render_task_trampoline, "tetris_render", 8192, this, 1, &render_task_handle, 1);

      xSemaphoreTake(done_sem, portMAX_DELAY);

      game_running = false;
      vTaskDelay(pdMS_TO_TICKS(50));

      return *final_result;
    }

    static void logic_task_trampoline(void* param) {
      static_cast<Game*>(param)->logic_task();
    }

    static void render_task_trampoline(void* param) {
      static_cast<Game*>(param)->render_task();
    }

    void render_task() {
      const TickType_t period = pdMS_TO_TICKS(16);
      TickType_t last_wake = xTaskGetTickCount();
      while (game_running) {
        RenderSnapshot& snap = triple_buffer.read_buf();
        if (snap.valid) render_from_snapshot(snap);
        vTaskDelayUntil(&last_wake, period);
      }
      vTaskDelete(NULL);
    }

    void logic_task() {
      unsigned long last_soft_dropped = 0;
      boolean horizontal_auto_repeat_started = false;
      ButtonState prev_input;
      boolean hard_dropped = false;

      unsigned long fps_counter = 0;
      unsigned long fps_last_checked = millis();

      while (game_running) {
        unsigned long now = millis();
        ButtonState btns = input.get();

        bool failed = false;

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
                  board.cur_mino->override_base_color(TFT_WHITE);
                  if (hold_mino.has_value()) hold_mino->override_base_color(TFT_WHITE);
                  publish_snapshot();
                  vTaskDelay(pdMS_TO_TICKS(30));

                  // render original lines
                  board.cur_mino->recover_base_color();
                  if (hold_mino.has_value()) hold_mino->recover_base_color();
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

        boolean cleared = false;

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
          cleared = (!(mode == GameMode::Endless) && goal <= 0);
        }

        
        publish_snapshot();
        prev_input = btns;

        if (failed)  { final_result = game_fail();  break; }
        if (cleared) { final_result = game_clear(); break; }

        vTaskDelay(1);

        fps_counter++;
        if (now - fps_last_checked >= 1000) {
          Serial.print("FPS: ");
          Serial.println(fps_counter);
          fps_counter = 0;
          fps_last_checked = now;
        }
      }

      xSemaphoreGive(done_sem);
      vTaskDelete(NULL);
    }

    void capture_snapshot(RenderSnapshot& snap) {
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
        snap.cur_mino.positions = m.positions;
        snap.cur_mino.base = m.color.base;
        snap.cur_mino.lighter = m.color.lighter;
        snap.cur_mino.darker = m.color.darker;
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
      snap.top_stat     = mode == GameMode::Endless ? removed_lines : goal;
      snap.is_endless   = (mode == GameMode::Endless);

      unsigned long elapsed_ms = millis() - game_started_at;
      snap.elapsed_ms = elapsed_ms;
      snap.tpm = elapsed_ms >= 3000 ? current_tpm() : 0;
      snap.lpm = elapsed_ms >= 3000 ? current_lpm() : 0;
    }

    void publish_snapshot() {
      capture_snapshot(triple_buffer.write_buf());
      triple_buffer.publish();
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

    void render_mino_data_on_sprite(TFT_eSprite& sprite, const MinoRenderData& m, int x_offset, int y_offset, int block_size, int bevel_size) {
      for (int i = 0; i < 4; i++) {
        render_block(sprite, m.positions[i].row, m.positions[i].col, x_offset, y_offset, block_size, bevel_size, m.base, m.lighter, m.darker);
      }
    }

    void render_ghost_from_data(const MinoRenderData& m, int hard_drop_distance) {
      for (int i = 0; i < 4; i++) {
        render_ghost_block(board_sprite, m.positions[i].row + hard_drop_distance, m.positions[i].col, dp.board_mino_block_size, m.base);
      }
    }

    void render_from_snapshot(const RenderSnapshot& snap) {
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

      render_right_label_sprite(stats_sprite, std::to_string(snap.top_stat),                     dp.stats_x_right_in_sprite, dp.lines_or_goal_y_in_sprite, dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(snap.level),                         dp.stats_x_right_in_sprite, dp.level_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(snap.tetris_count),                  dp.stats_x_right_in_sprite, dp.tetris_y_in_sprite,        dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(snap.tspins),                        dp.stats_x_right_in_sprite, dp.tspin_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(snap.combos < 0 ? 0 : snap.combos),  dp.stats_x_right_in_sprite, dp.combo_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, tpm_str,                                            dp.stats_x_right_in_sprite, dp.tpm_y_in_sprite,           dp.stats_font);
      render_right_label_sprite(stats_sprite, lpm_str,                                            dp.stats_x_right_in_sprite, dp.lpm_y_in_sprite,           dp.stats_font);
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
        render_ghost_from_data(snap.cur_mino, snap.hard_drop_distance);
        render_mino_data_on_sprite(board_sprite, snap.cur_mino, 0, 0, dp.board_mino_block_size, dp.board_mino_bevel);
      }
      board_sprite.pushSprite(dp.board_sprite_x, dp.board_sprite_y);

      /* next minos(色は上書きされないので通常のMinoで良い) */
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
};

/* tetris main */

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

        Game* game = new Game(input, screen, dp, cur_focus_mode);
        GameResult result = game->start();
        delete game;
        delay(1000);
        show_result(result);
        delay(100);
      }
    }
};
