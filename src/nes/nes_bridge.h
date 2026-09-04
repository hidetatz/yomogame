#pragma once

#include <stdint.h>

// Glue between the (C) Nofrendo/DSN emulator and the (C++) yomogame shell.
// Implementations live in nes_scene.h (video/audio/frame hooks) and
// nes_controller.cpp (input).

#ifdef __cplusplus
extern "C" {
#endif

// The 240x240 RGB565 buffer the emulator should render this frame into
// (double-buffered; may change frame to frame). NULL if unavailable.
uint16_t* nes_framebuffer(void);

// Hand the just-rendered frame to the display task (called from osd.cpp
// vid_flush, on the emulator task).
void nes_blit_frame(const uint16_t* rgb565_240x240);

// Hand `samples` mono int16 samples (emulator sample rate) to the audio bridge.
void nes_audio_push(const int16_t* mono, int samples);

// Called at the end of every vid_flush (emulator task) — used to redraw the
// volume overlay on top of the just-pushed frame.
void nes_post_frame(void);

// Current controller state as HW_MASK_* bits (see osd.cpp).
int nes_get_gamepad_state(void);
void setup_controller(void);

#ifdef __cplusplus
}
#endif
