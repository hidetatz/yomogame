// Compiles the Peanut-GB implementation (a single-header library) and bridges
// its sound register hooks to minigb_apu.

#pragma GCC optimize("O3")

#define ENABLE_SOUND 1
#define PEANUT_GB_IS_LITTLE_ENDIAN 1

#include <stdint.h>

/* Peanut-GB (ENABLE_SOUND) calls these; declare before including it. */
uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "peanut_gb.h"
#include "minigb_apu.h"

// Peanut-GB (with ENABLE_SOUND) calls these globals for the 0xFF10-0xFF3F range.
static struct minigb_apu_ctx *g_apu = NULL;

void gb_bind_apu(struct minigb_apu_ctx *apu) { g_apu = apu; }

uint8_t audio_read(const uint16_t addr) {
  return g_apu ? minigb_apu_audio_read(g_apu, addr) : 0xFF;
}

void audio_write(const uint16_t addr, const uint8_t val) {
  if (g_apu) minigb_apu_audio_write(g_apu, addr, val);
}
