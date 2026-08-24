#pragma once
#include <array>
#include <optional>
#include <tuple>

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define LOGF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

constexpr uint16_t lighten(uint16_t color, float amount) {
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = (color) & 0x1F;
  r += (31 - r) * amount;
  g += (63 - g) * amount;
  b += (31 - b) * amount;
  return (r << 11) | (g << 5) | b;
}

constexpr uint16_t darken(uint16_t color, float amount) {
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = (color) & 0x1F;
  r *= (1.0 - amount);
  g *= (1.0 - amount);
  b *= (1.0 - amount);
  return (r << 11) | (g << 5) | b;
}

class ButtonState {
  public:
    boolean A;
    boolean B;
    boolean START;
    boolean SELECT;
    boolean RIGHT;
    boolean UP;
    boolean DOWN;
    boolean LEFT;

    ButtonState() {
      A = false;
      B = false;
      START = false;
      SELECT = false;
      RIGHT = false;
      UP = false;
      DOWN = false;
      LEFT = false;
    }
    void APressed() {A = true;}
    void BPressed() {B = true;}
    void STARTPressed() {START = true;}
    void SELECTPressed() {SELECT = true;}
    void RIGHTPressed() {RIGHT = true;}
    void UPPressed() {UP = true;}
    void DOWNPressed() {DOWN = true;}
    void LEFTPressed() {LEFT = true;}
};

class Input {
  private:
   int pinA;
   int pinB;
   int pinSTART;
   int pinSELECT;
   int pinRIGHT;
   int pinUP;
   int pinDOWN;
   int pinLEFT;

  public:
    Input(int pinA, int pinB, int pinSTART, int pinSELECT, int pinRIGHT, int pinUP, int pinDOWN, int pinLEFT) :
      pinA(pinA), pinB(pinB), pinSTART(pinSTART), pinSELECT(pinSELECT), pinRIGHT(pinRIGHT), pinUP(pinUP), pinDOWN(pinDOWN), pinLEFT(pinLEFT) {
        pinMode(pinA, INPUT_PULLUP);
        pinMode(pinB, INPUT_PULLUP);
        pinMode(pinSTART, INPUT_PULLUP);
        pinMode(pinSELECT, INPUT_PULLUP);
        pinMode(pinRIGHT, INPUT_PULLUP);
        pinMode(pinUP, INPUT_PULLUP);
        pinMode(pinDOWN, INPUT_PULLUP);
        pinMode(pinLEFT, INPUT_PULLUP);
      }

    ButtonState get() {
      ButtonState bs = ButtonState();
      if (digitalRead(pinA) == LOW) bs.APressed();
      if (digitalRead(pinB) == LOW) bs.BPressed();
      if (digitalRead(pinSTART) == LOW) bs.STARTPressed();
      if (digitalRead(pinSELECT) == LOW) bs.SELECTPressed();
      if (digitalRead(pinRIGHT) == LOW) bs.RIGHTPressed();
      if (digitalRead(pinUP) == LOW) bs.UPPressed();
      if (digitalRead(pinDOWN) == LOW) bs.DOWNPressed();
      if (digitalRead(pinLEFT) == LOW) bs.LEFTPressed();
      return bs;
    }
};

// color of block in tetrimino
class BlockColor {
  public:
    uint16_t base;
    uint16_t lighter;
    uint16_t darker;
    BlockColor(uint16_t base) : base(base) {
      lighter = lighten(base, 0.4);
      darker = darken(base, 0.4);
    }
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

class Mino {
  public:
    char kind;
    BlockColor color;
    std::array<BlockPos, 4> positions;
    Pivot pivot;
    MinoDirection cur_direction;

    Mino(char kind, uint16_t base_color, std::array<BlockPos, 4> positions, Pivot pivot, MinoDirection direction) : color(base_color), positions(positions), pivot(pivot), kind(kind), cur_direction(direction) {}

    static Mino O(int row, int col) {
      return Mino('O', TFT_YELLOW, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col+1)}}, Pivot(0, 0), MinoDirection::NORTH);
    }

    static Mino I(int row, int col) {
        return Mino('I', TFT_SKYBLUE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row, col+3)}}, Pivot(row+0.5, col+1.5), MinoDirection::NORTH);
    }

    static Mino T(int row, int col) {
        return Mino('T', TFT_PURPLE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+1)}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    static Mino L(int row, int col) {
        return Mino('L', TFT_ORANGE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    static Mino J(int row, int col) {
        return Mino('J', TFT_DARKCYAN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col)}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    static Mino S(int row, int col) {
        return Mino('S', TFT_GREEN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col+1), BlockPos(row-1, col+2)}}, Pivot(row, col+1), MinoDirection::NORTH);
    }

    static Mino Z(int row, int col) {
        return Mino('Z', TFT_RED, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col-1)}}, Pivot(row, col), MinoDirection::NORTH);
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
      return kind == 'O';
    }

    boolean is_I() {
      return kind == 'I';
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
    }
};

class Tetris {
  private:
    Input input;

    std::array<Mino, 7> bag;
    int mino_idx;

    int block_size;
    int bevel;
    int board_grid_top_left_x;
    int board_grid_top_left_y;
    Board board;

    unsigned long last_landed_at;
    int lockdown_judge_reset_cnt;

    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;

  public:
    Tetris(Input input, int block_size, int bevel, int board_grid_top_left_x, int board_grid_top_left_y, uint16_t bgcolor, TFT_eSPI &screen)
      : input(input),
        bag{Mino::L(0, 3), Mino::J(0, 3), Mino::I(0, 3), Mino::O(0, 4), Mino::S(0, 3), Mino::Z(0, 3), Mino::T(0, 3)},
        mino_idx(0),
        block_size(block_size),
        bevel(bevel),
        board_grid_top_left_x(board_grid_top_left_x),
        board_grid_top_left_y(board_grid_top_left_y),
        board(),
        last_landed_at(0),
        lockdown_judge_reset_cnt(0),
        bgcolor(bgcolor),
        screen(screen),
        board_sprite(&screen) {}

    void shuffle_bag() {
      for (int i = 0; i < 7; i++) {
        int r = random(i, 7);
        Mino temp = bag[i];
        bag[i] = bag[r];
        bag[r] = temp;
      }
    }

    Mino randomMino() {
      Mino next = bag[mino_idx];
      mino_idx++;
      if (mino_idx == 7) {
        shuffle_bag();
        mino_idx = 0;
      }
      return next;
    }

    boolean try_move(MoveDirection dir, int distance) {
      if (!board.can_move_mino(dir, distance)) return false;
      board.move_mino(dir, 1);
      if (last_landed_at != 0) {
        // because the mino moved, landed_at timer must be reset, but reset_cnt is counted
        last_landed_at = 0;
        lockdown_judge_reset_cnt++;
      }
      return true;
    }

    boolean try_rotate(RotateDirection dir) {
      boolean rotated = board.rotate(dir);
      if (rotated && last_landed_at != 0) {
        // because the mino rotated, landed_at timer must be reset, but reset_cnt is counted
        last_landed_at = 0;
        lockdown_judge_reset_cnt++;
      }
      return rotated;
    }

    void start() {
      board_sprite.createSprite(10 * block_size, 20 * block_size);

      /* board grid */

      const int grid_width = block_size * 10 + 2;
      const int grid_height = block_size * 20 + 2;
      screen.drawFastHLine(board_grid_top_left_x,                  board_grid_top_left_y,                   grid_width,      TFT_WHITE); // top left to right
      screen.drawFastHLine(board_grid_top_left_x,                  board_grid_top_left_y + grid_height - 1, grid_width,      TFT_WHITE); // bottom left to right
      screen.drawFastVLine(board_grid_top_left_x,                  board_grid_top_left_y + 1,               grid_height - 2, TFT_WHITE); // top left to down
      screen.drawFastVLine(board_grid_top_left_x + grid_width - 1, board_grid_top_left_y + 1,               grid_height - 2, TFT_WHITE); // top right to down

      const int GRAVITY_MS = 1000;
      const int SOFT_DROP_MS = 50;

      boolean need_new_mino = true;

      unsigned long gravity_tick = millis();
      boolean was_down = false;
      boolean was_a = false;
      boolean was_b = false;
      boolean was_up = false;

      std::optional<MoveDirection> held_dir = std::nullopt;
      unsigned long das_start = 0;
      boolean das_charged = false;
      unsigned long last_repeat = 0;
      unsigned long landed = 0;
      int land_reset_cnt = 0;

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

        if (need_new_mino) {
          Mino m = randomMino();
          if (!board.mino_placable(m)) {
            Serial.println("Game over");
            while (true) delay(1000);
          }
          board.place_mino(m);
          need_new_mino = false;
          render();
          gravity_tick = millis();
          continue;
        }

        ButtonState btns = input.get();
        unsigned long now = millis();

        // hard drop
        if (btns.UP && !was_up) {
          was_up = true;
          int i = 0;
          while (try_move(MoveDirection::DOWN, 1)) {
            i++;
            if(i % 3 == 0) render();
          }
          render();
          board.fix_mino();
          need_new_mino = true;
          continue;
        }
        was_up = btns.UP;

        if (btns.A && !was_a) {
          try_rotate(RotateDirection::CLOCKWISE);
        } else if (btns.B && !was_b) {
          try_rotate(RotateDirection::COUNTER_CLOCKWISE);
        }
        was_a = btns.A;
        was_b = btns.B;

        if (btns.DOWN && !was_down) {
          try_move(MoveDirection::DOWN, 1);
          gravity_tick = now;
        }
        was_down = btns.DOWN;

        if (now - gravity_tick >= (btns.DOWN ? SOFT_DROP_MS : GRAVITY_MS)) {
          try_move(MoveDirection::DOWN, 1);
          gravity_tick = now;
        }

        std::optional<MoveDirection> dir = std::nullopt;
        if (btns.RIGHT && !btns.LEFT) dir = MoveDirection::RIGHT;
        else if (btns.LEFT && !btns.RIGHT) dir = MoveDirection::LEFT;

        if (dir != held_dir) {
          held_dir = dir;
          das_charged = false;
          if (dir.has_value()) {
            try_move(*dir, 1);
            das_start = now;
          }
        } else if (dir.has_value()) {
          if (!das_charged) {
            if (now - das_start >= 300) {
              das_charged = true;
              last_repeat = now;
              try_move(*dir, 1);
            }
          } else {
            if (now - last_repeat >= 50) {
              last_repeat += 50;
              try_move(*dir, 1);
            }
          }
        }

        if (board.mino_landed()) {
          if (last_landed_at == 0) {
            last_landed_at = millis();
          }
          if (now - last_landed_at >= 500 || lockdown_judge_reset_cnt >= 15) {
            board.fix_mino();
            need_new_mino = true;
            last_landed_at = 0;
            lockdown_judge_reset_cnt = 0;
          }
        } else {
          last_landed_at = 0;
          lockdown_judge_reset_cnt = 0;
        }

        render();
      }
    }

    void render() {
      int x = board_grid_top_left_x+1;
      int y = board_grid_top_left_y+1;

      board_sprite.fillSprite(bgcolor);

      // blocks
      for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 10; col++) {
          if (!board.block_exists(row, col)) {
            board_sprite.fillRect(col * block_size, row * block_size, block_size, block_size, TFT_BLACK);
          } else {
            for (int j = 0; j < 4; j++) {
              board_sprite.fillRect(col * block_size, row * block_size, block_size, block_size, board.blocks[row][col]->color.darker);
              board_sprite.fillRect(col * block_size, row * block_size, block_size - bevel, block_size - bevel, board.blocks[row][col]->color.lighter);
              board_sprite.fillRect(col * block_size + bevel, row * block_size + bevel, block_size - bevel*2, block_size - bevel*2, board.blocks[row][col]->color.base);
            }
          }
        }
      }

      if (board.cur_mino_exists()) {
        // ghosts
        int distance = board.hard_drop_distance();
        for (int i = 0; i < 4; i++) {
          board_sprite.drawRect(board.cur_mino->positions[i].col * block_size, (board.cur_mino->positions[i].row + distance) * block_size, block_size, block_size, board.cur_mino->color.base);
          board_sprite.drawRect(board.cur_mino->positions[i].col * block_size + 1, (board.cur_mino->positions[i].row + distance) * block_size + 1, block_size - 2, block_size - 2, board.cur_mino->color.base);
        }

        // cur_mino
        for (int i = 0; i < 4; i++) {
          BlockPos b = board.cur_mino->positions[i];
          board_sprite.fillRect(b.col * block_size, b.row * block_size, block_size, block_size, board.cur_mino->color.darker);
          board_sprite.fillRect(b.col * block_size, b.row * block_size, block_size - bevel, block_size - bevel, board.cur_mino->color.lighter);
          board_sprite.fillRect(b.col * block_size + bevel, b.row * block_size + bevel, block_size - bevel*2, block_size - bevel*2, board.cur_mino->color.base);
        }
      }
      board_sprite.pushSprite(x, y);
    }
};
