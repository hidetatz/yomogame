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
  g += (31 - g) * amount;
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

// const uint16_t yellow = TFT_YELLOW;
// constexpr uint16_t yellow_l = lighten(yellow, 0.4);
// constexpr uint16_t yellow_d = darken(yellow, 0.4);
// const uint16_t lightblue = TFT_SKYBLUE;
// constexpr uint16_t lightblue_l = lighten(lightblue, 0.4);
// constexpr uint16_t lightblue_d = darken(lightblue, 0.4);
// const uint16_t purple = TFT_PURPLE;
// constexpr uint16_t purple_l = lighten(purple, 0.4);
// constexpr uint16_t purple_d = darken(purple, 0.4);
// const uint16_t orange = TFT_ORANGE;
// constexpr uint16_t orange_l = lighten(orange, 0.4);
// constexpr uint16_t orange_d = darken(orange, 0.4);
// const uint16_t darkblue = TFT_DARKCYAN;
// constexpr uint16_t darkblue_l = lighten(darkblue, 0.4);
// constexpr uint16_t darkblue_d = darken(darkblue, 0.4);
// const uint16_t green = TFT_GREEN;
// constexpr uint16_t green_l = lighten(green, 0.4);
// constexpr uint16_t green_d = darken(green, 0.4);
// const uint16_t red = TFT_RED;
// constexpr uint16_t red_l = lighten(red, 0.4);
// constexpr uint16_t red_d = darken(red, 0.4);

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

class BlockPos {
  public:
    int row;
    int col;
    BlockPos(int row, int col) : row(row), col(col) {}
};

class Mino {
  public:
    BlockColor color;
    std::array<BlockPos, 4> positions;
    Mino(uint16_t base_color, std::array<BlockPos, 4> positions) : color(base_color), positions(positions) {}
    Mino() : color(0), positions{BlockPos(0, 0), BlockPos(0, 0), BlockPos(0, 0), BlockPos(0, 0)} {}
};

class MinoO : public Mino {
  public:
    MinoO(int row, int col) : Mino(TFT_YELLOW, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col+1)}}) {}
};

class MinoI : public Mino {
  public:
    MinoI(int row, int col) : Mino(TFT_SKYBLUE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row, col+3)}}) {}
};

class MinoT : public Mino {
  public:
    MinoT(int row, int col) : Mino(TFT_PURPLE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+1)}}) {}
};

class MinoL : public Mino {
  public:
    MinoL(int row, int col) : Mino(TFT_ORANGE, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col+2)}}) {}
};

class MinoJ : public Mino {
  public:
    MinoJ(int row, int col) : Mino(TFT_DARKCYAN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row, col+2), BlockPos(row-1, col)}}) {}
};

class MinoS : public Mino {
  public:
    MinoS(int row, int col) : Mino(TFT_GREEN, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col+1), BlockPos(row-1, col+2)}}) {}
};

class MinoZ : public Mino {
  public:
    MinoZ(int row, int col) : Mino(TFT_RED, {{BlockPos(row, col), BlockPos(row, col+1), BlockPos(row-1, col), BlockPos(row-1, col-1)}}) {}
};

const int down = 0;
const int left = 1;
const int right = 2;

const int BLOCK_SIZE = 11;
const int BEVEL = 2;
const int ROWS = 20;
const int COLS = 10;
const int GRID_TOP_LEFT_X = 64;
const int GRID_TOP_LEFT_Y = 15;
const int GRID_WIDTH = BLOCK_SIZE * COLS + 2;
const int GRID_HEIGHT = BLOCK_SIZE * ROWS + 2;

class Board {
  private:
    int x;
    int y;
    uint16_t bgcolor;
    Mino cur_mino;
    std::optional<BlockColor> block_colors[ROWS][COLS];
    TFT_eSprite sprite;

  public:
    Board(int x, int y, uint16_t bgcolor, TFT_eSPI* screen) : x(x), y(y), bgcolor(bgcolor), cur_mino(), sprite(screen) {
      sprite.createSprite(COLS * BLOCK_SIZE, ROWS * BLOCK_SIZE);
    }

    boolean mino_placable(Mino m) {
      for (int i = 0; i < 4; i++) {
        if (!placable_at(m.positions[i])) return false;
      }
      return true;
    }

    boolean placable_at(BlockPos pos) {
      if (pos.col < 0 || COLS <= pos.col || ROWS <= pos.row) return false;
      if (pos.row < 0) return true;
      return !block_colors[pos.row][pos.col].has_value();
    }

    void place_mino(Mino m) {
      cur_mino = m;
    }

    boolean can_move_mino(int dir, int distance) {
      for (int i = 0; i < 4; i++) {
        BlockPos newpos = cur_mino.positions[i];
        if (dir == down) newpos.row += distance;
        else if (dir == right) newpos.col += distance;
        else newpos.col -= distance;
        if (!placable_at(newpos)) return false;
      }
      return true;
    }

    void move_mino(int dir, int distance) {
      for (int i = 0; i < 4; i++) {
        if (dir == down) cur_mino.positions[i].row += distance;
        else if (dir == right) cur_mino.positions[i].col += distance;
        else cur_mino.positions[i].col -= distance;
      }
    }

    boolean mino_landed() {
      for (int i = 0; i < 4; i++) {
        if (cur_mino.positions[i].row == ROWS-1) return true;
        if (block_colors[cur_mino.positions[i].row + 1][cur_mino.positions[i].col].has_value()) return true;
      }
      return false;
    }

    void fix_mino() {
      for (int i = 0; i < 4; i++) {
        block_colors[cur_mino.positions[i].row][cur_mino.positions[i].col] = cur_mino.color;
      }
    }

    int hard_drop_distance() {
      for (int distance = 20; distance >= 1; distance--) {
        if (can_move_mino(down, distance)) {
          return distance;
        }
      }
      return 0;
    }

    boolean deletable_rows_exists() {
      for (int row = 0; row < ROWS; row++) {
        boolean all_block_exists = true;
        for (int col = 0; col < COLS; col++) {
          if (!block_colors[row][col].has_value()) {
            all_block_exists = false;
            break;
          }
        }
        if (all_block_exists) return true;
      }
      return false;
    }

    std::array<boolean, 20> deletable_rows() {
      std::array<boolean, 20> result = {};
      for (int row = 0; row < ROWS; row++) {
        boolean all_block_exists = true;
        for (int col = 0; col < COLS; col++) {
          if (!block_colors[row][col].has_value()) {
            all_block_exists = false;
            break;
          }
        }
        result[row] = all_block_exists;
      }
      return result;
    }

    void delete_rows_animated(int row, int col1, int col2) {
      block_colors[row][col1] = std::nullopt;
      block_colors[row][col2] = std::nullopt;
    }

    void fill_deleted_lines() {
      int write_row = ROWS - 1;

      for (int read_row = ROWS - 1; read_row >= 0; read_row--) {
        boolean row_has_block = false;
        for (int col = 0; col < COLS; col++) {
          if (block_colors[read_row][col].has_value()) { row_has_block = true; break; }
        }

        if (row_has_block) {
          if (write_row != read_row) {
            for (int col = 0; col < COLS; col++) {
              block_colors[write_row][col] = block_colors[read_row][col];
              block_colors[read_row][col] = std::nullopt;
            }
          }
          write_row--;
        }
      }
    }

    void render() {
      sprite.fillSprite(bgcolor);

      // blocks
      for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 10; col++) {
          if (!block_colors[row][col].has_value()) {
            sprite.fillRect(col * BLOCK_SIZE, row * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, TFT_BLACK);
          } else {
            for (int j = 0; j < 4; j++) {
              sprite.fillRect(col * BLOCK_SIZE, row * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, block_colors[row][col]->base);
              sprite.fillRect(col * BLOCK_SIZE, row * BLOCK_SIZE, BLOCK_SIZE - BEVEL, BLOCK_SIZE - BEVEL, block_colors[row][col]->lighter);
              sprite.fillRect(col * BLOCK_SIZE + BEVEL, row * BLOCK_SIZE + BEVEL, BLOCK_SIZE - BEVEL*2, BLOCK_SIZE - BEVEL*2, block_colors[row][col]->darker);
            }
          }
        }
      }

      // ghosts
      int distance = hard_drop_distance();
      for (int i = 0; i < 4; i++) {
        sprite.drawRect(cur_mino.positions[i].col * BLOCK_SIZE, (cur_mino.positions[i].row + distance) * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, cur_mino.color.base);
        sprite.drawRect(cur_mino.positions[i].col * BLOCK_SIZE + 1, (cur_mino.positions[i].row + distance) * BLOCK_SIZE + 1, BLOCK_SIZE - 2, BLOCK_SIZE - 2, cur_mino.color.base);
      }

      // cur_mino
      for (int i = 0; i < 4; i++) {
        BlockPos b = cur_mino.positions[i];
        sprite.fillRect(b.col * BLOCK_SIZE, b.row * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, cur_mino.color.darker);
        sprite.fillRect(b.col * BLOCK_SIZE, b.row * BLOCK_SIZE, BLOCK_SIZE - BEVEL, BLOCK_SIZE - BEVEL, cur_mino.color.lighter);
        sprite.fillRect(b.col * BLOCK_SIZE + BEVEL, b.row * BLOCK_SIZE + BEVEL, BLOCK_SIZE - BEVEL*2, BLOCK_SIZE - BEVEL*2, cur_mino.color.base);
      }
      sprite.pushSprite(x, y);
    }
};

class Tetris {
  private:
    TFT_eSPI screen;
    Input input;
    std::array<Mino, 7> bag;
    int mino_idx;

  public:
    Tetris(TFT_eSPI screen, Input input) : screen(screen), input(input), bag{MinoL(0, 3), MinoJ(0, 3), MinoI(0, 3), MinoO(0, 4), MinoS(0, 3), MinoZ(0, 3), MinoT(0, 3)}, mino_idx(0) {}

    void shuffle_bag() {
      for (int i = 0; i < 7; i++) {
        int r = random(i, 7);
        Mino temp = bag[i];
        bag[i] = bag[r];
        bag[r] = temp;
      }
    }

    Mino randomMino() {
      return MinoO(0, 4);
      Mino next = bag[mino_idx];
      mino_idx++;
      if (mino_idx == 7) {
        shuffle_bag();
        mino_idx = 0;
      }
      return next;
    }

    void start() {
      /* board grid */
      screen.drawFastHLine(GRID_TOP_LEFT_X, GRID_TOP_LEFT_Y, GRID_WIDTH, TFT_WHITE); // top left to right
      screen.drawFastHLine(GRID_TOP_LEFT_X, GRID_TOP_LEFT_Y + GRID_HEIGHT - 1, GRID_WIDTH, TFT_WHITE); // bottom left to right
      screen.drawFastVLine(GRID_TOP_LEFT_X, GRID_TOP_LEFT_Y + 1, GRID_HEIGHT - 2, TFT_WHITE); // top left to down
      screen.drawFastVLine(GRID_TOP_LEFT_X + GRID_WIDTH - 1, GRID_TOP_LEFT_Y + 1, GRID_HEIGHT - 2, TFT_WHITE); // top right to down

      Board board = Board(GRID_TOP_LEFT_X+1, GRID_TOP_LEFT_Y+1, TFT_BLACK, &screen);

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
            if (deletable[i]) board.delete_rows_animated(i, 4, 5);
          }
          board.render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) board.delete_rows_animated(i, 3, 6);
          }
          board.render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) board.delete_rows_animated(i, 2, 7);
          }
          board.render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) board.delete_rows_animated(i, 1, 8);
          }
          board.render();
          delay(50);
          for (int i = 0; i < 20; i++) {
            if (deletable[i]) board.delete_rows_animated(i, 0, 9);
          }
          board.render();
          board.fill_deleted_lines();
          board.render();
        }

        if (need_new_mino) {
          Mino m = randomMino();
          if (!board.mino_placable(m)) {
            Serial.println("Game over");
            while (true) delay(1000);
          }
          board.place_mino(m);
          need_new_mino = false;
          board.render();
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
            if(i % 3 == 0) board.render();
          }
          board.render();
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

        board.render();
      }
    }
};
