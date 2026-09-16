// Touch calibration for the Inland ESP32 2.8" board (ESP32-2432S028).
// Portrait (rotation 0). Requires TFT_eSPI + XPT2046_Touchscreen.
//
// 1. Tap the four crosshairs as accurately as you can (a fingernail helps).
// 2. Copy the #define block it prints into stella_todo/touch_config.h.
// 3. Then it enters test mode: draw on the screen to check the mapping.

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Touch controller pins on this board (separate SPI bus from the display)
#define XPT_CLK  25
#define XPT_MISO 39
#define XPT_MOSI 32
#define XPT_CS   33
#define XPT_IRQ  36

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);

const int W = 240, H = 320, INSET = 20;
const int targets[4][2] = { {INSET, INSET}, {W - INSET, INSET},
                            {INSET, H - INSET}, {W - INSET, H - INSET} };
int rawX[4], rawY[4];

// Calibration result, used by test mode in loop()
bool gSwap; long gXMin, gXMax, gYMin, gYMax;

void crosshair(int x, int y, uint16_t c) {
  tft.drawLine(x - 12, y, x + 12, y, c);
  tft.drawLine(x, y - 12, x, y + 12, c);
  tft.drawCircle(x, y, 7, c);
}

void waitRelease() {
  while (ts.touched()) delay(10);
  delay(150);
}

// Average several raw readings while the finger is down
void readRawAveraged(int& x, int& y) {
  long sx = 0, sy = 0; int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 250) {
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      if (p.z > 200) { sx += p.x; sy += p.y; n++; }
    }
    delay(5);
  }
  x = n ? sx / n : 0;
  y = n ? sy / n : 0;
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  touchSpi.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  ts.begin(touchSpi);
  ts.setRotation(0);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch calibration", W / 2, H / 2 - 20, 4);
  tft.drawString("Tap each crosshair", W / 2, H / 2 + 10, 2);

  for (int i = 0; i < 4; i++) {
    crosshair(targets[i][0], targets[i][1], TFT_RED);
    waitRelease();
    while (!ts.touched()) delay(10);
    readRawAveraged(rawX[i], rawY[i]);
    crosshair(targets[i][0], targets[i][1], TFT_GREEN);
    Serial.printf("Target %d at (%d,%d) -> raw (%d,%d)\n",
                  i, targets[i][0], targets[i][1], rawX[i], rawY[i]);
    delay(300);
  }

  // Decide whether raw X tracks screen X or screen Y by seeing which raw
  // axis changes more between the left and right targets.
  long dxRaw = ((long)rawX[1] + rawX[3]) - ((long)rawX[0] + rawX[2]);
  long dyRaw = ((long)rawY[1] + rawY[3]) - ((long)rawY[0] + rawY[2]);
  bool swapXY = abs(dyRaw) > abs(dxRaw);

  // Pick the raw axis that corresponds to each screen axis
  long left, right, top, bottom;
  if (!swapXY) {
    left   = (rawX[0] + rawX[2]) / 2;  right  = (rawX[1] + rawX[3]) / 2;
    top    = (rawY[0] + rawY[1]) / 2;  bottom = (rawY[2] + rawY[3]) / 2;
  } else {
    left   = (rawY[0] + rawY[2]) / 2;  right  = (rawY[1] + rawY[3]) / 2;
    top    = (rawX[0] + rawX[1]) / 2;  bottom = (rawX[2] + rawX[3]) / 2;
  }

  // Extrapolate from the inset targets out to the true screen edges
  float kx = (float)INSET / (W - 2 * INSET);
  float ky = (float)INSET / (H - 2 * INSET);
  long xMin = left - (right - left) * kx,  xMax = right  + (right - left) * kx;
  long yMin = top  - (bottom - top) * ky,  yMax = bottom + (bottom - top) * ky;

  Serial.println();
  Serial.println("// ---- paste into stella_todo/touch_config.h ----");
  Serial.printf("#define TOUCH_SWAP_XY %d\n", swapXY ? 1 : 0);
  Serial.printf("#define TOUCH_X_MIN %ld\n", xMin);
  Serial.printf("#define TOUCH_X_MAX %ld\n", xMax);
  Serial.printf("#define TOUCH_Y_MIN %ld\n", yMin);
  Serial.printf("#define TOUCH_Y_MAX %ld\n", yMax);
  Serial.println("// -----------------------------------------------");

  // Store for test mode
  gSwap = swapXY; gXMin = xMin; gXMax = xMax; gYMin = yMin; gYMax = yMax;

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Done! Values sent to", W / 2, 30, 2);
  tft.drawString("Serial Monitor.", W / 2, 50, 2);
  tft.drawString("Now draw here to test.", W / 2, 80, 2);
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z > 200) {
      long rx = gSwap ? p.y : p.x;
      long ry = gSwap ? p.x : p.y;
      int x = constrain(map(rx, gXMin, gXMax, 0, W - 1), 0, W - 1);
      int y = constrain(map(ry, gYMin, gYMax, 0, H - 1), 0, H - 1);
      tft.fillCircle(x, y, 2, TFT_CYAN);
    }
  }
  delay(5);
}
