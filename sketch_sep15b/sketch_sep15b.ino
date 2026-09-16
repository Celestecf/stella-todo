// Hello World connection test for the Inland ESP32 2.8" display board
// (ESP32-WROOM-32E, a.k.a. ESP32-2432S028 / "Cheap Yellow Display").
//
// No libraries needed. Prints to the Serial Monitor at 115200 baud
// and cycles the onboard RGB LED (red -> green -> blue) once per second.

// Onboard RGB LED pins on this board (active LOW: LOW = on, HIGH = off)
const int LED_R = 4;
const int LED_G = 16;
const int LED_B = 17;

int counter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);  // give the Serial Monitor a moment to connect

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  Serial.println();
  Serial.println("=================================");
  Serial.println("Hello World from the ESP32!");
  Serial.print("Chip model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("CPU freq:   ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Flash size: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println(" MB");
  Serial.print("Free heap:  ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println("=================================");
}

void loop() {
  counter++;
  Serial.print("Still alive... loop #");
  Serial.println(counter);

  // Cycle the RGB LED so you can also see it working without the monitor
  digitalWrite(LED_R, LOW);  delay(333);  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);  delay(333);  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);  delay(334);  digitalWrite(LED_B, HIGH);
}
