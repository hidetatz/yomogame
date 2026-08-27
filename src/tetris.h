#pragma once

#include <array>
#include <optional>
#include <tuple>
#include <string>

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <input.h>
#include <yomogi.h>

/*
 * Mino and Board
 * This does not depend on screen size.
 */

// color of block in tetrimino
class BlockColor {
  public:
    uint16_t base;
    uint16_t lighter;
    uint16_t darker;
    BlockColor(uint16_t base, uint16_t lighter, uint16_t darker) : base(base), lighter(lighter), darker(darker) {}
};

// block position in a board
class BlockPos {
  public:
    int row;
    int col;
    BlockPos(int row, int col) : row(row), col(col) {}

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

    Mino(MinoType type, uint16_t base_color, uint16_t lighter_color, uint16_t darker_color, std::array<BlockPos, 4> positions, Pivot pivot, MinoDirection direction) : color(base_color, lighter_color, darker_color), positions(positions), pivot(pivot), type(type), cur_direction(direction) {}

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

    void lockdown_mino() {
      for (int i = 0; i < 4; i++) blocks[cur_mino->positions[i].row][cur_mino->positions[i].col] = Block(cur_mino->color);
      cur_mino = std::nullopt;
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

struct DisplayParameters {
  int screen_width;
  int screen_height;

  // hold
  int hold_label_font;
  int hold_label_font_height;
  int hold_label_top_margin;
  int hold_box_top_margin;
  int hold_box_left_margin;
  int hold_mino_block_size;
  int hold_mino_bevel;
  int hold_mino_top_margin;
  int hold_mino_left_margin;

  // score
  int score_font;
  int score_font_height;
  int score_label_top_margin;
  int score_label_left_margin;
  int score_top_margin;

  // time
  int time_font;
  int time_font_height;
  int time_label_top_margin;
  int time_label_left_margin;
  int time_top_margin;

  // else stats; use sprite on values
  int stats_font;
  int stats_label_chars_count;
  int stats_label_left_margin;
  int stats_font_width;
  int stats_font_height;
  int stats_top_margin;
  int stats_left_margin;
  int stats_between_margin;

  // board
  int board_mino_block_size;
  int board_mino_bevel;
  int board_top_margin;
  int board_left_margin;

  // next minos
  int next_minos_label_font;
  int next_minos_label_font_height;
  int next_minos_label_top_margin;
  int next_minos_box_top_margin;
  int next_minos_box_left_margin;
  int next_minos_mino_block_size;
  int next_minos_mino_bevel;
  int next_minos_mino_top_margin;
  int next_minos_mino_left_margin;

  int hold_box_width() { return 2 + hold_mino_left_margin * 2 + hold_mino_block_size * 4; } // include line pixel
  int hold_box_height() { return 2 + hold_mino_top_margin * 2 + hold_mino_block_size * 4; } // include line pixel
  int hold_box_x_start() { return hold_box_left_margin; }
  int hold_box_x_end() { return hold_box_x_start() + hold_box_width(); }
  int hold_box_y_start() { return hold_label_top_margin + hold_label_font_height + hold_box_top_margin; }
  int hold_box_y_end() { return hold_box_y_start() + hold_box_height(); }
  int hold_label_top_center_pos_x_start() { return hold_box_x_start() + ((hold_box_width()) / 2); }
  int hold_label_top_center_pos_y_start() { return hold_label_top_margin; }
  int hold_sprite_width() { return hold_mino_block_size * 4; }
  int hold_sprite_height() { return hold_mino_block_size * 4; }
  int hold_mino_sprite_x_start() { return hold_box_x_start() + 1 + hold_mino_left_margin; }
  int hold_mino_sprite_y_start() { return hold_box_y_start() + 1 + hold_mino_top_margin; }

  int score_time_x_end() { return board_box_x_start() - board_left_margin; }

  int score_label_x_start() { return score_label_left_margin; }
  int score_label_y_start() { return hold_box_y_end() + score_label_top_margin; }
  int score_label_y_end() { return score_label_y_start() + score_font_height; }
  int score_y_start() { return score_label_y_end() + score_top_margin; }
  int score_y_end() { return score_y_start() + score_font_height; }

  int time_label_x_start() { return time_label_left_margin; }
  int time_label_y_start() { return score_y_end() + time_label_top_margin; }
  int time_label_y_end() { return time_label_y_start() + time_font_height; }
  int time_y_start() { return time_label_y_end() + time_top_margin; }
  int time_y_end() { return time_y_start() + time_font_height; }

  int stats_label_x_start() { return stats_label_left_margin; }
  int stats_label_x_end() { return stats_label_x_start() + (stats_label_chars_count * stats_font_width + stats_label_chars_count - 1); }

  int statvals_sprite_width() { return score_time_x_end() - statvals_x_start(); }
  int statvals_sprite_height() { return stats6_y_end() - stats0_y_start(); } // in all modes there are 7 stats shown
  int statvals_chars_count() { return (statvals_sprite_width() + 1) / (stats_font_width + 1); }
  int statvals_x_start() { return stats_label_x_end() + stats_left_margin; }
  int statvals_y_start() { return time_y_end() + stats_top_margin; }
  int statvals_x_end_in_sprite() { return statvals_sprite_width(); }

  int stats0_y_start() { return time_y_end() + stats_top_margin; }
  int stats0_y_end() { return stats0_y_start() + stats_font_height; }
  int stats1_y_start() { return stats0_y_end() + stats_between_margin; }
  int stats1_y_end() { return stats1_y_start() + stats_font_height; }
  int stats2_y_start() { return stats1_y_end() + stats_between_margin; }
  int stats2_y_end() { return stats2_y_start() + stats_font_height; }
  int stats3_y_start() { return stats2_y_end() + stats_between_margin; }
  int stats3_y_end() { return stats3_y_start() + stats_font_height; }
  int stats4_y_start() { return stats3_y_end() + stats_between_margin; }
  int stats4_y_end() { return stats4_y_start() + stats_font_height; }
  int stats5_y_start() { return stats4_y_end() + stats_between_margin; }
  int stats5_y_end() { return stats5_y_start() + stats_font_height; }
  int stats6_y_start() { return stats5_y_end() + stats_between_margin; }
  int stats6_y_end() { return stats6_y_start() + stats_font_height; }
  int stats0_y_start_in_sprite() { return 0; }
  int stats0_y_end_in_sprite() { return stats0_y_start_in_sprite() + stats_font_height; }
  int stats1_y_start_in_sprite() { return stats0_y_end_in_sprite() + stats_between_margin; }
  int stats1_y_end_in_sprite() { return stats1_y_start_in_sprite() + stats_font_height; }
  int stats2_y_start_in_sprite() { return stats1_y_end_in_sprite() + stats_between_margin; }
  int stats2_y_end_in_sprite() { return stats2_y_start_in_sprite() + stats_font_height; }
  int stats3_y_start_in_sprite() { return stats2_y_end_in_sprite() + stats_between_margin; }
  int stats3_y_end_in_sprite() { return stats3_y_start_in_sprite() + stats_font_height; }
  int stats4_y_start_in_sprite() { return stats3_y_end_in_sprite() + stats_between_margin; }
  int stats4_y_end_in_sprite() { return stats4_y_start_in_sprite() + stats_font_height; }
  int stats5_y_start_in_sprite() { return stats4_y_end_in_sprite() + stats_between_margin; }
  int stats5_y_end_in_sprite() { return stats5_y_start_in_sprite() + stats_font_height; }
  int stats6_y_start_in_sprite() { return stats5_y_end_in_sprite() + stats_between_margin; }
  int stats6_y_end_in_sprite() { return stats6_y_start_in_sprite() + stats_font_height; }
  std::string lines_label() { return stats_label_chars_count == 3 ? "LNS" : "L"; }
  std::string level_label() { return stats_label_chars_count == 3 ? "LVL" : "V"; }
  std::string goal_label() { return stats_label_chars_count == 3 ? "GOL" : "G"; }
  std::string tetris_label() { return stats_label_chars_count == 3 ? "TET" : "T"; }
  std::string tspins_label() { return stats_label_chars_count == 3 ? "TSP" : "S"; }
  std::string combos_label() { return stats_label_chars_count == 3 ? "REN" : "R"; }
  std::string tpm_label() { return stats_label_chars_count == 3 ? "TPM" : "P"; }
  std::string lpm_label() { return stats_label_chars_count == 3 ? "LPM" : "C"; }

  int board_box_width() { return 2 + board_sprite_width(); }
  int board_box_height() { return 2 + board_sprite_height(); }
  int board_box_x_start() { return (screen_width - board_box_width()) / 2; }
  int board_box_x_end() { return board_box_x_start() + board_box_width(); }
  int board_box_y_start() { return board_top_margin; }
  int board_box_y_end() { return board_box_y_start() + board_box_height(); }
  int board_sprite_width() { return board_mino_block_size * 10; }
  int board_sprite_height() { return board_mino_block_size * 20; }
  int board_sprite_x_start() { return board_box_x_start() + 1; }
  int board_sprite_y_start() { return board_box_y_start() + 1; }

  int next_minos_box_width() { return 2 + next_minos_mino_left_margin * 2 + next_minos_mino_block_size * 4; }
  int next_minos_box_height() { return 2 + next_minos_mino_top_margin * 7 + next_minos_mino_block_size * 2 * 6; }
  int next_minos_box_x_start() { return board_box_x_end() + next_minos_box_left_margin; }
  int next_minos_box_y_start() { return next_minos_label_top_margin + next_minos_label_font_height + next_minos_box_top_margin; }
  int next_minos_label_top_center_pos_x() { return next_minos_box_x_start() + next_minos_box_width() / 2; }
  int next_minos_label_top_center_pos_y() { return next_minos_label_top_margin; }
  int next_minos_sprite_width() { return next_minos_mino_block_size * 4; }
  int next_minos_sprite_height() { return next_minos_mino_block_size * 2; }
  int next_minos_sprite_x_start() { return next_minos_box_x_start() + 1 + next_minos_mino_left_margin; }
  int next_minos_sprite0_y_start() { return next_minos_box_y_start() + 1 + next_minos_mino_top_margin; }
  int next_minos_sprite0_y_end() { return next_minos_sprite0_y_start() + next_minos_sprite_height(); }
  int next_minos_sprite1_y_start() { return next_minos_sprite0_y_end() + next_minos_mino_top_margin; }
  int next_minos_sprite1_y_end() { return next_minos_sprite1_y_start() + next_minos_sprite_height(); }
  int next_minos_sprite2_y_start() { return next_minos_sprite1_y_end() + next_minos_mino_top_margin; }
  int next_minos_sprite2_y_end() { return next_minos_sprite2_y_start() + next_minos_sprite_height(); }
  int next_minos_sprite3_y_start() { return next_minos_sprite2_y_end() + next_minos_mino_top_margin; }
  int next_minos_sprite3_y_end() { return next_minos_sprite3_y_start() + next_minos_sprite_height(); }
  int next_minos_sprite4_y_start() { return next_minos_sprite3_y_end() + next_minos_mino_top_margin; }
  int next_minos_sprite4_y_end() { return next_minos_sprite4_y_start() + next_minos_sprite_height(); }
  int next_minos_sprite5_y_start() { return next_minos_sprite4_y_end() + next_minos_mino_top_margin; }
  int next_minos_sprite5_y_end() { return next_minos_sprite5_y_start() + next_minos_sprite_height(); }
};

const DisplayParameters disp_param_240x240 {
  .screen_width = 240,
  .screen_height = 240,

  // hold
  .hold_label_font = 2,
  .hold_label_font_height = 16,
  .hold_label_top_margin = 9,
  .hold_box_top_margin = 2,
  .hold_box_left_margin = 11,
  .hold_mino_block_size = 8,
  .hold_mino_bevel = 1,
  .hold_mino_top_margin = 4,
  .hold_mino_left_margin = 4,

  // score
  .score_font = 1,
  .score_font_height = 7,
  .score_label_top_margin = 11,
  .score_label_left_margin = 5,
  .score_top_margin = 6,

  // time
  .time_font = 1,
  .time_font_height = 7,
  .time_label_top_margin = 6,
  .time_label_left_margin = 5,
  .time_top_margin = 6,

  // other stats; use sprite on values
  .stats_font = 1,
  .stats_label_chars_count = 3,
  .stats_label_left_margin = 5,
  .stats_font_width = 5,
  .stats_font_height = 7,
  .stats_top_margin = 12,
  .stats_left_margin = 6,
  .stats_between_margin = 6,

  // board
  .board_mino_block_size = 11,
  .board_mino_bevel = 2,
  .board_top_margin = 9,
  .board_left_margin = 5,

  // next minos
  .next_minos_label_font = 2,
  .next_minos_label_font_height = 16,
  .next_minos_label_top_margin = 9,
  .next_minos_box_top_margin = 2,
  .next_minos_box_left_margin = 12,
  .next_minos_mino_block_size = 8,
  .next_minos_mino_bevel = 1,
  .next_minos_mino_top_margin = 8,
  .next_minos_mino_left_margin = 4,
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

enum class TetrisMode {
  Endless, L40, L150, L999
};

class YomoTetris {
  public:
    DisplayParameters dp;

    TetrisMode mode;

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

    Input input;
    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;
    TFT_eSprite next_minos_sprite0;
    TFT_eSprite next_minos_sprite1;
    TFT_eSprite next_minos_sprite2;
    TFT_eSprite next_minos_sprite3;
    TFT_eSprite next_minos_sprite4;
    TFT_eSprite next_minos_sprite5;
    TFT_eSprite hold_sprite;
    TFT_eSprite stats_sprite;

  public:
    YomoTetris(Input input, TFT_eSPI &screen, DisplayParameters params) :
      dp(params),

      mode(TetrisMode::Endless),

      cur_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      next_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      mino_idx(0),

      board(),

      last_kick_index(0),
      was_last_move_rotation(false),
      last_moved_at(0),
      last_horizontally_moved_at(0),
      free_fall_timer(0),
      lockdown_judging(false),
      move_cnt_while_lockdown_judging(0),
      hold_once_tried(false),

      mino_placed(0),
      game_started_at(0),
      score(0),
      removed_lines(0),
      starting_level(1),
      goal(0),
      tetris_count(0),
      tspins(0),
      combos(-1), // combos starts count when 2 consecutive clear happens, and it is counted as "1 combo", so it's good to start with -1
      in_b2b(false),

      input(input),
      bgcolor(TFT_BLACK),
      screen(screen),
      board_sprite(&screen),
      next_minos_sprite0(&screen),
      next_minos_sprite1(&screen),
      next_minos_sprite2(&screen),
      next_minos_sprite3(&screen),
      next_minos_sprite4(&screen),
      next_minos_sprite5(&screen),
      hold_sprite(&screen),
      stats_sprite(&screen)
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

    boolean try_place_mino(Mino m, unsigned long now) {
      if (!board.mino_placable(m)) return false;
      board.place_mino(m);
      free_fall_timer = now;
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

    void pop_new_mino_if_needed(unsigned long now) {
      if (board.cur_mino_exists()) return;
      Mino m = next_mino();
      if (!try_place_mino(m, now)) {
        Serial.println("Game over!");
        while (true) delay(1000);
      }
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

    void lock_mino_and_clear_lines() {
      // T-spin check
      TSpinKind tspin = check_tspin();
      if (tspin == TSpinKind::TSPIN || tspin == TSpinKind::TSPIN_MINI) tspins++;
      last_kick_index = 0; // reset last_kick_index for the next check

      board.lockdown_mino();
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
        return;
      }

      for (int i = 0; i < 3; i++) {
        // delete animation
        // render flashed (white) lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->override_base_color(TFT_WHITE);
          }
        }
        render();
        delay(30);
        // render original lines
        for (int r = 0; r < count; r++) {
          for (int col = 0; col < 10; col++) {
            board.blocks[rows[r]][col]->recover_base_color();
          }
        }
        render();
        delay(30);
      }
      board.clear_lines(rows, count);

      // if some lines cleared, record combos
      combos++;

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
    }

    int current_level() {
      return starting_level + (removed_lines / 10);
    }

    void start() {
      /* hold area */
      hold_sprite.createSprite(dp.hold_sprite_width(), dp.hold_sprite_height());
      render_square(dp.hold_box_x_start(), dp.hold_box_y_start(), dp.hold_box_width(), dp.hold_box_height());
      render_centered_label("Hold", dp.hold_label_top_center_pos_x_start(), dp.hold_label_top_center_pos_y_start(), dp.hold_label_font);

      /* score area */
      render_left_label("Score", dp.score_label_x_start(), dp.score_label_y_start(), dp.score_font);

      /* time area */
      render_left_label("Time", dp.time_label_x_start(), dp.time_label_y_start(), dp.time_font);

      /* stats labels area */
      stats_sprite.createSprite(dp.statvals_sprite_width(), dp.statvals_sprite_height());
      std::string lbl = mode == TetrisMode::Endless ? dp.lines_label() : dp.goal_label();
      render_left_label(lbl,               dp.stats_label_x_start(), dp.stats0_y_start(), dp.stats_font);
      render_left_label(dp.level_label(),  dp.stats_label_x_start(), dp.stats1_y_start(), dp.stats_font);
      render_left_label(dp.tetris_label(), dp.stats_label_x_start(), dp.stats2_y_start(), dp.stats_font);
      render_left_label(dp.tspins_label(), dp.stats_label_x_start(), dp.stats3_y_start(), dp.stats_font);
      render_left_label(dp.combos_label(), dp.stats_label_x_start(), dp.stats4_y_start(), dp.stats_font);
      render_left_label(dp.tpm_label(),    dp.stats_label_x_start(), dp.stats5_y_start(), dp.stats_font);
      render_left_label(dp.lpm_label(),    dp.stats_label_x_start(), dp.stats6_y_start(), dp.stats_font);

      /* board area */
      board_sprite.createSprite(dp.board_sprite_width(), dp.board_sprite_height());
      render_square(dp.board_box_x_start(), dp.board_box_y_start(), dp.board_box_width(), dp.board_box_height());

      /* next_minos area */
      next_minos_sprite0.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      next_minos_sprite1.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      next_minos_sprite2.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      next_minos_sprite3.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      next_minos_sprite4.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      next_minos_sprite5.createSprite(dp.next_minos_sprite_width(), dp.next_minos_sprite_height());
      render_square(dp.next_minos_box_x_start(), dp.next_minos_box_y_start(), dp.next_minos_box_width(), dp.next_minos_box_height());
      render_centered_label("Next", dp.next_minos_label_top_center_pos_x(), dp.next_minos_label_top_center_pos_y(), dp.next_minos_label_font);

      /* yomogi area */
      screen.setSwapBytes(true);
      screen.pushImage(183, 187, YOMOGI_WIDTH, YOMOGI_HEIGHT, yomogi, YOMOGI_TRANSPARENT);

      const int FREE_FALL_MS = 1000;

      unsigned long last_soft_dropped = 0;
      boolean horizontal_auto_repeat_started = false;

      ButtonState prev_input;

      game_started_at = millis();

      while (true) {
        unsigned long now = millis();
        ButtonState btns = input.get();

        // pop mino
        pop_new_mino_if_needed(now);

        // check hard drop
        if (btns.UP && !prev_input.UP) {
          int i = 0;
          // animation
          while (try_move(MoveDirection::DOWN, 1, now)) {
            i++;
            if(i % 3 == 0) render();
          }
          score += 2 * i;
          render();
          lock_mino_and_clear_lines();
          pop_new_mino_if_needed(now);
        }

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
              render();
              delay(30);
              // render original lines
              board.cur_mino->recover_base_color();
              if (hold_mino.has_value()) hold_mino->recover_base_color();
              render();
              delay(30);
            }

            // temporary save current hold mino
            std::optional<Mino> temp = hold_mino;

            // next hold mino is current mino
            hold_mino = Mino::for_board(board.cur_mino->type);

            // next mino is holded one if hold exists, else next_mino();
            Mino next = temp.has_value() ? *temp : next_mino();

            if (!try_place_mino(next, now)) {
              Serial.println("Game over!");
              while (true) delay(1000);
            }
          }
        }

        // rotation
        if (btns.A && !prev_input.A) try_rotate(RotateDirection::CLOCKWISE, now);
        else if (btns.B && !prev_input.B) try_rotate(RotateDirection::COUNTER_CLOCKWISE, now);

        // softdrop
        if (btns.DOWN) {
          // when DOWN button press held, soft drop needs some interval
          boolean soft_drop_interval_passed = (now - last_soft_dropped) >= FREE_FALL_MS / 20;

          // when the previous press was not DOWN, or soft drop interval has passed, soft drop happens
          if (!prev_input.DOWN || soft_drop_interval_passed) {
            if (try_move(MoveDirection::DOWN, 1, now)) score += 1;
            last_soft_dropped = now;
          }
        }

        // free fall
        if (now - free_fall_timer >= FREE_FALL_MS) {
          try_move(MoveDirection::DOWN, 1, now);
        }

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

        if (board.mino_landed()) {
          if (!lockdown_judging) lockdown_judging = true;
          if (now - last_moved_at >= lockdown_wait_ms || move_cnt_while_lockdown_judging >= lockdown_reset_move_limit) {
            lock_mino_and_clear_lines();
          }
        } else if (lockdown_judging) {
          // in case once landed and judge started, but now it's not landed, reset them.
          // this happens when once landed, but moved horizontally, then it's not landed now
          lockdown_judging = false;
          move_cnt_while_lockdown_judging = 0;
        }

        render();
        prev_input = btns;
      }
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

    void render_mino_on_board(Mino m) {
      render_mino_on_sprite_with_offset(board_sprite, m, 0, 0, dp.board_mino_block_size, dp.board_mino_bevel);
    }

    void render_mino_on_next_minos(TFT_eSprite& sprite, Mino m, int x_offset, int y_offset) {
      render_mino_on_sprite_with_offset(sprite, m, x_offset, y_offset, dp.next_minos_mino_block_size, dp.next_minos_mino_bevel);
    }

    void render_mino_on_hold_box(Mino m, int x_offset, int y_offset) {
      render_mino_on_sprite_with_offset(hold_sprite, m, x_offset, y_offset, dp.hold_mino_block_size, dp.hold_mino_bevel);
    }

    void render_ghost_mino_on_board(Mino m, int hard_drop_distance) {
      uint16_t color = m.overridden_color.has_value() ? *m.overridden_color : m.color.base;
      for (int i = 0; i < 4; i++) render_ghost_block(board_sprite, m.positions[i].row+hard_drop_distance, m.positions[i].col, dp.board_mino_block_size, color);
    }

    void render_blocks_on_board() {
      for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 10; col++) {
          if (!board.block_exists(row, col)) render_empty_block(board_sprite, row, col, dp.board_mino_block_size);
          else render_block(board_sprite, row, col, 0, 0, dp.board_mino_block_size, dp.board_mino_bevel, board.blocks[row][col]->color.base, board.blocks[row][col]->color.lighter, board.blocks[row][col]->color.darker);
        }
      }
    }

    void render_next_mino(int num) {
      Mino m = Mino::for_next_minos(next_mino_type(num));
      int x_offset = m.is_I() ? 0 : m.is_O() ? dp.next_minos_mino_block_size : dp.next_minos_mino_block_size / 2;
      int y_offset = m.is_I() ? (dp.next_minos_mino_block_size / 2) : dp.next_minos_mino_block_size;
      if (num == 0) render_mino_on_next_minos(next_minos_sprite0, m, x_offset, y_offset);
      if (num == 1) render_mino_on_next_minos(next_minos_sprite1, m, x_offset, y_offset);
      if (num == 2) render_mino_on_next_minos(next_minos_sprite2, m, x_offset, y_offset);
      if (num == 3) render_mino_on_next_minos(next_minos_sprite3, m, x_offset, y_offset);
      if (num == 4) render_mino_on_next_minos(next_minos_sprite4, m, x_offset, y_offset);
      if (num == 5) render_mino_on_next_minos(next_minos_sprite5, m, x_offset, y_offset);
    }

    void render_hold_mino() {
      Mino m = Mino::for_hold_box(hold_mino->type);
      // in case hold mino color is overridden. This is needed because this does not directly renders hold_mino but
      // it creates a new Mino instance m. This is not a good design
      m.color = hold_mino->color;
      int x_offset = m.is_I() ? 0 : m.is_O() ? dp.hold_mino_block_size : dp.hold_mino_block_size / 2;
      int y_offset = m.is_I() ? dp.hold_mino_block_size+(dp.hold_mino_block_size / 2) : dp.hold_mino_block_size*2;
      render_mino_on_hold_box(m, x_offset, y_offset);
    }

    void render() {
      board_sprite.fillSprite(bgcolor);

      // blocks
      render_blocks_on_board();

      // ghosts and current mino
      if (board.cur_mino_exists()) {
        render_ghost_mino_on_board(*board.cur_mino, board.hard_drop_distance());
        render_mino_on_board(*board.cur_mino);
      }

      board_sprite.pushSprite(dp.board_sprite_x_start(), dp.board_sprite_y_start());

      // next minos
      next_minos_sprite0.fillSprite(bgcolor);
      render_next_mino(0);
      next_minos_sprite0.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite0_y_start());
      next_minos_sprite1.fillSprite(bgcolor);
      render_next_mino(1);
      next_minos_sprite1.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite1_y_start());
      next_minos_sprite2.fillSprite(bgcolor);
      render_next_mino(2);
      next_minos_sprite2.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite2_y_start());
      next_minos_sprite3.fillSprite(bgcolor);
      render_next_mino(3);
      next_minos_sprite3.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite3_y_start());
      next_minos_sprite4.fillSprite(bgcolor);
      render_next_mino(4);
      next_minos_sprite4.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite4_y_start());
      next_minos_sprite5.fillSprite(bgcolor);
      render_next_mino(5);
      next_minos_sprite5.pushSprite(dp.next_minos_sprite_x_start(), dp.next_minos_sprite5_y_start());

      // hold
      hold_sprite.fillSprite(bgcolor);
      if (hold_mino.has_value()) {
        render_hold_mino();
        hold_sprite.pushSprite(dp.hold_mino_sprite_x_start(), dp.hold_mino_sprite_y_start());
      }

      // score
      render_right_label(std::to_string(score), dp.score_time_x_end(), dp.score_y_start(), dp.score_font);

      // time
      unsigned long elapsed_ms = millis() - game_started_at;
      int minutes = elapsed_ms / (1000 * 60);
      int seconds = (elapsed_ms / 1000) % 60;
      int centis = (elapsed_ms % 1000) / 10;
      char time[9];
      snprintf(time, sizeof(time), "%02d:%02d:%02d", minutes, seconds, centis);
      render_right_label(time, dp.score_time_x_end(), dp.time_y_start(), dp.time_font);

      // stats
      stats_sprite.fillRect(0, 0, dp.statvals_sprite_width(), dp.statvals_sprite_height(), bgcolor);

      double elapsed_min = elapsed_ms / 60000.0;
      float tpm = elapsed_ms >= 5000 ? mino_placed / elapsed_min : 0;
      char tpm_str[6];
      snprintf(tpm_str, sizeof(tpm_str), "%.1f", tpm);
      float lpm = elapsed_ms >= 5000 ? removed_lines / elapsed_min : 0;
      char lpm_str[6];
      snprintf(lpm_str, sizeof(lpm_str), "%.1f", lpm);

      int top_stat = mode == TetrisMode::Endless ? removed_lines : goal;

      render_right_label_sprite(stats_sprite, std::to_string(top_stat),                dp.statvals_x_end_in_sprite(), dp.stats0_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(current_level()),         dp.statvals_x_end_in_sprite(), dp.stats1_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(tetris_count),            dp.statvals_x_end_in_sprite(), dp.stats2_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(tspins),                  dp.statvals_x_end_in_sprite(), dp.stats3_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(combos < 0 ? 0 : combos), dp.statvals_x_end_in_sprite(), dp.stats4_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, tpm_str,                                 dp.statvals_x_end_in_sprite(), dp.stats5_y_start_in_sprite(), dp.stats_font);
      render_right_label_sprite(stats_sprite, lpm_str,                                 dp.statvals_x_end_in_sprite(), dp.stats6_y_start_in_sprite(), dp.stats_font);

      stats_sprite.pushSprite(dp.statvals_x_start(), dp.statvals_y_start());
    }
};
