#pragma once

#include <TFT_eSPI.h>

#include <audio.h>
#include <input.h>

namespace yomogame {

struct Context {
  TFT_eSPI& screen;
  audio::Audio& audio;
  input::Buttons& buttons;
};

class Scene {
  public:
    virtual ~Scene() = default;
    virtual void enter(Context&) {}
    virtual Scene* tick(Context&) = 0;
    virtual void exit(Context&) {}
};

} // namespace yomogame
