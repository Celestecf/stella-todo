// Hello World on the 2.8" display of the Inland ESP32 board (ESP32-2432S028).
// Requires the TFT_eSPI library with the board-specific User_Setup.h installed.
// Portrait orientation: 240 wide x 320 tall.

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

int counter = 0;

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);           // 0 = portrait, USB connector at the bottom
  tft.fillScreen(TFT_BLACK);

  // Big title
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(15, 30);
  tft.println("Hello World!");

  // Smaller subtitle
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(15, 80);
  tft.println("ESP32 display OK");

  // A few colored shapes to confirm color + drawing work
  tft.fillRect(15, 130, 50, 50, TFT_RED);
  tft.fillRect(75, 130, 50, 50, TFT_GREEN);
  tft.fillRect(135, 130, 50, 50, TFT_BLUE);
  tft.fillCircle(120, 230, 25, TFT_YELLOW);

  Serial.println("Display initialized.");
}

void loop() {
  // Live counter at the bottom so you can see the screen updating
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(15, 285);
  tft.print("Uptime: ");
  tft.print(counter++);
  tft.print(" s   ");   // trailing spaces erase leftover digits
  delay(1000);
}
