#pragma once
#include <array>
#include <optional>
#include <tuple>

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <input.h>

#define LOGF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

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

enum class MinoDirection {
  NORTH, EAST, SOUTH, WEST
};

enum class RotateDirection {
  CLOCKWISE, COUNTER_CLOCKWISE
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

enum class MinoType {
  O, I, T, L, J, S, Z
};

class Mino {
  public:
    MinoType type;
    BlockColor color;
    std::array<BlockPos, 4> positions;
    Pivot pivot;
    MinoDirection cur_direction;

    Mino(MinoType type, uint16_t base_color, uint16_t lighter_color, uint16_t darker_color, std::array<BlockPos, 4> positions, Pivot pivot, MinoDirection direction) : color(base_color, lighter_color, darker_color), positions(positions), pivot(pivot), type(type), cur_direction(direction) {}

    static Mino from_type(MinoType type) {
      return Mino::from_type_row_col(type, 0, type == MinoType::O || type == MinoType::Z ? 4 : 3);
    }

    static Mino from_type_row_col(MinoType type, int row, int col) {
      if (type == MinoType::O) return Mino(type, 63456, 63468, 38048, /* yellow */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col+1)}}, Pivot(0, 0), MinoDirection::NORTH);
      if (type == MinoType::I) return Mino(type, 1694, 26398, 1010, /* sky blue */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row, col+3)}}, Pivot(row+0.5, col+1.5), MinoDirection::NORTH);
      if (type == MinoType::T) return Mino(type, 40989, 49981, 24593, /* purple */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+1)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::L) return Mino(type, 62242, 62733, 37345, /* orange */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::J) return Mino(type, 8254, 29502, 4114, /* blue */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col)}}, Pivot(row, col+1), MinoDirection::NORTH);
      if (type == MinoType::S) return Mino(type, 6049, 28589, 3200, /* green */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col+1), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
      /* if (type == MinoType::Z) */ return Mino(type, 63521, 64301, 36864, /* red */ {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col-1)}}, Pivot(row, col), MinoDirection::NORTH);
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


enum class MoveDirection {
  UP, DOWN, LEFT, RIGHT
};

class Block {
  public:
    BlockColor color;
    Block(BlockColor color) : color(color) {}
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

    boolean rotate(RotateDirection dir) {
      if (cur_mino->is_O()) return false; // o does not rotate

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
          return true;
        }
      }
      return false;
    }

    boolean mino_landed() {
      return !can_move_mino(MoveDirection::DOWN, 1);
    }

    void fix_mino() {
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

    std::tuple<boolean, std::array<boolean, 20>> deletable_rows() {
      boolean exists = false;
      std::array<boolean, 20> result = {};
      for (int row = 0; row < 20; row++) {
        result[row] = is_full_row(row);
        if (result[row]) exists = true;
      }
      return {exists, result};
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

    void clear_lines(std::array<boolean, 20> deletable) {
      int write_row = 19;
      for (int read_row = 19; read_row >= 0; read_row--) {
        if (!deletable[read_row]) {
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
};

const int block_size = 11;
const int bevel = 2;

const int board_grid_top_left_x = 64;
const int board_grid_top_left_y = 9;

const int next_minos_block_size = 8;
const int next_minos_grid_top_left_x = 187;
const int next_minos_grid_top_left_y = 27;
const int next_minos_left_margin = 4;
const int next_minos_right_margin = 4;
const int next_minos_top_margin = 8;
const int next_minos_bottom_margin = 8;
const int next_minos_between_margin = 8;

const int hold_block_size = 8;
const int hold_grid_top_left_x = 11;
const int hold_grid_top_left_y = 27;
const int hold_left_margin = 4;
const int hold_right_margin = 4;
const int hold_top_margin = 4;
const int hold_bottom_margin = 4;

const int horizontal_move_first_wait_ms = 300;
const int horizontal_move_auto_repeating_wait_ms = 50;
const int lockdown_wait_ms = 500;
const int lockdown_reset_move_limit = 15;

class YomoTetris_240x240 {
  public:
    std::array<MinoType, 7> cur_bag;
    std::array<MinoType, 7> next_bag;
    int mino_idx;

    Board board;

    std::optional<Mino> hold_mino;

    unsigned long last_moved_at;
    unsigned long last_horizontally_moved_at;
    unsigned long free_fall_timer;
    boolean lockdown_judging;
    int move_cnt_while_lockdown_judging;

    Input input;
    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;
    std::array<TFT_eSprite, 6> next_minos_sprites;
    TFT_eSprite hold_sprite;

  public:
    YomoTetris_240x240(Input input, TFT_eSPI &screen) :
      cur_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      next_bag{MinoType::L, MinoType::J, MinoType::I, MinoType::O, MinoType::S, MinoType::Z, MinoType::T},
      mino_idx(0),

      board(),

      last_moved_at(0),
      last_horizontally_moved_at(0),
      free_fall_timer(0),
      lockdown_judging(false),
      move_cnt_while_lockdown_judging(0),

      input(input),
      bgcolor(TFT_BLACK),
      screen(screen),
      board_sprite(&screen),
      next_minos_sprites{TFT_eSprite(&screen), TFT_eSprite(&screen), TFT_eSprite(&screen), TFT_eSprite(&screen), TFT_eSprite(&screen), TFT_eSprite(&screen)},
      hold_sprite(&screen)
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
      return Mino::from_type(next);
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
      return true;
    }

    boolean try_rotate(RotateDirection dir, unsigned long now) {
      boolean rotated = board.rotate(dir);
      if (!rotated) return false;
      if (lockdown_judging) move_cnt_while_lockdown_judging++;
      last_moved_at = now;
      return true;
    }

    void start() {
      /* board area */
      board_sprite.createSprite(10 * block_size, 20 * block_size);
      render_square(board_grid_top_left_x, board_grid_top_left_y, block_size * 10 + 2, block_size * 20 + 2);

      /* next_minos area */
      for (int i = 0; i < 6; i++) next_minos_sprites[i].createSprite(next_minos_block_size * 4, next_minos_block_size * 2);
      const int next_minos_grid_width = next_minos_left_margin + next_minos_block_size * 4 + next_minos_right_margin + 2;
      const int next_minos_grid_height = next_minos_top_margin + next_minos_block_size * 2 * 6 + next_minos_between_margin * 5 + next_minos_bottom_margin + 2;
      render_square(next_minos_grid_top_left_x, next_minos_grid_top_left_y, next_minos_grid_width, next_minos_grid_height);
      render_centered_label("Next", next_minos_grid_top_left_x + next_minos_grid_width / 2, next_minos_grid_top_left_y - 18, 2);

      /* hold area */
      hold_sprite.createSprite(4 * hold_block_size, 4 * hold_block_size);
      const int hold_grid_width = hold_left_margin + hold_block_size * 4 + hold_right_margin + 2;
      const int hold_grid_height = hold_top_margin + hold_block_size * 4 + hold_bottom_margin + 2;
      render_square(hold_grid_top_left_x, hold_grid_top_left_y, hold_grid_width, hold_grid_height);
      render_centered_label("Hold", hold_grid_top_left_x + hold_grid_width / 2, hold_grid_top_left_y - 18, 2);

      const int FREE_FALL_MS = 1000;

      unsigned long last_soft_dropped = 0;
      boolean horizontal_auto_repeat_started = false;

      boolean was_up = false;
      boolean was_down = false;
      boolean was_a = false;
      boolean was_b = false;
      boolean was_right = false;
      boolean was_left = false;

      boolean hold_once_tried = false;

      while (true) {
        // delete rows with animation
        auto [deletable_rows_exists, deletable] = board.deletable_rows();
        if (deletable_rows_exists) {
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) {
              board.delete_block(i, 4);
              board.delete_block(i, 5);
            }
          }
          render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) {
              board.delete_block(i, 3);
              board.delete_block(i, 6);
            }
          }
          render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) {
              board.delete_block(i, 2);
              board.delete_block(i, 7);
            }
          }
          render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) {
              board.delete_block(i, 1);
              board.delete_block(i, 8);
            }
          }
          render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) {
              board.delete_block(i, 0);
              board.delete_block(i, 9);
            }
          }
          render();
          board.clear_lines(deletable);
          render();
        }

        unsigned long now = millis();

        // new mino pop
        if (!board.cur_mino_exists()) {
          Mino m = next_mino();
          if (!try_place_mino(m, now)) {
            Serial.println("Game over!");
            while (true) delay(1000);
          }

          render();
          continue;
        }

        ButtonState btns = input.get();

        // hard drop
        if (btns.UP && !was_up) {
          was_up = true;
          int i = 0;
          while (try_move(MoveDirection::DOWN, 1, now)) {
            i++;
            if(i % 3 == 0) render();
          }
          render();
          board.fix_mino();
          if (hold_once_tried) hold_once_tried = false;
          continue;
        }
        was_up = btns.UP;

        // hold
        // because R button does not exist, uses SELECT press as hold
        if (btns.SELECT) {
          if (!hold_once_tried && board.cur_mino_exists()) {
            hold_once_tried = true;

            // temporary save current hold mino
            std::optional<Mino> temp = hold_mino;

            // next hold mino is current mino
            hold_mino = Mino::from_type(board.cur_mino->type);

            // next mino is holded one if hold exists, else next_mino();
            Mino next = temp.has_value() ? *temp : next_mino();

            if (!try_place_mino(next, now)) {
              Serial.println("Game over!");
              while (true) delay(1000);
            }

            render();
          }
        }

        // rotation
        if (btns.A && !was_a) {
          try_rotate(RotateDirection::CLOCKWISE, now);
        } else if (btns.B && !was_b) {
          try_rotate(RotateDirection::COUNTER_CLOCKWISE, now);
        }
        was_a = btns.A;
        was_b = btns.B;

        // softdrop
        if (btns.DOWN) {
          // when DOWN button press held, soft drop needs some interval
          boolean soft_drop_interval_passed = (now - last_soft_dropped) >= FREE_FALL_MS / 20;

          // when the previous press was not DOWN, or soft drop interval has passed, soft drop happens
          if (!was_down || soft_drop_interval_passed) {
            try_move(MoveDirection::DOWN, 1, now);
            last_soft_dropped = now;
          }
        }
        was_down = btns.DOWN;

        // free fall
        if (now - free_fall_timer >= FREE_FALL_MS) {
          try_move(MoveDirection::DOWN, 1, now);
        }

        // horizontal move
        if (btns.RIGHT || btns.LEFT) {
          MoveDirection dir = btns.RIGHT ? MoveDirection::RIGHT : MoveDirection::LEFT;

          if (btns.RIGHT && btns.LEFT) {
            // on both pressed, do nothing

          } else if ((btns.RIGHT && !was_right) || (btns.LEFT && !was_left)) {
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
        was_right = btns.RIGHT;
        was_left = btns.LEFT;

        if (board.mino_landed()) {
          if (!lockdown_judging) lockdown_judging = true;
          if (now - last_moved_at >= lockdown_wait_ms || move_cnt_while_lockdown_judging >= lockdown_reset_move_limit) {
            board.fix_mino();
            if (hold_once_tried) hold_once_tried = false;
          }
        } else if (lockdown_judging) {
          // in case once landed and judge started, but now it's not landed, reset them.
          // this happens when once landed, but moved horizontally, then it's not landed now
          lockdown_judging = false;
          move_cnt_while_lockdown_judging = 0;
        }

        render();
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

    void render_centered_label(const char *string, int x, int y, uint8_t font) {
      screen.setTextColor(TFT_WHITE, bgcolor);
      screen.setTextDatum(TC_DATUM);
      screen.drawString(string, x, y, font);
    }

    void render_empty_block(TFT_eSprite& sprite, int row, int col, int block_size) {
      sprite.fillRect(col * block_size, row * block_size, block_size, block_size, bgcolor);
    }

    void render_block(TFT_eSprite& sprite, int row, int col, int x_offset, int y_offset, int block_size, BlockColor color) {
      sprite.fillRect(x_offset + col * block_size,         y_offset + row * block_size,         block_size,           block_size,           color.darker);
      sprite.fillRect(x_offset + col * block_size,         y_offset + row * block_size,         block_size - bevel,   block_size - bevel,   color.lighter);
      sprite.fillRect(x_offset + col * block_size + bevel, y_offset + row * block_size + bevel, block_size - bevel*2, block_size - bevel*2, color.base);
    }

    void render_ghost_block(TFT_eSprite& sprite, int row, int col, int block_size, int distance, BlockColor color) {
      sprite.drawRect(col * block_size,     (row + distance) * block_size,     block_size,     block_size,     color.base);
      sprite.drawRect(col * block_size + 1, (row + distance) * block_size + 1, block_size - 2, block_size - 2, color.base);
    }

    void render_mino(TFT_eSprite& sprite, Mino m, int x_offset, int y_offset, int block_size) {
      for (int i = 0; i < 4; i++) {
        render_block(sprite, m.positions[i].row, m.positions[i].col, x_offset, y_offset, block_size, m.color);
      }
    }

    void render_ghost_mino(TFT_eSprite& sprite, Mino m, int block_size, int distance) {
      for (int i = 0; i < 4; i++) {
        render_ghost_block(sprite, m.positions[i].row, m.positions[i].col, block_size, distance, m.color);
      }
    }

    void render() {
      board_sprite.fillSprite(bgcolor);

      // blocks
      for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 10; col++) {
          if (!board.block_exists(row, col)) {
            render_empty_block(board_sprite, row, col, block_size);
          } else {
            for (int j = 0; j < 4; j++) {
              render_block(board_sprite, row, col, 0, 0, block_size, board.blocks[row][col]->color);
            }
          }
        }
      }

      // ghosts and current mino
      if (board.cur_mino_exists()) {
        render_ghost_mino(board_sprite, *board.cur_mino, block_size, board.hard_drop_distance());
        render_mino(board_sprite, *board.cur_mino, 0, 0, block_size);
      }

      board_sprite.pushSprite(board_grid_top_left_x+1, board_grid_top_left_y+1);

      // next minos
      for (int s = 0; s < 6; s++) {
        next_minos_sprites[s].fillSprite(bgcolor);
        MinoType t = next_mino_type(s);
        Mino m = Mino::from_type_row_col(t, t == MinoType::I ? 0 : 1, t == MinoType::Z ? 1 : 0);
        int x = m.is_I() ? 0 : m.is_O() ? next_minos_block_size : next_minos_block_size / 2;
        int y = m.is_I() ? next_minos_block_size / 2 : 0;

        render_mino(next_minos_sprites[s], m, x, y, next_minos_block_size);
        next_minos_sprites[s].pushSprite(1 + next_minos_grid_top_left_x + next_minos_left_margin, 1 + next_minos_grid_top_left_y + next_minos_top_margin + (next_minos_block_size * 2 + next_minos_between_margin) * s);
      }

      // hold
      hold_sprite.fillSprite(bgcolor);
      if (hold_mino.has_value()) {
        MinoType t = hold_mino->type;
        Mino m = Mino::from_type_row_col(t, t == MinoType::I ? 0 : 1, t == MinoType::Z ? 1 : 0);
        int x = m.is_I() ? 0 : m.is_O() ? hold_block_size : hold_block_size / 2;
        int y = m.is_I() ? hold_block_size + hold_block_size / 2 : hold_block_size;

        render_mino(hold_sprite, m, x, y, hold_block_size);
        hold_sprite.pushSprite(1 + hold_grid_top_left_x + hold_left_margin, 1 + hold_grid_top_left_y + hold_top_margin);
      }
    }
};
