// TFT_eSPI User_Setup.h for the Inland ESP32 2.8" display board
// (ESP32-2432S028 / "Cheap Yellow Display", ESP32-WROOM-32E)
//
// The stock file was backed up as User_Setup.h.original

#define USER_SETUP_INFO "ESP32-2432S028 CYD"

// ---- Display driver ----
#define ILI9341_2_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- Display SPI pins (HSPI) ----
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // display reset is tied to the ESP32 reset

// ---- Backlight ----
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// NOTE: the touch controller (XPT2046) on this board is on a *separate* SPI bus
// (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36), so TOUCH_CS is intentionally NOT
// defined here. Use the XPT2046_Touchscreen library for touch instead.

// ---- Fonts ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---- SPI speeds ----
#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
