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
 * Mino, Board and Game
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

  public:
    Game(Input input, TFT_eSPI &screen, DisplayParameters params, GameMode mode) :
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

      const int FREE_FALL_MS = 1000;

      unsigned long last_soft_dropped = 0;
      boolean horizontal_auto_repeat_started = false;

      ButtonState prev_input;

      game_started_at = millis();

      boolean hard_dropped = false;

      while (true) {
        unsigned long now = millis();
        ButtonState btns = input.get();

        // pop mino if needed
        if (!board.cur_mino_exists()) {
          Mino m = next_mino();
          if (!try_place_mino(m, now)) return game_fail();
        }

        // check hard drop
        if (btns.UP && !prev_input.UP) {
          int i = 0;
          // animation
          while (try_move(MoveDirection::DOWN, 1, now)) {
            i++;
            if(i % 3 == 0) render();
          }
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

              if (!try_place_mino(next, now)) return game_fail();

              hold_count++;
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
        }

        if (board.mino_landed()) {
          if (!lockdown_judging) lockdown_judging = true;
          if (hard_dropped || (now - last_moved_at >= lockdown_wait_ms || move_cnt_while_lockdown_judging >= lockdown_reset_move_limit)) {
            if (hard_dropped) hard_dropped = false;
            if (!lock_mino_and_clear_lines()) return game_fail();
          }
        } else if (lockdown_judging) {
          // in case once landed and judge started, but now it's not landed, reset them.
          // this happens when once landed, but moved horizontally, then it's not landed now
          lockdown_judging = false;
          move_cnt_while_lockdown_judging = 0;
        }

        render();
        prev_input = btns;

        if (!(mode == GameMode::Endless) && goal <= 0) return game_clear();

        yield();
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

    void render_next_minos() {
      for (int i = 0; i < 6; i++) {
        Mino m = Mino::for_next_minos(next_mino_type(i));
        int x_offset = m.is_I() ? 0 : m.is_O() ? dp.next_minos_mino_block_size : dp.next_minos_mino_block_size / 2;
        int y_offset = m.is_I() ? (dp.next_minos_mino_block_size / 2) : dp.next_minos_mino_block_size;
        if (i == 0) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y0_in_sprite + y_offset);
        if (i == 1) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y1_in_sprite + y_offset);
        if (i == 2) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y2_in_sprite + y_offset);
        if (i == 3) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y3_in_sprite + y_offset);
        if (i == 4) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y4_in_sprite + y_offset);
        if (i == 5) render_mino_on_next_minos(next_minos_sprite, m, x_offset, dp.next_minos_mino_y5_in_sprite + y_offset);
      }
    }

    void render_hold_mino() {
      Mino m = Mino::for_hold_box(hold_mino->type);
      // in case hold mino color is overridden. This is needed because this does not directly renders hold_mino but
      // it creates a new Mino instance m. This is not a good design
      m.color = hold_mino->color;
      int x_offset = m.is_I() ? 0 : m.is_O() ? dp.hold_mino_block_size : dp.hold_mino_block_size / 2;
      int y_offset = m.is_I() ? (dp.hold_mino_block_size / 2) : dp.hold_mino_block_size;
      render_mino_on_hold_box(m, x_offset, y_offset);
    }

    void render() {
      /* left side */

      // hold
      hold_sprite.fillSprite(bgcolor);
      if (hold_mino.has_value()) {
        render_hold_mino();
        hold_sprite.pushSprite(dp.hold_sprite_x, dp.hold_sprite_y);
      }

      // score
      render_right_label(std::to_string(score), dp.score_time_x_right, dp.score_y, dp.stats_font);

      // time
      unsigned long elapsed_ms = millis() - game_started_at;
      int minutes = elapsed_ms / (1000 * 60);
      int seconds = (elapsed_ms / 1000) % 60;
      int centis = (elapsed_ms % 1000) / 10;
      char time[9];
      snprintf(time, sizeof(time), "%02d:%02d:%02d", minutes, seconds, centis);
      render_right_label(time, dp.score_time_x_right, dp.time_y, dp.stats_font);

      // stats
      stats_sprite.fillRect(0, 0, dp.stats_sprite_width, dp.stats_sprite_height, bgcolor);
      char tpm_str[6];
      char lpm_str[6];
      snprintf(tpm_str, sizeof(tpm_str), "%.1f", elapsed_ms >= 3000 ? current_tpm() : 0);
      snprintf(lpm_str, sizeof(lpm_str), "%.1f", elapsed_ms >= 3000 ? current_lpm() : 0);

      int top_stat = mode == GameMode::Endless ? removed_lines : goal;
      render_right_label_sprite(stats_sprite, std::to_string(top_stat),                dp.stats_x_right_in_sprite, dp.lines_or_goal_y_in_sprite, dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(current_level()),         dp.stats_x_right_in_sprite, dp.level_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(tetris_count),            dp.stats_x_right_in_sprite, dp.tetris_y_in_sprite,        dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(tspins),                  dp.stats_x_right_in_sprite, dp.tspin_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, std::to_string(combos < 0 ? 0 : combos), dp.stats_x_right_in_sprite, dp.combo_y_in_sprite,         dp.stats_font);
      render_right_label_sprite(stats_sprite, tpm_str,                                 dp.stats_x_right_in_sprite, dp.tpm_y_in_sprite,           dp.stats_font);
      render_right_label_sprite(stats_sprite, lpm_str,                                 dp.stats_x_right_in_sprite, dp.lpm_y_in_sprite,           dp.stats_font);
      stats_sprite.pushSprite(dp.stats_sprite_x, dp.stats_sprite_y);

      /* board */

      board_sprite.fillSprite(bgcolor);
      render_blocks_on_board();
      if (board.cur_mino_exists()) {
        render_ghost_mino_on_board(*board.cur_mino, board.hard_drop_distance());
        render_mino_on_board(*board.cur_mino);
      }
      board_sprite.pushSprite(dp.board_sprite_x, dp.board_sprite_y);

      /* next mino */
      next_minos_sprite.fillSprite(bgcolor);
      render_next_minos();
      next_minos_sprite.pushSprite(dp.next_minos_sprite_x, dp.next_minos_sprite_y);
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

        Game game = Game(input, screen, dp, cur_focus_mode);
        GameResult result = game.start();
        delay(1000);
        show_result(result);
        delay(100);
      }
    }
};
