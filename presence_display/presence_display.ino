// human-presence-detector
// Handheld presence display for an Arduino UNO R3, an Adafruit 2.8" ILI9341
// TFT shield (product 1651 style), and a DFRobot C4001 UART radar.
//
// Edit the settings in this block. They are the handheld, indoor/outdoor,
// and serial-port options. The rest of the sketch reads them.

// ----- Indoor / outdoor preset ---------------------------------------------
// PRESET_INDOOR:  0.6 m to 6 m, threshold 22.
//   Shorter gate. Walls and the next room show up less often.
// PRESET_OUTDOOR: 0.6 m to 15 m, threshold 12.
//   Longer gate. Raise the threshold if rain or moving foliage chatters.
// Ranging mode on a 25 m module accepts up to about 2500 cm. A unit in
// your hand gets noisy well before that. Edit the numbers in the active
// block for a custom gate. A 12 m module (SEN0610) should stay at or
// below 1200 cm.
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

// ----- Radar mode -----------------------------------------------------------
// RANGING shows distance, speed, and energy (default, useful in the hand).
// PRESENCE is the "someone is here, even if still" mode. It has no range,
// speed, or energy. The cone then means "somewhere in the beam".
#define RADAR_MODE_RANGING 1
#define RADAR_MODE_PRESENCE 0
#define RADAR_MODE RADAR_MODE_RANGING

// 1: micro-motion on, so a still person can register when YOU are still.
// 0: fewer false hits while you walk; a motionless person is easier to miss.
#define FRETTING_DETECTION 1

// Used only when RADAR_MODE is PRESENCE.
// Sensitivity is 0..9. Higher is more sensitive (datasheet default is 7).
// Handheld starts lower so your own sway triggers less often.
// Trigger delay unit is 0.01 s (20 = 0.2 s). Keep unit is 0.5 s (8 = 4 s).
#define TRIG_SENSITIVITY 4
#define KEEP_SENSITIVITY 4
#define TRIG_DELAY_UNITS 20
#define KEEP_DELAY_UNITS 8

// ----- Handheld smoothing ---------------------------------------------------
// The C4001 is meant to be mounted still. Walking makes the room look like
// it is moving. These keep the big digits readable.
// HOLD: how long PRESENT stays after the radar drops out.
// ENTER: consecutive hits required before PRESENT appears.
// SMOOTH_DEN: 4 means each new sample is one quarter of the shown range.
// JUMP: a single range step larger than this (meters) is ignored until a
// second sample agrees.
#define PRESENCE_HOLD_MS 1600
#define PRESENCE_ENTER_READS 2
#define DISTANCE_SMOOTH_DEN 4
#define DISTANCE_JUMP_M 1.5f

// ----- Serial port ----------------------------------------------------------
// Uno: RADAR_LINK_SOFT. Sensor TX -> RADAR_RX_PIN (D6), sensor RX -> D7.
//      D4 is the shield's microSD chip select. Do not move the radar there.
// Mega: change the next line to RADAR_LINK_SERIAL1 (module TX -> pin 19,
//       module RX -> pin 18). Leave Serial2 free for a future LD2450.
#define RADAR_LINK_SOFT 0
#define RADAR_LINK_SERIAL1 1
#define RADAR_LINK_SERIAL2 2
#define RADAR_LINK_SERIAL3 3
#define RADAR_LINK RADAR_LINK_SOFT

#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7
#define C4001_BAUD 9600

// ----- Shield pins (same on Uno and Mega for this shield) ------------------
#define TFT_CS 10
#define TFT_DC 9
#define TFT_ROTATION 0  // use 2 if the picture is upside down
#define SD_CS_PIN 4     // microSD chip select; held HIGH. No SD library.
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
                        DISTANCE_SMOOTH_DEN, DISTANCE_JUMP_M);

static bool gReady = false;
static uint32_t gLastTryMs = 0;
static uint32_t gLastPollMs = 0;
static bool gLastPresent = false;
static int16_t gLastDistQ = -32000;

static void holdShieldIdlePins() {
  // The shield wires D4 to the microSD chip select even when the slot is
  // empty and even though this project never talks to a card. Hold it high
  // so the slot cannot answer SPI traffic meant for the TFT.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  // Older resistive shields use D8 as the touch controller's chip select.
  // This build does not read touch. Holding D8 high keeps that chip quiet.
  pinMode(UNUSED_TOUCH_CS, OUTPUT);
  digitalWrite(UNUSED_TOUCH_CS, HIGH);
  // If the backlight jumper to D3 is open, this does nothing visible.
  // If it is closed, high keeps the backlight on for outdoor reading.
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
}

static void logIfRoom(const __FlashStringHelper *line) {
  if (Serial.availableForWrite() > 20) {
    Serial.println(line);
  }
}

static ViewCaps capsFor(const PresenceSensor &sensor) {
  ViewCaps caps;
  caps.providesRange = sensor.providesRange();
  caps.providesAngle = sensor.providesAngle();
  caps.maxRangeCm = sensor.maxRangeCm();
  caps.beamDeg = sensor.horizontalBeamDeg();
  return caps;
}

static void showSearch() {
#if RADAR_LINK == RADAR_LINK_SERIAL1
  uiStartup(tft, BANNER_SEARCH, F("TX to pin 19"), F("RX to pin 18"));
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  uiStartup(tft, BANNER_SEARCH, F("TX to pin 17"), F("RX to pin 16"));
#elif RADAR_LINK == RADAR_LINK_SERIAL3
  uiStartup(tft, BANNER_SEARCH, F("TX to pin 15"), F("RX to pin 14"));
#else
  uiStartup(tft, BANNER_SEARCH, F("TX to D6"), F("RX to D7"));
#endif
}

static void showNoSensor() {
#if RADAR_LINK == RADAR_LINK_SERIAL1
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 19"), F("RX to pin 18"));
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 17"), F("RX to pin 16"));
#elif RADAR_LINK == RADAR_LINK_SERIAL3
  uiStartup(tft, BANNER_NOSENSOR, F("TX to pin 15"), F("RX to pin 14"));
#else
  uiStartup(tft, BANNER_NOSENSOR, F("TX to D6"), F("RX to D7"));
#endif
}

static void tryBringUp() {
  showSearch();
  if (!radar.probe()) {
    showNoSensor();
    logIfRoom(F("no radar frame"));
    gReady = false;
    return;
  }
  uiStartup(tft, BANNER_CONFIG, F("Saving settings"), F("wait 10-20 s"));
  logIfRoom(F("configuring radar"));
  if (!radar.configure()) {
    showNoSensor();
    gReady = false;
    return;
  }
  smoother.reset();
  gReady = true;
  gLastPresent = false;
  gLastDistQ = -32000;
  uiArmRunning(tft);
  logIfRoom(radar.rangeAccepted() ? F("radar ready") : F("radar ready, range kept"));
}

static void logFix(const TargetFix &fix) {
  const int16_t distQ =
      fix.hasDistance ? (int16_t)(fix.distanceM * 10.0f + 0.5f) : (int16_t)-1;
  if (fix.present == gLastPresent && distQ == gLastDistQ) {
    return;
  }
  if (Serial.availableForWrite() < 48) {
    return;
  }
  gLastPresent = fix.present;
  gLastDistQ = distQ;
  Serial.print(fix.present ? F("PRESENT") : F("CLEAR"));
  if (fix.hasDistance) {
    Serial.print(F(" "));
    Serial.print((int)(fix.distanceM * 100.0f + 0.5f));
    Serial.print(F("cm"));
  }
  if (fix.hasSpeed) {
    Serial.print(F(" "));
    Serial.print((int)(fix.speedMps * 100.0f));
    Serial.print(F("cm/s"));
  }
  if (fix.hasEnergy) {
    Serial.print(F(" e="));
    Serial.print(fix.energy);
  }
  Serial.println();
}

void setup() {
  // The display runs with no host attached. Do not wait for Serial.
  Serial.begin(115200);
  holdShieldIdlePins();
  tft.begin();
  tft.setRotation(TFT_ROTATION);
  uiBegin(tft);
  radar.begin();
  gLastTryMs = millis();
  tryBringUp();
}

void loop() {
  const uint32_t now = millis();
  if (!gReady) {
    if ((uint32_t)(now - gLastTryMs) >= 2000UL) {
      gLastTryMs = now;
      tryBringUp();
    }
    return;
  }
  if ((uint32_t)(now - gLastPollMs) < 250UL) {
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
