#include <Arduino.h>
#include <SPI.h>

#include <TFT_eSPI.h> 

#include <driver/i2s.h>

// 1:ok   2:LED  3:JTAG_EN  4:ok  5:ok     6:ok     7:ok     8:ok     9:ok  10:ok
// 11:ok 12:ok  13:ok      14:ok 15:uart? 16:uart? 17:uart? 18:uart? 19:usb 20:usbpio pkg list
// 21:ok 35:psram 36:psram 37:psram 38:ok 39:jtag(ok) 40:jtag(ok) 41:jtag(ok) 42:jtag(ok) 43:usb 44:usb
// 45:vspi 46:log 47:ok 48:LED

const int ledPin = 48;

const int btnA = 1;
const int btnB = 4;
const int btnS = 5;
const int btnE = 6;
const int btnR = 7;
const int btnU = 8;
const int btnD = 9;
const int btnL = 21;

  // -D TFT_CS=10
  // -D TFT_MOSI=11
  // -D TFT_SCLK=12
  // -D TFT_MISO=-1
  // -D TFT_DC=14
  // -D TFT_RST=38

const int i2sBCLK = 39;
const int i2sLRC = 40;
const int i2sDIN = 41;

const int samplerate = 44100;

TFT_eSPI screen = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  pinMode(btnA, INPUT_PULLUP);
  pinMode(btnB, INPUT_PULLUP);
  pinMode(btnS, INPUT_PULLUP);
  pinMode(btnE, INPUT_PULLUP);
  pinMode(btnR, INPUT_PULLUP);
  pinMode(btnU, INPUT_PULLUP);
  pinMode(btnD, INPUT_PULLUP);
  pinMode(btnL, INPUT_PULLUP);

  screen.init();
  screen.fillScreen(TFT_GREEN);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = samplerate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 6,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true 
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.print("I2S install failed\n");
    for (;;) delay(1000);
  }

  i2s_pin_config_t pin_config = {
    .bck_io_num = i2sBCLK,
    .ws_io_num = i2sLRC,
    .data_out_num = i2sDIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.print("I2S pin config failed\n");
    for (;;) delay(1000);
  }

  Serial.println("done!");
}

int pressed(int btn) {
  int state = digitalRead(btn);
  return state == LOW;
}

int lastColor = -1; // 直前に描画した色を記録

void updateScreen(int color) {
  if (color != lastColor) {
    screen.fillScreen(color);
    lastColor = color;
  }
}

// const float sndC = 65.406;
// const float sndD = 73.416;
// const float sndE = 82.407;
// const float sndF = 87.307;
// const float sndG = 97.999;
// const float sndA = 110.000;
// const float sndB = 123.471;
// const float sndC2 = 130.813;
// const float sndC = 130.813;
// const float sndD = 146.832;
// const float sndE = 164.814;
// const float sndF = 174.614;
// const float sndG = 195.998;
// const float sndA = 220.000;
// const float sndB = 246.942;
// const float sndC2 = 261.626;
const float sndC = 261.626;
const float sndD = 293.665;
const float sndE = 329.628;
const float sndF = 349.228;
const float sndG = 391.995;
const float sndA = 440.000;
const float sndB = 493.883;
const float sndC2 = 523.251;

double phase = 0.0;
float currentFreq = 0.0;
float lastFreq = 0.0;
float currentAmp = 0.0;
const float maxAmp = 3000.0;
const float attackTimeMs  = 100.0;
const float releaseTimeMs = 100.0;
const float attackStep  = maxAmp / (attackTimeMs  * samplerate / 1000.0);
const float releaseStep = maxAmp / (releaseTimeMs * samplerate / 1000.0);

void ring() {
  static int16_t buffer[256];
  size_t dummy;

  float targetAmp = (currentFreq > 0.0) ? maxAmp : 0.0;
  float playFreq = (currentFreq > 0.0) ? currentFreq : lastFreq;

  for (int i = 0; i < 256; i++) {
    if (currentAmp < targetAmp) {
      currentAmp += attackStep;      
      if (currentAmp > targetAmp) currentAmp = targetAmp;
    } else if (currentAmp > targetAmp) {
      currentAmp -= releaseStep;      
      if (currentAmp < targetAmp) currentAmp = targetAmp;
    }

    buffer[i] = (int16_t)(currentAmp * sin(2 * PI * playFreq * phase / samplerate));
    phase += 1.0;
  }

  
  if (currentFreq > 0.0) {
    lastFreq = currentFreq;
  }

  i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &dummy, portMAX_DELAY);
}

void loop() {
  if (pressed(btnA)) {
  //   Serial.println("A!");
    // updateScreen(TFT_RED);
    currentFreq = sndC2;
  } else if (pressed(btnB)) {
  //   Serial.println("B!");
    // updateScreen(TFT_YELLOW);
    currentFreq = sndB;
  } else if (pressed(btnS)) {
  //   Serial.println("S!");
    // updateScreen(TFT_BLUE);
    currentFreq = sndA;
  } else if (pressed(btnE)) {
  //   Serial.println("E!");
    // updateScreen(TFT_GREEN);
    currentFreq = sndG;
  } else if (pressed(btnR)) {
  //   Serial.println("R!");
    // updateScreen(TFT_WHITE);
    currentFreq = sndF;
  } else if (pressed(btnU)) {
  //   Serial.println("U!");
    // updateScreen(TFT_MAGENTA);
    currentFreq = sndE;
  } else if (pressed(btnD)) {
  //   Serial.println("D!");
    // updateScreen(TFT_ORANGE);
    currentFreq = sndD;
  } else if (pressed(btnL)) {
  //   Serial.println("L!");
    // updateScreen(TFT_CYAN);
    currentFreq = sndC;
  } else {
    // updateScreen(TFT_BLACK);
    currentFreq = 0.0;
  }

  ring();
}
