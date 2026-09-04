// Bridges the emulator's controller read (nes_get_gamepad_state) to yomogame's
// input::Buttons. NesScene binds the live Buttons instance before starting the
// emulator.

#include <input.h>
#include "nes_bridge.h"

// HW_MASK_* bits expected by osd.cpp
#define NES_HW_A      0x01
#define NES_HW_B      0x02
#define NES_HW_SELECT 0x04
#define NES_HW_START  0x08
#define NES_HW_UP     0x10
#define NES_HW_DOWN   0x20
#define NES_HW_LEFT   0x40
#define NES_HW_RIGHT  0x80

static input::Buttons* g_nes_buttons = nullptr;

void nes_bind_buttons(input::Buttons* b) { g_nes_buttons = b; }

extern "C" void setup_controller() {}   // pins already set up by input::Buttons

extern "C" int nes_get_gamepad_state() {
  if (!g_nes_buttons) return 0;
  input::ButtonState s = g_nes_buttons->get();
  int hw = 0;
  if (s.A)      hw |= NES_HW_A;
  if (s.B)      hw |= NES_HW_B;
  if (s.SELECT) hw |= NES_HW_SELECT;
  if (s.START)  hw |= NES_HW_START;
  if (s.UP)     hw |= NES_HW_UP;
  if (s.DOWN)   hw |= NES_HW_DOWN;
  if (s.LEFT)   hw |= NES_HW_LEFT;
  if (s.RIGHT)  hw |= NES_HW_RIGHT;
  return hw;
}
