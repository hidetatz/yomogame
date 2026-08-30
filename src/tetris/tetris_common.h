#pragma once
 
#include <array>
#include <cmath>
#include <optional>
#include <tuple>
#include <string>
 
#include <Arduino.h>

struct BlockColor {
  uint16_t base;
  uint16_t lighter;
  uint16_t darker;
};

struct BlockPos {
  int row = 0;
  int col = 0;

  void up(int distance) { row -= distance; }
  void down(int distance) { row += distance; }
  void right(int distance) { col += distance; }
  void left(int distance) { col -= distance; }
};

struct Block {
  uint16_t base_color{0}; // for backup
  BlockColor color{0, 0, 0};

  void flash() { color.base = TFT_WHITE; }
  void stop_flash() { color.base = base_color; }
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


class Mino {
  public:
    MinoType type;
    uint16_t base_color; // for backup
    BlockColor color;
    std::array<BlockPos, 4> positions;
    Pivot pivot;
    MinoDirection cur_direction;

    Mino(MinoType type, uint16_t base_color, uint16_t lighter_color, uint16_t darker_color, std::array<BlockPos, 4> positions, Pivot pivot, MinoDirection direction) : base_color(base_color), color{base_color, lighter_color, darker_color}, positions(positions), pivot(pivot), type(type), cur_direction(direction) {}

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
      if (type == MinoType::O) return Mino(type, 63456, 63468, 38048, /* yellow */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row-1, col}, BlockPos{row-1, col+1}}}, Pivot(0, 0), MinoDirection::NORTH);
      if (type == MinoType::I) return Mino(type, 1694, 26398, 1010, /* sky blue */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row, col+2}, BlockPos{row, col+3}}}, Pivot(row+0.5, col+1.5), MinoDirection::NORTH);
      if (type == MinoType::T) return Mino(type, 40989, 49981, 24593, /* purple */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row, col+2}, BlockPos{row-1, col+1}}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::L) return Mino(type, 62242, 62733, 37345, /* orange */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row, col+2}, BlockPos{row-1, col+2}}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::J) return Mino(type, 8254, 29502, 4114, /* blue */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row, col+2}, BlockPos{row-1, col}}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::S) return Mino(type, 6049, 28589, 3200, /* green */ {{BlockPos{row, col}, BlockPos{row, col+1}, BlockPos{row-1, col+1}, BlockPos{row-1, col+2}}}, Pivot(row, col+1), MinoDirection::NORTH);
      /* if (type == MinoType::Z) */ return Mino(type, 63521, 64301, 36864, /* red */ {{BlockPos{row, col+1}, BlockPos{row, col+2}, BlockPos{row-1, col}, BlockPos{row-1, col+1}}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    void flash() { color.base = TFT_WHITE; }
    void stop_flash() { color.base = base_color; }

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

    boolean is_O() { return type == MinoType::O; }
    boolean is_I() { return type == MinoType::I; }
    boolean is_T() { return type == MinoType::T; }

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
};


class Board {
  public:
    std::optional<Mino> cur_mino;
    std::optional<Block> blocks[20][10]; // block position is managed by the index in blocks, not BlockPos
    Board() {}

    boolean cur_mino_exists() { return cur_mino.has_value(); }
    boolean block_exists(int row, int col) { return blocks[row][col].has_value(); }

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

    void place_mino(Mino m) { cur_mino = m; }

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

    boolean mino_landed() { return !can_move_mino(MoveDirection::DOWN, 1); }

    boolean lockdown_mino() {
      boolean locked_out = false;
      for (int i = 0; i < 4; i++) {
        int row = cur_mino->positions[i].row;
        int col = cur_mino->positions[i].col;
        if (row < 0) {
          locked_out = true;
          continue;
        }
        blocks[row][col] = Block{cur_mino->color.base, cur_mino->color};
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

    void delete_block(int row, int col) { blocks[row][col] = std::nullopt; }

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
