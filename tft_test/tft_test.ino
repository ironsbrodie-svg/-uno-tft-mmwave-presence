// TFT-only test for the Adafruit 2.8" ILI9341 shield on an UNO.
// No radar, no touch library, no SD library.
// D4 is the shield's microSD chip select and is held HIGH.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS 10
#define TFT_DC 9
#define TFT_ROTATION 0
#define SD_CS_PIN 4
#define UNUSED_TOUCH_CS 8
#define BACKLIGHT_PIN 3

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);

static uint16_t gCount = 0;
static uint32_t gLastMs = 0;

static void holdShieldIdlePins() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(UNUSED_TOUCH_CS, OUTPUT);
  digitalWrite(UNUSED_TOUCH_CS, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  holdShieldIdlePins();
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);

  // ILI9341 register 0xD3 returns ... 0x93, 0x41 when MISO is connected.
  const uint8_t idHi = tft.readcommand8(0xD3, 2);
  const uint8_t idLo = tft.readcommand8(0xD3, 3);
  const bool pass = idHi == 0x93 && idLo == 0x41;

  tft.fillRect(0, 0, 240, 70, pass ? ILI9341_GREEN : ILI9341_RED);
  tft.setTextSize(4);
  tft.setTextColor(pass ? ILI9341_BLACK : ILI9341_WHITE);
  tft.setCursor(pass ? 60 : 48, 20);
  tft.print(pass ? F("PASS") : F("FAIL"));

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 88);
  tft.print(F("CS 10   DC 9"));
  tft.setCursor(12, 116);
  tft.print(F("D4 SD CS held HIGH"));
  tft.setCursor(12, 144);
  tft.print(F("No SD, no touch"));

  // Color bars so a dead row or a swapped ribbon is obvious.
  const uint16_t bars[] = {ILI9341_RED,  ILI9341_YELLOW, ILI9341_GREEN,
                           ILI9341_CYAN, ILI9341_BLUE,   ILI9341_MAGENTA};
  for (uint8_t i = 0; i < 6; i++) {
    tft.fillRect((int16_t)(i * 40), 250, 40, 40, bars[i]);
  }

  tft.fillRect(12, 190, 216, 40, ILI9341_BLACK);
  tft.drawRect(12, 190, 216, 40, ILI9341_WHITE);
  gLastMs = millis();
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 168);
  tft.print(F("ID "));
  if (idHi < 16) {
    tft.print('0');
  }
  tft.print(idHi, HEX);
  if (idLo < 16) {
    tft.print('0');
  }
  tft.print(idLo, HEX);
  if (!pass) {
    tft.setTextSize(1);
    tft.setCursor(12, 232);
    tft.print(F("FAIL can mean MISO is open"));
  }

  if (Serial.availableForWrite() > 24) {
    Serial.print(F("tft "));
    Serial.print(pass ? F("PASS") : F("FAIL"));
    Serial.print(F(" id=0x"));
    if (idHi < 16) {
      Serial.print('0');
    }
    Serial.print(idHi, HEX);
    if (idLo < 16) {
      Serial.print('0');
    }
    Serial.println(idLo, HEX);
    if (!pass) {
      Serial.println(F("FAIL can mean MISO is not wired"));
    }
  }
}

void loop() {
  const uint32_t now = millis();
  if ((uint32_t)(now - gLastMs) < 500UL) {
    return;
  }
  gLastMs = now;
  gCount++;

  // Repaint only the counter box. The rest of the screen stays put.
  tft.fillRect(14, 192, 212, 36, ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(24, 200);
  tft.print(gCount);

  if (Serial.availableForWrite() > 12) {
    Serial.println(gCount);
  }
}
