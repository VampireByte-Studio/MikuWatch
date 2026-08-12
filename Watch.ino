#include <TFT_eSPI.h>   
#include <chibimiku.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
//pre-setup
TFT_eSPI tft = TFT_eSPI();  


#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);



int touchX, touchY;
TFT_eSPI_Button chibi;
TFT_eSPI_Button wave;

//setup
void setup() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(34)); // seed randomness using a floating pin

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
//buttons

  chibi.initButton(&tft, 100, 120, 120, 50, TFT_WHITE, TFT_BLUE, TFT_WHITE, "Chibi", 2);
  chibi.drawButton();
  wave.initButton(&tft, 230, 120, 120, 50, TFT_WHITE, TFT_BLUE, TFT_WHITE, "Wave", 2);
  wave.drawButton();
}

bool imageshown = false;
bool waveActive = false;
unsigned long lastGlitchTime = 0;
const unsigned long glitchInterval = 15000; // 5 seconds

// --- Wave animation config ---
const int NUM_BARS = 16;
int barHeights[NUM_BARS];
int barTargets[NUM_BARS];
uint16_t barColors[NUM_BARS];   // NEW: fixed solid color per bar
unsigned long lastWaveFrame = 0;
const unsigned long waveFrameInterval = 80; // ms between animation frames (a bit slower/calmer)

void loop() {
  bool pressed = false;

  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    touchX = map(p.x, 200, 3700, tft.width(), 0); // FIXED: inverted so left/right match physical touch
    touchY = map(p.y, 240, 3800, 0, tft.height());
    pressed = true;
  }

  if (!imageshown && !waveActive) {
    chibi.press(pressed && chibi.contains(touchX, touchY));

    if (chibi.justPressed()) {
      chibi.drawButton(true);
      DisplayChibi();          // just the clean image now, no glitch on press
      imageshown = true;
      lastGlitchTime = millis(); // start the 5-second countdown from now
      Serial.println("Chibi pressed!");
    }

    if (chibi.justReleased()) {
      chibi.drawButton(false);
    }

    wave.press(pressed && wave.contains(touchX, touchY));

    if (wave.justPressed()) {
      wave.drawButton(true);
      Serial.println("Wave pressed!");
      startWaveAnimation();
    }

    if (wave.justReleased()) {
      wave.drawButton(false);
    }
  } else if (imageshown) {
    // Image is showing - check if it's time for a random glitch burst
    if (millis() - lastGlitchTime >= glitchInterval) {
      PlayGlitchBurst();
      lastGlitchTime = millis(); // reset timer for the next 5-second cycle
    }
  } else if (waveActive) {
    if (millis() - lastWaveFrame >= waveFrameInterval) {
      updateWaveAnimation();
      lastWaveFrame = millis();
    }

    if (pressed) {
      waveActive = false;
      tft.fillScreen(TFT_BLACK);
      chibi.drawButton();
      wave.drawButton();
    }
  }

  delay(10);
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

// Draws random noise-colored blocks over the current screen content
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

// Redraws random horizontal strips of the image shifted sideways
void rowShiftGlitch(const uint16_t* img, int w, int h) {
  tft.setSwapBytes(true);
  for (int i = 0; i < 12; i++) {
    int y = random(0, h);
    int shift = random(0, 15); // positive-only shift keeps things simple & safe
    int rowWidth = w - shift;
    if (rowWidth <= 0) continue;
    tft.pushImage(shift, y, rowWidth, 1, img + (y * w));
  }
  tft.setSwapBytes(false);
}

// Brief glitch burst, then settles back on the clean image
void PlayGlitchBurst() {
  int glitchFrames = 6;
  for (int frame = 0; frame < glitchFrames; frame++) {
    blockGlitch(320, 240);
    if (random(0, 3) == 0) {
      rowShiftGlitch((const uint16_t*)chibimiku, 320, 240);
    }
    delay(random(25, 70));

    DisplayChibi(); // redraw clean between glitch frames for a flicker feel
    delay(random(15, 40));
  }

  DisplayChibi(); // final clean settle
}

// ---------------- Wave (music visualizer) animation ----------------

// A small fixed palette of solid, pleasant colors to assign to bars
const uint16_t wavePalette[] = {
  TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN,
  TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_PINK
};
const int wavePaletteSize = sizeof(wavePalette) / sizeof(wavePalette[0]);

void startWaveAnimation() {
  tft.fillScreen(TFT_BLACK);
  waveActive = true;

  int screenH = tft.height();
  for (int i = 0; i < NUM_BARS; i++) {
    barHeights[i] = random(10, screenH - 20);
    barTargets[i] = random(10, screenH - 20);
    barColors[i] = wavePalette[random(0, wavePaletteSize)]; // assign once, stays fixed
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

    // erase only the old bar area first (clears trailing pixels from previous height)
    tft.fillRect(x, 0, w, screenH, TFT_BLACK);

    // move current height toward its target
    int step = 5;
    if (barHeights[i] < barTargets[i]) {
      barHeights[i] = min(barHeights[i] + step, barTargets[i]);
    } else if (barHeights[i] > barTargets[i]) {
      barHeights[i] = max(barHeights[i] - step, barTargets[i]);
    } else {
      // reached target -> pick a new random target so it keeps bouncing
      barTargets[i] = random(10, screenH - 20);
    }

    int h = barHeights[i];
    int y = (screenH - h) / 2; // bars grow from the vertical center, up and down

    tft.fillRect(x, y, w, h, barColors[i]); // solid, consistent color per bar
  }
}
