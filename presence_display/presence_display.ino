// Handheld presence display for an Arduino UNO R3, a 2.8" ILI9341 shield,
// and a DFRobot C4001 UART radar.
//
// Edit the settings in this block.

// 25 m module (SEN0609) is the default. SEN0610 is the 12 m module.
#define C4001_MODEL_12M 12
#define C4001_MODEL_25M 25
#define C4001_MODEL C4001_MODEL_25M

#if C4001_MODEL == C4001_MODEL_12M
#define MODEL_MAX_CM 1200
#else
#define MODEL_MAX_CM 2500
#endif

#define PRESET_INDOOR 0
#define PRESET_OUTDOOR 1
#define RADAR_PRESET PRESET_INDOOR

#if RADAR_PRESET == PRESET_OUTDOOR
#define RANGE_MIN_CM 60
#define RANGE_MAX_CM 1500
#define RANGE_THRESHOLD 12
#else
#define RANGE_MIN_CM 60
#define RANGE_MAX_CM 600
#define RANGE_THRESHOLD 22
#endif

#if RANGE_MAX_CM > MODEL_MAX_CM
#undef RANGE_MAX_CM
#define RANGE_MAX_CM MODEL_MAX_CM
#endif

static_assert(RANGE_MIN_CM >= 30, "RANGE_MIN_CM is below the sensor minimum");
static_assert(RANGE_MAX_CM <= MODEL_MAX_CM, "RANGE_MAX_CM exceeds this model");
static_assert(RANGE_MIN_CM < RANGE_MAX_CM, "RANGE_MIN_CM must be below RANGE_MAX_CM");

#define RADAR_MODE_RANGING 1
#define RADAR_MODE_PRESENCE 0
#define RADAR_MODE RADAR_MODE_RANGING

#define FRETTING_DETECTION 1

// Presence mode only. Sensitivity 0..9, higher is more sensitive.
// Trigger delay is 0.01 s units (20 = 0.2 s). Keep is 0.5 s units (8 = 4 s).
#define TRIG_SENSITIVITY 4
#define KEEP_SENSITIVITY 4
#define TRIG_DELAY_UNITS 20
#define KEEP_DELAY_UNITS 8

// HOLD keeps PRESENT after the radar drops out.
// ENTER is the hit weight: score += 2 on a hit, -= 1 on a miss, latch at
// ENTER * 2. A single miss no longer clears the score.
// SMOOTH_DEN 4 keeps 3/4 of the previous range.
// JUMP is the centimeter step that needs a second agreeing sample.
#define PRESENCE_HOLD_MS 1600
#define PRESENCE_ENTER_READS 2
#define DISTANCE_SMOOTH_DEN 4
#define DISTANCE_JUMP_CM 150

// No complete frame for this long means the link is down, not an empty room.
// 5 s covers a 1 s presence-mode report with margin. The datasheet names
// setUartOutput for the period, but the argument list was not in the text
// we could verify, so the timeout is the knob instead of a guessed command.
#define LINK_TIMEOUT_MS 5000

// Directional drawing (future radar on a Mega). Leave 0 on the UNO.
#ifndef CONE_DIRECTIONAL
#define CONE_DIRECTIONAL 0
#endif

#ifndef RADAR_LINK_SOFT
#define RADAR_LINK_SOFT 0
#define RADAR_LINK_SERIAL1 1
#define RADAR_LINK_SERIAL2 2
#define RADAR_LINK_SERIAL3 3
#endif
#ifndef RADAR_LINK
#define RADAR_LINK RADAR_LINK_SOFT
#endif

#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7
#define C4001_BAUD 9600

#define TFT_CS 10
#define TFT_DC 9
#define TFT_ROTATION 0
#define SD_CS_PIN 4
#define UNUSED_TOUCH_CS 8
#define BACKLIGHT_PIN 3

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "c4001_sensor.h"
#include "target_smoother.h"
#include "ui.h"

#if RADAR_PORT_IS_SOFT
C4001Sensor radar(RANGE_MIN_CM, RANGE_MAX_CM, RANGE_THRESHOLD,
                  RADAR_MODE == RADAR_MODE_RANGING, FRETTING_DETECTION != 0,
                  TRIG_SENSITIVITY, KEEP_SENSITIVITY, TRIG_DELAY_UNITS,
                  KEEP_DELAY_UNITS, RADAR_RX_PIN, RADAR_TX_PIN);
#else
C4001Sensor radar(RANGE_MIN_CM, RANGE_MAX_CM, RANGE_THRESHOLD,
                  RADAR_MODE == RADAR_MODE_RANGING, FRETTING_DETECTION != 0,
                  TRIG_SENSITIVITY, KEEP_SENSITIVITY, TRIG_DELAY_UNITS,
                  KEEP_DELAY_UNITS);
#endif

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
TargetSmoother smoother(PRESENCE_HOLD_MS, PRESENCE_ENTER_READS,
                        DISTANCE_SMOOTH_DEN, DISTANCE_JUMP_CM);

static bool gReady = false;
static bool gConfigTried = false;
static uint32_t gNextTryMs = 0;
static uint32_t gLastPollMs = 0;
static bool gLastPresent = false;
static int16_t gLastDist = -1;

static void holdShieldIdlePins() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(UNUSED_TOUCH_CS, OUTPUT);
  digitalWrite(UNUSED_TOUCH_CS, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
}

static void logIfRoom(const __FlashStringHelper *line) {
  if (Serial.availableForWrite() > 24) {
    Serial.println(line);
  }
}

static ViewCaps capsFor(const PresenceSensor &sensor) {
  ViewCaps caps;
  caps.providesRange = sensor.providesRange();
  caps.maxRangeCm = sensor.maxRangeCm();
  caps.beamDeg = sensor.horizontalBeamDeg();
  return caps;
}

static void showSearch() {
  uiStartup(tft, BANNER_SEARCH, F("Waiting"), F("for radar"));
}

static void showNoSensor() {
#if RADAR_LINK == RADAR_LINK_SOFT
  uiStartup(tft, BANNER_NOSENSOR, F("TX to D6"), F("RX to D7"));
#elif RADAR_LINK == RADAR_LINK_SERIAL1
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 19"), F("RX to pin 18"));
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 17"), F("RX to pin 16"));
#else
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 15"), F("RX to pin 14"));
#endif
}

static void showNoRx() {
#if RADAR_LINK == RADAR_LINK_SOFT
  uiStartup(tft, BANNER_NORX, F("Check D7"), F("UNO to sensor RX"));
#elif RADAR_LINK == RADAR_LINK_SERIAL1
  uiStartup(tft, BANNER_NORX, F("Check pin 18"), F("to sensor RX"));
#else
  uiStartup(tft, BANNER_NORX, F("Check TX wire"), F("board to sensor RX"));
#endif
}

static void tryBringUp() {
  if (sBanner != BANNER_NOSENSOR && sBanner != BANNER_NORX &&
      sBanner != BANNER_CFGFAIL) {
    showSearch();
  }
  if (radar.lastFrameMs() == 0 && !radar.probe()) {
    showNoSensor();
    logIfRoom(F("no radar frame"));
    gReady = false;
    return;
  }
  if (!gConfigTried) {
    gConfigTried = true;
    if (sBanner != BANNER_NORX && sBanner != BANNER_CFGFAIL) {
      uiStartup(tft, BANNER_CONFIG, F("Checking"), F("radar settings"));
    }
    const bool running = radar.configure();
    if (!running) {
      if (radar.rxTimedOut() && radar.lastFrameMs() == 0) {
        showNoRx();
        logIfRoom(F("sensor RX timeout"));
        gReady = false;
        return;
      }
      logIfRoom(F("config unverified"));
    }
  }
  smoother.reset();
  gReady = true;
  gLastPresent = false;
  gLastDist = -1;
  uiArmRunning(tft);
  if (radar.configUnverified()) {
    uiWarn(tft, radar.rxTimedOut() ? F("NO RX, LIVE") : F("CFG UNVERIFIED"));
    logIfRoom(radar.configError() ? radar.configError() : F("config unverified"));
  } else {
    logIfRoom(F("radar ready"));
  }
}

static void logFix(const TargetFix &fix) {
  const int16_t dist = fix.hasDistance ? (int16_t)fix.distanceCm : (int16_t)-1;
  if (fix.present == gLastPresent && dist == gLastDist) {
    return;
  }
  if (Serial.availableForWrite() < 32) {
    return;
  }
  gLastPresent = fix.present;
  gLastDist = dist;
  Serial.print(fix.present ? F("PRESENT") : F("CLEAR"));
  if (fix.hasDistance) {
    Serial.print(' ');
    Serial.print(fix.distanceCm);
    Serial.print(F("cm"));
  }
  if (fix.hasSpeed) {
    Serial.print(' ');
    Serial.print(fix.speedCms);
    Serial.print(F("cm/s"));
  }
  if (fix.hasEnergy) {
    Serial.print(F(" e="));
    Serial.print((unsigned)fix.energy);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  holdShieldIdlePins();
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  uiBegin(tft);
  showSearch();
  radar.begin();
  tryBringUp();
  gNextTryMs = millis() + 1200;
}

void loop() {
  const uint32_t now = millis();
  if (!gReady) {
    if ((int32_t)(now - gNextTryMs) < 0) {
      return;
    }
    tryBringUp();
    gNextTryMs = millis() + 1200;
    return;
  }
  if (radar.lastFrameMs() == 0 ||
      (uint32_t)(now - radar.lastFrameMs()) > LINK_TIMEOUT_MS) {
    gReady = false;
    smoother.reset();
    showNoSensor();
    logIfRoom(F("radar link lost"));
    gNextTryMs = millis() + 1200;
    return;
  }
  if ((uint32_t)(now - gLastPollMs) < 40UL) {
    return;
  }
  gLastPollMs = now;

  TargetFix raw;
  TargetFix shown;
  if (!radar.read(raw)) {
    return;
  }
  smoother.apply(raw, shown);
  uiRefresh(tft, shown, capsFor(radar));
  logFix(shown);
}
