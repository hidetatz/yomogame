#pragma once
#include <Arduino.h>

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

const int BUTTON_COUNT = 8;

class Input {
  private:
    int pins[BUTTON_COUNT];

    boolean stable_state[BUTTON_COUNT];
    boolean last_raw_state[BUTTON_COUNT];
    unsigned long last_change_at[BUTTON_COUNT];

    const unsigned long debounce_ms = 5;

  public:
    Input(int pinA, int pinB, int pinSTART, int pinSELECT, int pinRIGHT, int pinUP, int pinDOWN, int pinLEFT) {
      pins[0] = pinA;
      pins[1] = pinB;
      pins[2] = pinSTART;
      pins[3] = pinSELECT;
      pins[4] = pinRIGHT;
      pins[5] = pinUP;
      pins[6] = pinDOWN;
      pins[7] = pinLEFT;

      unsigned long now = millis();
      for (int i = 0; i < BUTTON_COUNT; i++) {
        pinMode(pins[i], INPUT_PULLUP);
        stable_state[i] = false;
        last_raw_state[i] = false;
        last_change_at[i] = now;
      }
    }

    ButtonState get() {
      unsigned long now = millis();

      for (int i = 0; i < BUTTON_COUNT; i++) {
        boolean raw = (digitalRead(pins[i]) == LOW);

        if (raw != last_raw_state[i]) {
          last_raw_state[i] = raw;
          last_change_at[i] = now;
        } else if (raw != stable_state[i] && (now - last_change_at[i]) >= debounce_ms) {
          stable_state[i] = raw;
        }
      }

      return ButtonState{
        stable_state[0], stable_state[1], stable_state[2], stable_state[3],
        stable_state[4], stable_state[5], stable_state[6], stable_state[7]
      };
    }
};
