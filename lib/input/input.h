#pragma once
#include <Arduino.h>

namespace input {
  class Button {
    public:
      Button(int pin) : pin(pin) { pinMode(pin, INPUT_PULLUP); }

      bool read() {
        unsigned long now = millis();
        bool raw = (digitalRead(pin) == LOW);
        if (raw != last_raw) {
          last_raw = raw;
          last_change = now;
        } else if (raw != stable && (now - last_change) >= 10) { // debounce
          stable = raw;
        }
        return stable;
      }

    private:
      int pin;
      bool stable{false};
      bool last_raw{false};
      unsigned long last_change{millis()};
  };

  struct ButtonState {
    boolean A{false};
    boolean B{false};
    boolean START{false};
    boolean SELECT{false};
    boolean RIGHT{false};
    boolean UP{false};
    boolean DOWN{false};
    boolean LEFT{false};
  };

  class Buttons {
    public:
      Buttons(int pin_A, int pin_B, int pin_START, int pin_SELECT, int pin_RIGHT, int pin_UP, int pin_DOWN, int pin_LEFT) :
        buttons{Button(pin_A), Button(pin_B), Button(pin_START), Button(pin_SELECT), Button(pin_RIGHT), Button(pin_UP), Button(pin_DOWN), Button(pin_LEFT)} {}

      ButtonState get() {
        return ButtonState{
          buttons[0].read(), buttons[1].read(), buttons[2].read(), buttons[3].read(),
          buttons[4].read(), buttons[5].read(), buttons[6].read(), buttons[7].read(),
        };
      }

    private:
      Button buttons[8];
  };

  struct VolumeButtonState {
    boolean UP{false};
    boolean DOWN{false};
  };

  class VolumeButtons {
    public:
      VolumeButtons(int pin_UP, int pin_DOWN) : buttons{Button(pin_UP), Button(pin_DOWN)} {}

      VolumeButtonState get() {
        return VolumeButtonState{buttons[0].read(), buttons[1].read()};
      }

    private:
      Button buttons[2];
  };
} // namespace input
