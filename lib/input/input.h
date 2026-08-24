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


