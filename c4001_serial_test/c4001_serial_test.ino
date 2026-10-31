// Raw C4001 link test. No smoothing and no saved settings.
// Monitor at 115200. The sketch does not wait for the monitor.
// Keep RADAR_LINK identical to presence_display.ino.
// Uno: sensor TX -> D6, sensor RX -> D7. D4 stays HIGH (shield SD select).

#define RADAR_LINK_SOFT 0
#define RADAR_LINK_SERIAL1 1
#define RADAR_LINK_SERIAL2 2
#define RADAR_LINK_SERIAL3 3
#define RADAR_LINK RADAR_LINK_SOFT

#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7
#define C4001_BAUD 9600
#define SD_CS_PIN 4
#define LINK_TIMEOUT_MS 2000

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#if RADAR_LINK != RADAR_LINK_SOFT
#error "This board (" ARDUINO_BOARD ") needs RADAR_LINK_SOFT."
#endif
#include <SoftwareSerial.h>
SoftwareSerial radarPort(RADAR_RX_PIN, RADAR_TX_PIN);
#define RADAR_PORT radarPort
#else
#if RADAR_LINK == RADAR_LINK_SOFT
#error "This board (" ARDUINO_BOARD ") needs RADAR_LINK_SERIAL1."
#endif
#if RADAR_LINK == RADAR_LINK_SERIAL1
#define RADAR_PORT Serial1
#elif RADAR_LINK == RADAR_LINK_SERIAL2
#define RADAR_PORT Serial2
#else
#define RADAR_PORT Serial3
#endif
#endif

static char gBuf[80];
static uint8_t gLen = 0;
static uint16_t gFrames = 0;
static uint32_t gLastFrameMs = 0;
static bool gTxOk = false;
static bool gRxOk = false;

static void drain() {
  while (RADAR_PORT.available() > 0) {
    const char c = (char)RADAR_PORT.read();
    if (gLen + 1 >= sizeof(gBuf)) {
      memmove(gBuf, gBuf + 24, gLen - 24);
      gLen = (uint8_t)(gLen - 24);
    }
    gBuf[gLen++] = c;
    gBuf[gLen] = 0;
    if (c == '*') {
      gFrames++;
      gLastFrameMs = millis();
      gTxOk = true;
    }
  }
}

static bool waitAck(uint16_t timeoutMs) {
  char tail[12];
  uint8_t n = 0;
  tail[0] = 0;
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    while (RADAR_PORT.available() > 0) {
      const char c = (char)RADAR_PORT.read();
      if (n + 1 < sizeof(tail)) {
        tail[n++] = c;
      } else {
        memmove(tail, tail + 1, sizeof(tail) - 2);
        tail[sizeof(tail) - 2] = c;
        n = sizeof(tail) - 1;
      }
      tail[n] = 0;
      if (strstr(tail, "Done") != 0 || strstr(tail, "Error") != 0) {
        return true;
      }
    }
    delay(2);
  }
  return false;
}

static bool roundTripRx() {
  while (RADAR_PORT.available() > 0) {
    RADAR_PORT.read();
  }
  RADAR_PORT.print(F("sensorStop"));
  if (!waitAck(2000)) {
    return false;
  }
  RADAR_PORT.print(F("sensorStart"));
  waitAck(1000);
  return true;
}

static void printResult() {
  if (Serial.availableForWrite() < 24) {
    return;
  }
  if (gTxOk && gRxOk) {
    Serial.println(F("PASS tx+rx"));
  } else if (!gTxOk) {
    Serial.println(F("FAIL tx  no frames"));
  } else {
    Serial.println(F("FAIL rx  check sensor RX wire"));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  RADAR_PORT.begin(C4001_BAUD);

  if (Serial.availableForWrite() > 40) {
    Serial.println(F("C4001 link test 115200"));
#if RADAR_LINK == RADAR_LINK_SOFT
    Serial.println(F("TX->D6 RX->D7  D4 held high"));
#elif RADAR_LINK == RADAR_LINK_SERIAL1
    Serial.println(F("TX->19 RX->18"));
#endif
  }

  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < 2000UL && !gTxOk) {
    drain();
    delay(5);
  }
  gRxOk = roundTripRx();
  gLastFrameMs = millis();
  printResult();
}

void loop() {
  static uint32_t lastBeat = 0;
  drain();
  const uint32_t now = millis();
  if (gLastFrameMs != 0 && (uint32_t)(now - gLastFrameMs) > LINK_TIMEOUT_MS) {
    if (Serial.availableForWrite() > 16) {
      Serial.println(F("FAIL link"));
    }
    gTxOk = false;
    gLastFrameMs = now;
  }
  if ((uint32_t)(now - lastBeat) < 1000UL) {
    return;
  }
  lastBeat = now;
  if (Serial.availableForWrite() < 20) {
    return;
  }
  Serial.print(F("hb frames="));
  Serial.println(gFrames);
}
