// Touch calibration for the display. Defaults are typical for the
// ESP32-2432S028 in portrait; run touch_calibrate and paste its output
// over the values below for an accurate mapping on your specific panel.
#pragma once

#define TOUCH_SWAP_XY 0
#define TOUCH_X_MIN 325
#define TOUCH_X_MAX 3795
#define TOUCH_Y_MIN 199
#define TOUCH_Y_MAX 3741

// Touch controller pins on this board (separate SPI bus from the display)
#define XPT_CLK  25
#define XPT_MISO 39
#define XPT_MOSI 32
#define XPT_CS   33
#define XPT_IRQ  36
