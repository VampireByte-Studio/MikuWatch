#include <TFT_eSPI.h>
#include <chibimiku.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

int touchX, touchY;

// ---------------- Keypad ----------------
// Each of these codes triggers a different action when entered:
const char* CODE_DISPLAY01 = "001";
const char* CODE_CHIBI     = "101";
const char* CODE_WAVE      = "123";

String enteredCode = "";
TFT_eSPI_Button keypadBtn[10];      // digits 0-9
TFT_eSPI_Button clearBtn;

// Keypad layout constants (320x240 landscape)
const int KP_BTN_W = 80;
const int KP_BTN_H = 42;
const int KP_GAP_X = 15;
const int KP_GAP_Y = 6;
const int KP_START_X = 25;
const int KP_START_Y = 45;

enum AppState { KEYPAD, DISPLAY01, IMAGE_SHOWN, WAVE_ACTIVE };
AppState state = KEYPAD;

unsigned long lastGlitchTime = 0;
const unsigned long glitchInterval = 15000;

const int NUM_BARS = 16;
int barHeights[NUM_BARS];
int barTargets[NUM_BARS];
uint16_t barColors[NUM_BARS];
unsigned long lastWaveFrame = 0;
const unsigned long waveFrameInterval = 80;

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(34));

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);

  drawKeypadScreen();
}

void loop() {
  bool pressed = false;

  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    touchX = map(p.x, 200, 3700, tft.width(), 0);
    touchY = map(p.y, 240, 3800, tft.height(), 0);
    pressed = true;
  }

  switch (state) {
    case KEYPAD:
      handleKeypadTouch(pressed);
      break;

    case DISPLAY01:
      // static screen - any tap returns to the keypad
      if (pressed) {
        state = KEYPAD;
        drawKeypadScreen();
      }
      break;

    case IMAGE_SHOWN:
      if (millis() - lastGlitchTime >= glitchInterval) {
        PlayGlitchBurst();
        lastGlitchTime = millis();
      }
      if (pressed) {
        state = KEYPAD;
        drawKeypadScreen();
      }
      break;

    case WAVE_ACTIVE:
      if (millis() - lastWaveFrame >= waveFrameInterval) {
        updateWaveAnimation();
        lastWaveFrame = millis();
      }
      if (pressed) {
        state = KEYPAD;
        drawKeypadScreen();
      }
      break;
  }

  delay(10);
}

// ================= KEYPAD SCREEN =================

void drawKeypadScreen() {
  tft.fillScreen(TFT_BLACK);
  enteredCode = "";

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(95, 12);
  tft.println("ENTER CODE");

  drawCodeDots();

  // 1-9 grid, rows 0-2
  int layout[9] = {1,2,3,4,5,6,7,8,9};
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int digit = layout[row * 3 + col];
      int x = KP_START_X + col * (KP_BTN_W + KP_GAP_X) + KP_BTN_W / 2;
      int y = KP_START_Y + row * (KP_BTN_H + KP_GAP_Y) + KP_BTN_H / 2;
      char label[2] = { (char)('0' + digit), '\0' };
      keypadBtn[digit].initButton(&tft, x, y, KP_BTN_W, KP_BTN_H, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, label, 2);
      keypadBtn[digit].drawButton();
    }
  }

  // Row 3: Clear (col0), 0 (col1)
  int row3y = KP_START_Y + 3 * (KP_BTN_H + KP_GAP_Y) + KP_BTN_H / 2;

  int clearX = KP_START_X + KP_BTN_W / 2;
  clearBtn.initButton(&tft, clearX, row3y, KP_BTN_W, KP_BTN_H, TFT_WHITE, TFT_RED, TFT_WHITE, "C", 2);
  clearBtn.drawButton();

  int zeroX = KP_START_X + (KP_BTN_W + KP_GAP_X) + KP_BTN_W / 2;
  keypadBtn[0].initButton(&tft, zeroX, row3y, KP_BTN_W, KP_BTN_H, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, "0", 2);
  keypadBtn[0].drawButton();
}

void drawCodeDots() {
  tft.fillRect(90, 30, 140, 20, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(90, 30);

  String display = "";
  for (int i = 0; i < 3; i++) {
    display += (i < (int)enteredCode.length()) ? "* " : "_ ";
  }
  tft.println(display);
}

void handleKeypadTouch(bool pressed) {
  for (int d = 0; d <= 9; d++) {
    keypadBtn[d].press(pressed && keypadBtn[d].contains(touchX, touchY));
    if (keypadBtn[d].justPressed()) {
      keypadBtn[d].drawButton(true);
      onDigitPressed(d);
    }
    if (keypadBtn[d].justReleased()) {
      keypadBtn[d].drawButton(false);
    }
  }

  clearBtn.press(pressed && clearBtn.contains(touchX, touchY));
  if (clearBtn.justPressed()) {
    clearBtn.drawButton(true);
    enteredCode = "";
    drawCodeDots();
  }
  if (clearBtn.justReleased()) {
    clearBtn.drawButton(false);
  }
}

void onDigitPressed(int digit) {
  if ((int)enteredCode.length() >= 3) return;

  enteredCode += String(digit);
  drawCodeDots();

  if ((int)enteredCode.length() == 3) {
    delay(150); // let the 3rd dot render before checking
    checkCode();
  }
}

// Routes to a different action depending on which code was entered
void checkCode() {
  if (enteredCode.equals(CODE_DISPLAY01)) {
    goToDisplay01();
  } else if (enteredCode.equals(CODE_CHIBI)) {
    goToChibi();
  } else if (enteredCode.equals(CODE_WAVE)) {
    goToWave();
  } else {
    onCodeIncorrect();
  }
}

void goToDisplay01() {
  Display01();
  state = DISPLAY01;
}

void goToChibi() {
  DisplayChibi();
  state = IMAGE_SHOWN;
  lastGlitchTime = millis();
}

void goToWave() {
  startWaveAnimation();
  state = WAVE_ACTIVE;
}

void onCodeIncorrect() {
  tft.fillRect(90, 30, 140, 20, TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.setCursor(90, 30);
  tft.println("* * *");
  delay(500);

  enteredCode = "";
  drawCodeDots();
}


void Display01(){
  tft.fillScreen(TFT_CYAN);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(100);
  tft.setCursor(120, 100);
  tft.println("01");
}

void DisplayChibi(){
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 320, 240, chibimiku);
  tft.setSwapBytes(false);
}

void blockGlitch(int w, int h) {
  for (int i = 0; i < 10; i++) {
    int bw = random(15, 60);
    int bh = random(3, 14);
    int bx = random(0, w - bw);
    int by = random(0, h - bh);
    uint16_t noiseColor = random(0, 65536);
    tft.fillRect(bx, by, bw, bh, noiseColor);
  }
}

void rowShiftGlitch(const uint16_t* img, int w, int h) {
  tft.setSwapBytes(true);
  for (int i = 0; i < 12; i++) {
    int y = random(0, h);
    int shift = random(0, 15);
    int rowWidth = w - shift;
    if (rowWidth <= 0) continue;
    tft.pushImage(shift, y, rowWidth, 1, img + (y * w));
  }
  tft.setSwapBytes(false);
}

void PlayGlitchBurst() {
  int glitchFrames = 6;
  for (int frame = 0; frame < glitchFrames; frame++) {
    blockGlitch(320, 240);
    if (random(0, 3) == 0) {
      rowShiftGlitch((const uint16_t*)chibimiku, 320, 240);
    }
    delay(random(25, 70));

    DisplayChibi();
    delay(random(15, 40));
  }

  DisplayChibi();
}

const uint16_t wavePalette[] = {
  TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN,
  TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_PINK
};
const int wavePaletteSize = sizeof(wavePalette) / sizeof(wavePalette[0]);

void startWaveAnimation() {
  tft.fillScreen(TFT_BLACK);
  int screenH = tft.height();
  for (int i = 0; i < NUM_BARS; i++) {
    barHeights[i] = random(10, screenH - 20);
    barTargets[i] = random(10, screenH - 20);
    barColors[i] = wavePalette[random(0, wavePaletteSize)];
  }
}

void updateWaveAnimation() {
  int screenW = tft.width();
  int screenH = tft.height();
  int barWidth = screenW / NUM_BARS;
  int gap = 4;
  int w = barWidth - gap;

  for (int i = 0; i < NUM_BARS; i++) {
    int x = i * barWidth + gap / 2;
    tft.fillRect(x, 0, w, screenH, TFT_BLACK);

    int step = 5;
    if (barHeights[i] < barTargets[i]) {
      barHeights[i] = min(barHeights[i] + step, barTargets[i]);
    } else if (barHeights[i] > barTargets[i]) {
      barHeights[i] = max(barHeights[i] - step, barTargets[i]);
    } else {
      barTargets[i] = random(10, screenH - 20);
    }

    int h = barHeights[i];
    int y = (screenH - h) / 2;

    tft.fillRect(x, y, w, h, barColors[i]);
  }
}
