#pragma once
#include <Arduino.h>

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

      ButtonState bs = ButtonState();
      if (stable_state[0]) bs.APressed();
      if (stable_state[1]) bs.BPressed();
      if (stable_state[2]) bs.STARTPressed();
      if (stable_state[3]) bs.SELECTPressed();
      if (stable_state[4]) bs.RIGHTPressed();
      if (stable_state[5]) bs.UPPressed();
      if (stable_state[6]) bs.DOWNPressed();
      if (stable_state[7]) bs.LEFTPressed();
      return bs;
    }
};
