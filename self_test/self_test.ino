// Screen, link, frame-rate, and mode check. Upload this instead of the
// main sketch. D5 is the optional button pin documented in the README;
// this sketch runs the checks immediately. D4 is held HIGH. No SD library.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SoftwareSerial.h>

#define TFT_CS 10
#define TFT_DC 9
#define SD_CS_PIN 4
#define UNUSED_TOUCH_CS 8
#define BACKLIGHT_PIN 3
#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
SoftwareSerial radarPort(RADAR_RX_PIN, RADAR_TX_PIN);

static uint16_t gFrames = 0;
static bool gSpeed = false;
static bool gPresence = false;

static void line(int16_t y, bool ok, const __FlashStringHelper *name) {
  tft.setTextSize(2);
  tft.setTextColor(ok ? ILI9341_GREEN : ILI9341_RED);
  tft.setCursor(8, y);
  tft.print(ok ? F("PASS ") : F("FAIL "));
  tft.print(name);
}

void setup() {
  Serial.begin(115200);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(UNUSED_TOUCH_CS, OUTPUT);
  digitalWrite(UNUSED_TOUCH_CS, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(5, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(8, 8);
  tft.print(F("SELF TEST"));

  const uint8_t idHi = tft.readcommand8(0xD3, 2);
  const uint8_t idLo = tft.readcommand8(0xD3, 3);
  const bool tftOk = idHi == 0x93 && idLo == 0x41;
  line(48, tftOk, F("TFT"));

  radarPort.begin(9600);
  const uint32_t t0 = millis();
  bool got = false;
  while ((uint32_t)(millis() - t0) < 5000UL) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      if (c == '$') {
        got = true;
      }
      if (c == '*') {
        gFrames++;
      }
    }
    if (got && (uint32_t)(millis() - t0) > 3000UL) {
      break;
    }
  }
  line(80, got, F("LINK"));

  const uint32_t t1 = millis();
  const uint16_t before = gFrames;
  while ((uint32_t)(millis() - t1) < 3000UL) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      if (c == '*') {
        gFrames++;
      }
    }
  }
  const uint16_t added = (uint16_t)(gFrames - before);
  const bool rateOk = added >= 2;
  line(112, rateOk, F("RATE"));

  bool modeOk = false;
  char win[8];
  uint8_t n = 0;
  const uint32_t t2 = millis();
  while ((uint32_t)(millis() - t2) < 2000UL && !modeOk) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      if ((int)n + 1 < (int)sizeof(win)) {
        win[n++] = c;
        win[n] = 0;
      }
      if (strstr(win, "$DFDMD") != 0) {
        gSpeed = true;
        modeOk = true;
      }
      if (strstr(win, "$DFHPD") != 0) {
        gPresence = true;
        modeOk = true;
      }
    }
  }
  line(144, modeOk, gSpeed ? F("RANGE") : F("PRES"));

  const bool all = tftOk && got && rateOk && modeOk;
  tft.setTextSize(4);
  tft.setCursor(16, 200);
  tft.setTextColor(all ? ILI9341_GREEN : ILI9341_RED);
  tft.print(all ? F("PASS") : F("FAIL"));

  if (Serial.availableForWrite() > 16) {
    Serial.println(all ? F("PASS self-test") : F("FAIL self-test"));
  }
}

void loop() {}
