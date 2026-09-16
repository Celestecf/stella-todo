// Stella's Day - kids' to-do list for the Inland ESP32 2.8" display board
// (ESP32-2432S028). Portrait, 240 x 320.
//
// Fetches today's list from the parent portal (GET /api/today), shows one
// page per section (Before School / After School / Bedtime), and posts
// check-offs back (POST /api/done).
//
// Libraries: TFT_eSPI (with this board's User_Setup.h), XPT2046_Touchscreen,
// ArduinoJson, and the local WiFiSecrets library for WIFI_SSID/WIFI_PASSWORD.

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFiSecrets.h>
#include "icons.h"
#include "touch_config.h"
#include "secrets.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite row = TFT_eSprite(&tft);   // off-screen buffer for one task row
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);

// ---------- Palette (sampled from design/main_screen_mockup.png) ----------
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t C_BG        = rgb(0xFE, 0xF9, 0xF4);  // cream background
const uint16_t C_HEADER    = rgb(0xFD, 0xE0, 0xF4);  // pink header
const uint16_t C_TITLE     = rgb(0x1A, 0x00, 0x47);  // deep purple text
const uint16_t C_MINT      = rgb(0xDC, 0xF7, 0xE5);  // done card
const uint16_t C_LAVENDER  = rgb(0xEE, 0xEE, 0xF8);  // to-do card
const uint16_t C_YELLOW    = rgb(0xFE, 0xEA, 0xAB);  // special / one-off card
const uint16_t C_GREEN     = rgb(0x32, 0x97, 0x5D);  // checked box
const uint16_t C_BOX_LINE  = rgb(0xB9, 0x8C, 0xF6);  // unchecked box border
const uint16_t C_PINK      = rgb(0xFD, 0x69, 0xA5);  // accent (nav buttons)
const uint16_t C_DONE_TEXT = rgb(0x5A, 0x64, 0x70);  // greyed text on done rows
const uint16_t C_ERROR     = rgb(0xE0, 0x40, 0x40);  // offline indicator
const uint16_t C_WHITE     = TFT_WHITE;

// ---------- Layout (pixels) ----------
const int SCREEN_W   = 240;
const int SCREEN_H   = 320;
const int HEADER_H   = 60;
const int ROW_X      = 8;
const int ROW_W      = 224;
const int ROW_H      = 36;
const int ROW_GAP    = 4;
const int ROW_Y0     = 64;
const int ROWS_PER_PAGE = 6;
const int BOX_SIZE   = 26;
const int NAV_Y      = 310;          // centre line of the bottom nav band
const int NAV_TOP    = 300;          // taps below this line are nav taps
const unsigned long REFRESH_MS = 60 * 1000;

// ---------- Schedule (local time) ----------
// POSIX TZ string; must match TIMEZONE on the server. America/New_York:
const char* TZ_INFO = "EST5EDT,M3.2.0,M11.1.0";
// Which page to land on, by time of day (minutes since midnight)
const int AFTER_SCHOOL_MIN = 14 * 60 + 30;   // 2:30 PM -> After School page
const int BEDTIME_MIN      = 19 * 60;        // 7:00 PM -> Bedtime page
// Night mode: backlight off between these times, wake on touch
const int NIGHT_START_MIN  = 20 * 60 + 30;   // 8:30 PM
const int NIGHT_END_MIN    = 6 * 60;         // 6:00 AM
const unsigned long WAKE_MS = 60 * 1000;     // how long a touch keeps it awake at night

// ---------- Data ----------
const int MAX_SECTIONS = 3;
const int MAX_TASKS    = 12;         // per section

struct Task {
  char            id[24];
  char            name[48];
  char            time[16];
  const uint16_t* icon;
  bool            special;
  bool            done;
};

struct Section {
  char title[24];
  Task tasks[MAX_TASKS];
  int  count;
};

Section sections[MAX_SECTIONS];
int  sectionCount = 0;
char dateLabel[24] = "";
bool haveData = false;
bool online = false;
bool dataChanged = false;     // set by fetchToday() when the list differs from last time
unsigned long lastFetch = 0;
String lastBody;

// Time-of-day state
int  lastAutoSection = -1;    // section the schedule last put us on
int  lastClockMin    = -1;    // minute the header clock was last drawn for
bool screenOn        = true;
unsigned long lastTouchMs = 0;

// A "page" is one screen: a section plus an offset into its tasks
// (sections with more than ROWS_PER_PAGE tasks spill onto extra pages).
int curSection = 0;
int curOffset  = 0;

const uint16_t* iconFor(const char* name) {
  for (int i = 0; i < ICON_COUNT; i++) {
    if (!strcmp(name, ICON_TABLE[i].name)) return ICON_TABLE[i].data;
  }
  return icon_heart;
}

// ---------- Network ----------
bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

// ---------- Time ----------
// Minutes since local midnight, or -1 if the clock hasn't synced yet
int minutesNow() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return -1;
  return t.tm_hour * 60 + t.tm_min;
}

int sectionForMinutes(int m) {
  int sec = (m < AFTER_SCHOOL_MIN) ? 0 : (m < BEDTIME_MIN) ? 1 : 2;
  return min(sec, max(sectionCount - 1, 0));
}

bool isNight(int m) {
  return (NIGHT_START_MIN > NIGHT_END_MIN)
       ? (m >= NIGHT_START_MIN || m < NIGHT_END_MIN)   // range wraps midnight
       : (m >= NIGHT_START_MIN && m < NIGHT_END_MIN);
}

void setBacklight(bool on) {
#ifdef TFT_BL
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
#endif
  screenOn = on;
}

// Parse the /api/today JSON into sections[]. Returns false on any problem.
bool parseToday(const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }

  strlcpy(dateLabel, doc["label"] | "", sizeof(dateLabel));
  sectionCount = 0;
  for (JsonObject s : doc["sections"].as<JsonArray>()) {
    if (sectionCount >= MAX_SECTIONS) break;
    Section& sec = sections[sectionCount++];
    strlcpy(sec.title, s["title"] | "", sizeof(sec.title));
    sec.count = 0;
    for (JsonObject t : s["tasks"].as<JsonArray>()) {
      if (sec.count >= MAX_TASKS) break;
      Task& task = sec.tasks[sec.count++];
      strlcpy(task.id,   t["id"]   | "", sizeof(task.id));
      strlcpy(task.name, t["name"] | "", sizeof(task.name));
      strlcpy(task.time, t["time"] | "", sizeof(task.time));
      task.icon    = iconFor(t["icon"] | "heart");
      task.special = t["special"] | false;
      task.done    = t["done"]    | false;
    }
  }
  return sectionCount > 0;
}

bool fetchToday() {
  if (!connectWiFi(10000)) {
    Serial.println("WiFi not connected");
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();   // skip certificate check (home project; see README)
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, String(API_BASE) + "/api/today");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int code = http.GET();
  Serial.printf("GET /api/today -> %d (free heap %u)\n", code, ESP.getFreeHeap());
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    dataChanged = (body != lastBody);
    ok = dataChanged ? parseToday(body) : true;
    if (ok) lastBody = body;
  }
  http.end();
  return ok;
}

bool postDone(const char* taskId, bool done) {
  if (!connectWiFi(5000)) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, String(API_BASE) + "/api/done");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  String body = String("{\"taskId\":\"") + taskId + "\",\"done\":" + (done ? "true" : "false") + "}";
  int code = http.POST(body);
  http.end();
  Serial.printf("POST /api/done %s=%d -> %d\n", taskId, done, code);
  return code == 200;
}

// ---------- Drawing helpers ----------
// Sprites can't pushImage() with a transparent key, so draw icons pixel by pixel
void drawIcon(TFT_eSprite& s, int x, int y, const uint16_t* icon) {
  for (int j = 0; j < ICON_SIZE; j++) {
    for (int i = 0; i < ICON_SIZE; i++) {
      uint16_t c = icon[j * ICON_SIZE + i];
      if (c != ICON_KEY) s.drawPixel(x + i, y + j, c);
    }
  }
}

void drawCheckbox(TFT_eSprite& s, int x, int y, bool done) {
  if (done) {
    s.fillRoundRect(x, y, BOX_SIZE, BOX_SIZE, 7, C_GREEN);
    s.drawWideLine(x + 6,  y + 13, x + 11, y + 18, 3.5, C_WHITE, C_GREEN);
    s.drawWideLine(x + 11, y + 18, x + 20, y + 8,  3.5, C_WHITE, C_GREEN);
  } else {
    s.fillRoundRect(x, y, BOX_SIZE, BOX_SIZE, 7, C_WHITE);
    s.drawRoundRect(x, y, BOX_SIZE, BOX_SIZE, 7, C_BOX_LINE);
    s.drawRoundRect(x + 1, y + 1, BOX_SIZE - 2, BOX_SIZE - 2, 6, C_BOX_LINE);
  }
}

// Try to split text into two lines at a word boundary so both fit in maxW
// using the current font. Returns false if no split works.
bool splitTwoLines(TFT_eSprite& s, const char* text, int maxW, char* l1, char* l2, size_t n) {
  int len = strlen(text);
  // Try the space closest to the middle first, then work outwards
  for (int d = 0; d < len; d++) {
    for (int sign = -1; sign <= 1; sign += 2) {
      int i = len / 2 + sign * d;
      if (i <= 0 || i >= len || text[i] != ' ') continue;
      strlcpy(l1, text, min((size_t)i + 1, n));
      strlcpy(l2, text + i + 1, n);
      if (s.textWidth(l1) <= maxW && s.textWidth(l2) <= maxW) return true;
    }
  }
  return false;
}

// Draw text left-aligned at (x, cy), shrinking the font and finally
// truncating with "..." so it never runs past maxW.
void drawFittedText(TFT_eSprite& s, const char* text, int x, int cy, int maxW) {
  s.setTextDatum(ML_DATUM);
  s.setFreeFont(&FreeSansBold9pt7b);
  if (s.textWidth(text) <= maxW) { s.drawString(text, x, cy); return; }

  s.setTextFont(2);                         // narrower built-in 16px font
  if (s.textWidth(text) <= maxW) { s.drawString(text, x, cy); return; }

  char buf[48];
  strlcpy(buf, text, sizeof(buf));
  int n = strlen(buf);
  while (n > 1) {
    buf[--n] = 0;
    char tmp[52];
    snprintf(tmp, sizeof(tmp), "%s...", buf);
    if (s.textWidth(tmp) <= maxW) { s.drawString(tmp, x, cy); return; }
  }
}

// ---------- Screen pieces ----------
int rowY(int slot) { return ROW_Y0 + slot * (ROW_H + ROW_GAP); }

void drawRowSlot(int slot) {
  int y = rowY(slot);
  Section& sec = sections[curSection];
  int idx = curOffset + slot;

  row.fillSprite(C_BG);
  if (idx < sec.count) {
    const Task& t = sec.tasks[idx];
    uint16_t card = t.done ? C_MINT : (t.special ? C_YELLOW : C_LAVENDER);
    uint16_t text = t.done ? C_DONE_TEXT : C_TITLE;

    row.fillRoundRect(0, 0, ROW_W, ROW_H, 10, card);
    drawIcon(row, 4, (ROW_H - ICON_SIZE) / 2, t.icon);
    row.setTextColor(text, card);

    int textX = 42;
    int textW = ROW_W - BOX_SIZE - 8 - 6 - textX;
    char l1[48], l2[48];
    row.setTextDatum(ML_DATUM);
    row.setFreeFont(&FreeSansBold9pt7b);
    if (t.time[0]) {
      // name on top, time underneath
      drawFittedText(row, t.name, textX, 12, textW);
      row.setFreeFont(&FreeSans9pt7b);
      row.drawString(t.time, textX, 27);
    } else if (row.textWidth(t.name) <= textW) {
      row.drawString(t.name, textX, ROW_H / 2);
    } else if (splitTwoLines(row, t.name, textW, l1, l2, sizeof(l1))) {
      // long name: wrap onto two lines
      row.drawString(l1, textX, 11);
      row.drawString(l2, textX, 26);
    } else {
      drawFittedText(row, t.name, textX, ROW_H / 2, textW);
    }
    drawCheckbox(row, ROW_W - BOX_SIZE - 6, (ROW_H - BOX_SIZE) / 2, t.done);
  }
  row.pushSprite(ROW_X, y);
}

// Date plus clock (once the clock has synced), between the heart and flower
void drawDateLine() {
  char buf[40];
  struct tm t;
  if (getLocalTime(&t, 0)) {
    int h = t.tm_hour % 12; if (h == 0) h = 12;
    snprintf(buf, sizeof(buf), "%s   %d:%02d %s", dateLabel, h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
    lastClockMin = t.tm_hour * 60 + t.tm_min;
  } else {
    strlcpy(buf, dateLabel, sizeof(buf));
  }
  tft.fillRect(44, 34, 152, 20, C_HEADER);
  tft.setTextColor(C_TITLE, C_HEADER);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.drawString(buf, SCREEN_W / 2, 36);
}

void drawHeader() {
  tft.fillRoundRect(0, -16, SCREEN_W, HEADER_H + 16, 16, C_HEADER);
  tft.pushImage(10, 12, ICON_SIZE, ICON_SIZE, icon_heart, ICON_KEY);
  tft.pushImage(198, 24, ICON_SIZE, ICON_SIZE, icon_flower, ICON_KEY);

  tft.setTextColor(C_TITLE, C_HEADER);
  tft.setTextDatum(TC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.drawString("Stella's Day", SCREEN_W / 2, 8);
  drawDateLine();

  // Small red dot in the corner when the last sync failed
  if (!online) tft.fillCircle(SCREEN_W - 8, 8, 3, C_ERROR);
}

void drawArrowButton(int cx, int cy, bool pointRight) {
  tft.fillCircle(cx, cy, 9, C_PINK);
  int d = pointRight ? 1 : -1;
  tft.drawWideLine(cx - 4 * d, cy, cx + 4 * d, cy, 2.5, C_WHITE, C_PINK);
  tft.drawWideLine(cx + 4 * d, cy, cx + 1 * d, cy - 3, 2.5, C_WHITE, C_PINK);
  tft.drawWideLine(cx + 4 * d, cy, cx + 1 * d, cy + 3, 2.5, C_WHITE, C_PINK);
}

void drawNav() {
  tft.fillRect(0, NAV_TOP, SCREEN_W, SCREEN_H - NAV_TOP, C_BG);
  drawArrowButton(22, NAV_Y, false);
  drawArrowButton(SCREEN_W - 22, NAV_Y, true);

  char label[32];
  if (curOffset == 0) strlcpy(label, sections[curSection].title, sizeof(label));
  else snprintf(label, sizeof(label), "%s (%d)", sections[curSection].title, curOffset / ROWS_PER_PAGE + 1);
  tft.setTextColor(C_TITLE, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.drawString(label, SCREEN_W / 2, NAV_Y);
}

void drawEmptyMessage() {
  tft.fillRect(0, ROW_Y0, SCREEN_W, NAV_TOP - ROW_Y0, C_BG);
  tft.pushImage(SCREEN_W / 2 - ICON_SIZE / 2, 150, ICON_SIZE, ICON_SIZE, icon_flower, ICON_KEY);
  tft.setTextColor(C_DONE_TEXT, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.drawString("Nothing to do here!", SCREEN_W / 2, 200);
}

void drawPage() {
  tft.fillScreen(C_BG);
  drawHeader();
  if (sections[curSection].count == 0) {
    drawEmptyMessage();
  } else {
    for (int slot = 0; slot < ROWS_PER_PAGE; slot++) drawRowSlot(slot);
  }
  drawNav();
}

void drawStatusScreen(const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  tft.fillRoundRect(0, -16, SCREEN_W, HEADER_H + 16, 16, C_HEADER);
  tft.pushImage(SCREEN_W / 2 - ICON_SIZE / 2, 14, ICON_SIZE, ICON_SIZE, icon_heart, ICON_KEY);
  tft.setTextColor(C_TITLE, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.drawString(line1, SCREEN_W / 2, 150);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(C_DONE_TEXT, C_BG);
  tft.drawString(line2, SCREEN_W / 2, 180);
}

// ---------- Paging ----------
void nextPage() {
  if (curOffset + ROWS_PER_PAGE < sections[curSection].count) {
    curOffset += ROWS_PER_PAGE;
  } else {
    curSection = (curSection + 1) % sectionCount;
    curOffset = 0;
  }
  drawPage();
}

void prevPage() {
  if (curOffset > 0) {
    curOffset -= ROWS_PER_PAGE;
  } else {
    curSection = (curSection + sectionCount - 1) % sectionCount;
    int n = sections[curSection].count;
    curOffset = n > ROWS_PER_PAGE ? ((n - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE : 0;
  }
  drawPage();
}

// ---------- Touch ----------
bool readTouch(int& x, int& y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  if (p.z < 200) return false;
  long rx = TOUCH_SWAP_XY ? p.y : p.x;
  long ry = TOUCH_SWAP_XY ? p.x : p.y;
  x = constrain(map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  y = constrain(map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
  return true;
}

int slotAt(int x, int y) {
  if (x < ROW_X || x >= ROW_X + ROW_W) return -1;
  for (int slot = 0; slot < ROWS_PER_PAGE; slot++) {
    int top = rowY(slot);
    if (y >= top && y < top + ROW_H) return slot;
  }
  return -1;
}

void handleTap(int x, int y) {
  Serial.printf("tap (%d,%d)\n", x, y);
  lastTouchMs = millis();
  if (!screenOn) {            // first touch at night just wakes the screen
    setBacklight(true);
    return;
  }
  if (!haveData) return;

  if (y >= NAV_TOP - 6) {                       // bottom band: page arrows
    if (x < SCREEN_W / 3) prevPage();
    else if (x > SCREEN_W * 2 / 3) nextPage();
    return;
  }

  int slot = slotAt(x, y);
  if (slot < 0) return;
  int idx = curOffset + slot;
  Section& sec = sections[curSection];
  if (idx >= sec.count) return;

  Task& t = sec.tasks[idx];
  t.done = !t.done;
  drawRowSlot(slot);                            // instant feedback
  if (!postDone(t.id, t.done)) {
    online = false;
    drawHeader();
  }
}

// ---------- Sync ----------
void refresh() {
  bool ok = fetchToday();
  if (ok) {
    bool wasOffline = !online || !haveData;
    haveData = true;
    online = true;
    if (dataChanged || wasOffline) {
      Serial.printf("Drawing page: %d sections, \"%s\"\n", sectionCount, dateLabel);
      // Keep the current page valid if the list shrank
      if (curSection >= sectionCount) { curSection = 0; curOffset = 0; }
      if (curOffset >= sections[curSection].count) curOffset = 0;
      drawPage();
    }
  } else {
    online = false;
    if (haveData) drawHeader();
    else drawStatusScreen("Can't reach the list", "Check WiFi, retrying...");
  }
  lastFetch = millis();
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);

  touchSpi.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  ts.begin(touchSpi);
  ts.setRotation(0);

  row.setColorDepth(16);
  if (!row.createSprite(ROW_W, ROW_H)) Serial.println("Row sprite allocation failed!");

  drawStatusScreen("Connecting...", WIFI_SSID);
  if (connectWiFi(20000)) {
    Serial.printf("WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());
    configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");   // clock syncs in the background
    drawStatusScreen("Loading...", "Getting today's list");
    Serial.println("Drew Loading screen");
  } else {
    Serial.println("WiFi connect failed");
  }
  refresh();
}

bool wasTouched = false;

void loop() {
  int x, y;
  bool touching = readTouch(x, y);
  if (touching && !wasTouched) handleTap(x, y);
  wasTouched = touching;

  if (millis() - lastFetch > REFRESH_MS) refresh();

  int m = minutesNow();
  if (m >= 0) {
    // Land on the right page when the time of day crosses a boundary
    int autoSec = sectionForMinutes(m);
    if (autoSec != lastAutoSection) {
      lastAutoSection = autoSec;
      curSection = autoSec;
      curOffset = 0;
      if (haveData) drawPage();
    }

    // Night mode: backlight off after WAKE_MS without a touch
    bool night = isNight(m);
    if (night && screenOn && millis() - lastTouchMs > WAKE_MS) setBacklight(false);
    if (!night && !screenOn) setBacklight(true);

    // Tick the header clock once a minute
    if (m != lastClockMin && haveData && screenOn) drawDateLine();
  }
  delay(10);
}
