// Combined TFT + C4001 bring-up. Does not change saved radar settings
// beyond a stop/start round trip. D4 is held HIGH. No SD library.

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

static bool waitToken(const char *token, uint16_t timeoutMs) {
  char tail[12];
  uint8_t n = 0;
  tail[0] = 0;
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      if ((int)n + 1 < (int)sizeof(tail)) {
        tail[n++] = c;
      } else {
        memmove(tail, tail + 1, sizeof(tail) - 2);
        tail[sizeof(tail) - 2] = c;
        n = sizeof(tail) - 1;
      }
      tail[n] = 0;
      if (strstr(tail, token) != 0 || strstr(tail, "Error") != 0) {
        return true;
      }
    }
    delay(2);
  }
  return false;
}

static bool seeFrame(uint16_t timeoutMs) {
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    while (radarPort.available() > 0) {
      if ((char)radarPort.read() == '$') {
        return true;
      }
    }
    delay(5);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(UNUSED_TOUCH_CS, OUTPUT);
  digitalWrite(UNUSED_TOUCH_CS, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 16);
  tft.print(F("BRINGUP"));

  const uint8_t idHi = tft.readcommand8(0xD3, 2);
  const uint8_t idLo = tft.readcommand8(0xD3, 3);
  const bool tftOk = idHi == 0x93 && idLo == 0x41;
  tft.setTextSize(3);
  tft.setCursor(12, 70);
  tft.setTextColor(tftOk ? ILI9341_GREEN : ILI9341_RED);
  tft.print(tftOk ? F("TFT PASS") : F("TFT FAIL"));

  radarPort.begin(9600);
  bool txOk = seeFrame(2000);
  if (!txOk) {
    radarPort.print(F("sensorStart"));
    txOk = seeFrame(1000);
  }
  bool rxOk = false;
  if (txOk) {
    while (radarPort.available() > 0) {
      radarPort.read();
    }
    radarPort.print(F("sensorStop"));
    rxOk = waitToken("Done", 2000);
    radarPort.print(F("sensorStart"));
    waitToken("Done", 1000);
  }

  tft.setCursor(12, 120);
  tft.setTextColor(txOk ? ILI9341_GREEN : ILI9341_RED);
  tft.print(txOk ? F("TX PASS") : F("TX FAIL"));
  tft.setCursor(12, 160);
  tft.setTextColor(rxOk ? ILI9341_GREEN : ILI9341_RED);
  tft.print(rxOk ? F("RX PASS") : F("RX FAIL"));

  const bool pass = tftOk && txOk && rxOk;
  tft.setTextSize(4);
  tft.setCursor(24, 230);
  tft.setTextColor(pass ? ILI9341_GREEN : ILI9341_RED);
  tft.print(pass ? F("PASS") : F("FAIL"));

  if (Serial.availableForWrite() > 16) {
    Serial.println(pass ? F("PASS bringup") : F("FAIL bringup"));
  }
}

void loop() {}
