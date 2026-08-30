#pragma once
 
#include <array>
#include <atomic>
#include <optional>
 
#include "tetris_common.h"

struct GameSnapshot {
  bool valid{false};

  // blocks
  std::optional<BlockColor> blocks[20][10];

  // current mino and ghost
  bool has_cur_mino{false};
  std::array<BlockPos, 4> cur_mino_block_pos;
  BlockColor cur_mino_color{0, 0, 0};
  int hard_drop_distance{0};

  // hold
  bool has_hold_mino{false};
  MinoType hold_type{MinoType::T};
  BlockColor hold_color{0, 0, 0};

  // next minos
  std::array<MinoType, 6> next_types{};

  // stats
  int score{0};
  int level{1};
  int tetris_count{0};
  int tspins{0};
  int combos{-1};
  int top_stat{0};
  bool is_endless{true};
  double tpm{0};
  double lpm{0};
  unsigned long elapsed_ms{0};
};

class TripleBuffer {
  private:
    GameSnapshot buffers[3];
    int write_idx;
    int read_idx;
    std::atomic<int> middle_idx;
    std::atomic<bool> has_new{false};

  public:
    TripleBuffer() : write_idx(0), read_idx(1), middle_idx(2) {}

    GameSnapshot& write_buf() { return buffers[write_idx]; }

    void publish() {
      int new_middle = write_idx;
      int old_middle = middle_idx.exchange(new_middle, std::memory_order_acq_rel);
      write_idx = old_middle;
      has_new.store(true, std::memory_order_release);
    }

    GameSnapshot& read_buf() {
      if (has_new.exchange(false, std::memory_order_acq_rel)) {
        int old_read = read_idx;
        read_idx = middle_idx.exchange(old_read, std::memory_order_acq_rel);
      }
      return buffers[read_idx];
    }
};
