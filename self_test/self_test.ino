// Screen, link, frame gaps, mode, command wire, and stack margin.
// Upload this instead of the main sketch. It runs the checks immediately.
// D4 is held HIGH. No SD library. D5 is left as an input with a pull-up.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SoftwareSerial.h>
#include <string.h>

#define TFT_CS 10
#define TFT_DC 9
#define SD_CS_PIN 4
#define UNUSED_TOUCH_CS 8
#define BACKLIGHT_PIN 3
#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7

extern "C" uint8_t __bss_end;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
SoftwareSerial radarPort(RADAR_RX_PIN, RADAR_TX_PIN);

static uint16_t gFrames = 0;
static bool gSpeed = false;
static bool gPresence = false;
static uint32_t gGapMin = 0xFFFFFFFFUL;
static uint32_t gGapMax = 0;
static uint32_t gGapSum = 0;
static uint16_t gGapN = 0;

static void paintStack() {
  volatile uint8_t marker;
  uint8_t *p = &__bss_end;
  uint8_t *limit = (uint8_t *)&marker - 32;
  while (p < limit) {
    *p++ = 0xE5;
  }
}

static uint16_t stackMargin() {
  uint8_t *p = &__bss_end;
  while (*p == 0xE5) {
    p++;
  }
  return (uint16_t)(p - &__bss_end);
}

static void line(int16_t y, bool ok, const __FlashStringHelper *name) {
  tft.setTextSize(2);
  tft.setTextColor(ok ? ILI9341_GREEN : ILI9341_RED, ILI9341_BLACK);
  tft.setCursor(8, y);
  tft.print(ok ? F("PASS ") : F("FAIL "));
  tft.print(name);
  const int16_t x = tft.getCursorX();
  if (x < 240) {
    tft.fillRect(x, y, 240 - x, 16, ILI9341_BLACK);
  }
}

static void noteToken(const char *text) {
  if (strstr(text, "$DFDMD") != 0) {
    gSpeed = true;
  }
  if (strstr(text, "$DFHPD") != 0) {
    gPresence = true;
  }
}

static void pushSlide(char *slide, uint8_t *n, uint8_t cap, char c) {
  if ((int)(*n) + 1 < (int)cap) {
    slide[(*n)++] = c;
  } else {
    memmove(slide, slide + 1, cap - 2);
    slide[cap - 2] = c;
    *n = (uint8_t)(cap - 1);
  }
  slide[*n] = 0;
}

static bool seeReply(uint16_t timeoutMs) {
  char tail[16];
  uint8_t n = 0;
  tail[0] = 0;
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      pushSlide(tail, &n, sizeof(tail), c);
      if (strstr(tail, "Done") != 0 || strstr(tail, "Error") != 0 ||
          strstr(tail, "Response") != 0) {
        return true;
      }
    }
    delay(2);
  }
  return false;
}

void setup() {
  paintStack();
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
  tft.setTextWrap(false);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(8, 4);
  tft.print(F("SELF TEST"));

  const uint8_t idHi = tft.readcommand8(0xD3, 2);
  const uint8_t idLo = tft.readcommand8(0xD3, 3);
  const bool tftOk = idHi == 0x93 && idLo == 0x41;
  line(36, tftOk, F("TFT"));

  radarPort.begin(9600);
  char frame[56];
  uint8_t fn = 0;
  bool inFrame = false;
  char slide[16];
  uint8_t sn = 0;
  slide[0] = 0;
  uint8_t printed = 0;
  uint32_t lastStar = 0;
  const uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < 4000UL) {
    while (radarPort.available() > 0) {
      const char c = (char)radarPort.read();
      pushSlide(slide, &sn, sizeof(slide), c);
      noteToken(slide);
      if (c == '$') {
        inFrame = true;
        fn = 0;
        frame[fn++] = c;
        frame[fn] = 0;
      } else if (inFrame) {
        if ((int)fn + 1 < (int)sizeof(frame)) {
          frame[fn++] = c;
          frame[fn] = 0;
        }
        if (c == '*') {
          noteToken(frame);
          gFrames++;
          const uint32_t now = millis();
          if (lastStar != 0) {
            const uint32_t gap = (uint32_t)(now - lastStar);
            if (gap < gGapMin) {
              gGapMin = gap;
            }
            if (gap > gGapMax) {
              gGapMax = gap;
            }
            gGapSum += gap;
            gGapN++;
          }
          lastStar = now;
          if (printed < 3 && Serial.availableForWrite() > (int)fn + 2) {
            Serial.println(frame);
            printed++;
          }
          inFrame = false;
        }
      }
    }
  }
  const bool linkOk = gFrames > 0;
  line(58, linkOk, F("LINK"));
  const bool rateOk = gFrames >= 2;
  line(80, rateOk, F("RATE"));

  const bool modeOk = gSpeed || gPresence;
  const __FlashStringHelper *modeName = F("MODE");
  if (gSpeed && gPresence) {
    modeName = F("BOTH");
  } else if (gSpeed) {
    modeName = F("RANGE");
  } else if (gPresence) {
    modeName = F("PRES");
  }
  line(102, modeOk, modeName);

  const bool gapOk = gGapN > 0;
  line(124, gapOk, F("GAP"));
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(8, 144);
  if (gapOk) {
    const uint32_t mean = gGapSum / gGapN;
    tft.print(F("min "));
    tft.print(gGapMin);
    tft.print(F(" max "));
    tft.print(gGapMax);
    tft.print(F(" avg "));
    tft.print(mean);
    tft.print(F(" ms"));
  } else {
    tft.print(F("no frame gap"));
  }
  if (Serial.availableForWrite() > 40 && gapOk) {
    Serial.print(F("GAP min="));
    Serial.print(gGapMin);
    Serial.print(F(" max="));
    Serial.print(gGapMax);
    Serial.print(F(" mean="));
    Serial.println(gGapSum / gGapN);
  }

  while (radarPort.available() > 0) {
    radarPort.read();
  }
  radarPort.print(F("getRange"));
  bool d7Ok = seeReply(800);
  if (!d7Ok) {
    radarPort.print(F("getRange\r\n"));
    d7Ok = seeReply(800);
  }
  line(160, d7Ok, F("D7"));

  const uint16_t margin = stackMargin();
  const bool stackOk = margin >= 64;
  line(182, stackOk, F("STACK"));
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(8, 202);
  tft.print(F("free "));
  tft.print(margin);
  tft.print(F(" bytes"));
  if (Serial.availableForWrite() > 24) {
    Serial.print(F("STACK free="));
    Serial.println(margin);
  }

  const bool all = tftOk && linkOk && rateOk && modeOk && d7Ok && stackOk;
  tft.setTextSize(3);
  tft.setCursor(8, 240);
  tft.setTextColor(all ? ILI9341_GREEN : ILI9341_RED);
  tft.print(all ? F("PASS") : F("FAIL"));

  if (Serial.availableForWrite() > 24) {
    Serial.println(all ? F("PASS self-test") : F("FAIL self-test"));
    Serial.print(F("mode "));
    Serial.println(gSpeed ? (gPresence ? F("BOTH") : F("RANGE"))
                          : (gPresence ? F("PRES") : F("NONE")));
    Serial.println(d7Ok ? F("D7 PASS") : F("D7 FAIL"));
  }
}

void loop() {}
