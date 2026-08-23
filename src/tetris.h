#pragma once
#include <array>
#include <optional>
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

    void down(int distance) { row += distance; }
    void right(int distance) { col += distance; }
    void left(int distance) { col -= distance; }
};

class Mino {
  public:
    char kind;
    BlockColor color;
    std::array<BlockPos, 4> positions;
    Mino(char kind, uint16_t base_color, std::array<BlockPos, 4> positions) : color(base_color), positions(positions), kind(kind) {}

    static Mino O(int row, int col) {
      return Mino('O', TFT_YELLOW, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col+1)}});
    }

    static Mino I(int row, int col) {
        return Mino('I', TFT_SKYBLUE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row, col+3)}});
    }

    static Mino T(int row, int col) {
        return Mino('T', TFT_PURPLE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+1)}});
    }

    static Mino L(int row, int col) {
        return Mino('L', TFT_ORANGE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+2)}});
    }

    static Mino J(int row, int col) {
        return Mino('J', TFT_DARKCYAN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col)}});
    }

    static Mino S(int row, int col) {
        return Mino('S', TFT_GREEN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col+1), BlockPos(row-1, col+2)}});
    }

    static Mino Z(int row, int col) {
        return Mino('Z', TFT_RED, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col-1)}});
    }

    void down(int distance) {
      for (int i = 0; i < 4; i++) positions[i].down(distance);
    }

    void right(int distance) {
      for (int i = 0; i < 4; i++) positions[i].right(distance);
    }

    void left(int distance) {
      for (int i = 0; i < 4; i++) positions[i].left(distance);
    }
};

const int down = 0;
const int left = 1;
const int right = 2;

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

    boolean mino_placable(Mino m) {
      for (int i = 0; i < 4; i++) {
        if (!block_placable_at(m.positions[i].row, m.positions[i].col)) return false;
      }
      return true;
    }

    void place_mino(Mino m) {
      cur_mino = m;
    }

    boolean can_move_mino(int dir, int distance) {
      for (int i = 0; i < 4; i++) {
        BlockPos cur_pos = cur_mino->positions[i];
        int new_row = cur_pos.row;
        int new_col = cur_pos.col;
        if (dir == down) new_row += distance;
        else if (dir == right) new_col += distance;
        else new_col -= distance;
        if (!block_placable_at(new_row, new_col)) return false;
      }
      return true;
    }

    void move_mino(int dir, int distance) {
      if (dir == down) cur_mino->down(distance);
      else if (dir == right) cur_mino->right(distance);
      else cur_mino->left(distance);
    }

    boolean mino_landed() {
      return !can_move_mino(down, 1);
    }

    void fix_mino() {
      for (int i = 0; i < 4; i++) blocks[cur_mino->positions[i].row][cur_mino->positions[i].col] = Block(cur_mino->color);
      cur_mino = std::nullopt;
    }

    // how many down happens on hard drop?
    int hard_drop_distance() {
      int cur_limit = 0;
      for (int distance = 1; distance < 20; distance++) {
        if (!can_move_mino(down, distance)) break;
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

    boolean deletable_rows_exists() {
      for (int row = 0; row < 20; row++) {
        if (is_full_row(row)) return true;
      }
      return false;
    }

    std::array<boolean, 20> deletable_rows() {
      std::array<boolean, 20> result = {};
      for (int row = 0; row < 20; row++) result[row] = is_full_row(row);
      return result;
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

    void clear_lines() {
      int write_row = 19;
      for (int read_row = 19; read_row >= 0; read_row--) {
        boolean full = is_full_row(read_row);
        if (!full) {
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

    uint16_t bgcolor;
    TFT_eSPI &screen;
    TFT_eSprite board_sprite;

  public:
    Tetris(Input input, int block_size, int bevel, int board_grid_top_left_x, int board_grid_top_left_y, uint16_t bgcolor, TFT_eSPI screen) 
      : input(input), 
        bag{Mino::L(0, 3), Mino::J(0, 3), Mino::I(0, 3), Mino::O(0, 4), Mino::S(0, 3), Mino::Z(0, 3), Mino::T(0, 3)}, 
        mino_idx(0),
        block_size(block_size),
        bevel(bevel),
        board_grid_top_left_x(board_grid_top_left_x),
        board_grid_top_left_y(board_grid_top_left_y),
        board(),
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
      return Mino::O(0, 4);
      Mino next = bag[mino_idx];
      mino_idx++;
      if (mino_idx == 7) {
        shuffle_bag();
        mino_idx = 0;
      }
      return next;
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

      unsigned long gravity_tick = millis();
      boolean was_down = false;
      boolean need_new_mino = true;

      int held_dir = -1;
      unsigned long das_start = 0;
      boolean das_charged = false;
      unsigned long last_repeat = 0;
      unsigned long landed = 0;
      int land_reset_cnt = 0;

      while (true) {
        boolean deletable_rows_exists = board.deletable_rows_exists();
        if (deletable_rows_exists) {
          std::array<boolean, 20> deletable = board.deletable_rows();
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
          board.clear_lines();
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

        if (btns.UP) {
          int i = 0;
          while (board.can_move_mino(down, 1)) {
            board.move_mino(down, 1);
            if (landed != 0) {
              landed = 0;
              land_reset_cnt++;
            }
            i++;
            if(i % 3 == 0) render();
          }
          render();
          board.fix_mino();
          need_new_mino = true;
          delay(130);
          continue;
        }

        if (btns.DOWN && !was_down) {
          if (board.can_move_mino(down, 1)) {
            board.move_mino(down, 1);
            if (landed != 0) {
              landed = 0;
              land_reset_cnt++;
            }
          }
          gravity_tick = now;
        }
        was_down = btns.DOWN;

        if (now - gravity_tick >= (btns.DOWN ? SOFT_DROP_MS : GRAVITY_MS)) {
          if (board.can_move_mino(down, 1)) {
            board.move_mino(down, 1);
            if (landed != 0) {
              landed = 0;
              land_reset_cnt++;
            }
          }
          gravity_tick = now;
        }

        int dir = -1;
        if (btns.RIGHT && !btns.LEFT) dir = right;
        else if (btns.LEFT && !btns.RIGHT) dir = left;

        if (dir != held_dir) {
          held_dir = dir;
          das_charged = false;
          if (dir != -1) {
            if (board.can_move_mino(dir, 1)) {
              board.move_mino(dir, 1);
              if (landed != 0) {
                landed = 0;
                land_reset_cnt++;
              }
            }
            das_start = now;
          }
        } else if (dir != -1) {
          if (!das_charged) {
            if (now - das_start >= 300) {
              das_charged = true;
              last_repeat = now;
              if (board.can_move_mino(dir, 1)) {
                board.move_mino(dir, 1);
                if (landed != 0) {
                  landed = 0;
                  land_reset_cnt++;
                }
              }
            }
          } else {
            if (now - last_repeat >= 50) {
              last_repeat += 50;
              if (board.can_move_mino(dir, 1)) {
                board.move_mino(dir, 1);
                if (landed != 0) {
                  landed = 0;
                  land_reset_cnt++;
                }
              }
            }
          }
        }

        if (board.mino_landed()) {
          if (landed == 0) {
            landed = millis();
          }
          if (now - landed >= 500 || land_reset_cnt >= 15) {
            board.fix_mino();
            need_new_mino = true;
            landed = 0;
            land_reset_cnt = 0;
          }
        } else {
          landed = 0;
          land_reset_cnt = 0;
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
        // Serial.printf("hard_drop_distance: %d on cur_mino (%d, %d), (%d, %d), (%d, %d), (%d, %d)\n", distance, board.cur_mino->positions[0].row, board.cur_mino->positions[0].col, board.cur_mino->positions[1].row, board.cur_mino->positions[1].col, board.cur_mino->positions[2].row, board.cur_mino->positions[2].col, board.cur_mino->positions[3].row, board.cur_mino->positions[3].col);
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
