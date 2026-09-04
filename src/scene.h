#pragma once

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>
#include <volume_overlay.h>

namespace yomogame {

struct Context {
  TFT_eSPI& screen;
  audio::Audio& audio;
  input::Buttons& buttons;
  volui::VolumeOverlay& overlay;   // shell-owned; most scenes ignore it
};

class Scene {
  public:
    virtual ~Scene() = default;
    virtual void enter(Context&) {}
    virtual Scene* tick(Context&) = 0;
    virtual void exit(Context&) {}
    virtual bool owns_overlay() { return false; }
};

} // namespace yomogame
