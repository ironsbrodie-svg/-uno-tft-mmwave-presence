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

// Display hysteresis. 0 shows every centimeter step.
#define DISTANCE_DEADBAND_CM 7

// Extra hardware. Default off so the Uno build stays small.
#define FEATURE_BATTERY 0
#define FEATURE_BACKLIGHT_PWM 0
#define FEATURE_STATUS_LED 0
#define FEATURE_SERIAL_LOG 0
#define FEATURE_MODE_BUTTON 0
#define BATTERY_PIN A1
#define STATUS_LED_PIN A0
#define MODE_BUTTON_PIN 5
#define BACKLIGHT_IDLE_MS 20000
#define BACKLIGHT_DIM 90
#define LOW_BATTERY_MV 6600
#define LOW_BATTERY_CLEAR_MV 6900
// Nominal bandgap scale: 1.1 V * 1024 * 1000. One-off meter trim:
// set this to 1126400 * Vtrue / Vshown (integer values).
#ifndef BATTERY_CAL_SCALE
#define BATTERY_CAL_SCALE 1126400UL
#endif

// ATmegaBOOT (Nano "ATmega328P (Old Bootloader)") does not clear a
// watchdog reset and can bootloop. That menu sets no compiler flag.
// You must set this to 1 by hand when you use that menu. The watchdog
// then stays off. Power-cycle the board to recover a loop. Prefer a
// Uno or a Nano with the new bootloader.
#ifndef NANO_OLD_BOOTLOADER
#define NANO_OLD_BOOTLOADER 0
#endif
#ifndef ENABLE_WATCHDOG
#if NANO_OLD_BOOTLOADER
#define ENABLE_WATCHDOG 0
#else
#define ENABLE_WATCHDOG 1
#endif
#endif

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

// 1 = module TX only. The sketch does not send commands or save settings.
// Leave 0 unless the command wire (D7, or the Mega TX pin) is open.
#ifndef RADAR_RX_ONLY
#define RADAR_RX_ONLY 0
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
// 1651 v2 capacitive shield: the backlight is D5, the same pin as the
// mode button. Leave FEATURE_MODE_BUTTON at 0 on that shield.
#if FEATURE_MODE_BUTTON
#warning "MODE_BUTTON_PIN is D5. The 1651 v2 capacitive shield uses D5 for the backlight."
#endif

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "c4001_sensor.h"
#include "target_smoother.h"
#include "ui.h"
#include <avr/wdt.h>

// Optiboot and stk500v2 clear MCUSR before the sketch, so a .noinit cookie
// set in the watchdog ISR is the reset marker. Cleared after it is shown.
static const uint32_t kWdtCookie = 0xC4001A5AUL;
volatile uint32_t gWdtCookie __attribute__((section(".noinit")));

// Falls through into the next .init section (no prologue, no ret). `used`
// keeps LTO and --gc-sections from dropping it. Clears MCUSR (r1 is 0),
// then disarms a watchdog a previous sketch left running. WDRF must be
// clear or the disable is ignored. The WDT RESET banner uses the .noinit
// cookie, not the value that was in MCUSR.
__attribute__((naked, used, section(".init3"))) void wdtEarly(void) {
  __asm__ __volatile__(
      "cli\n\t"
      "wdr\n\t"
      "out %0, __zero_reg__\n\t"
      "sts %1, %2\n\t"
      "sts %1, __zero_reg__\n\t"
      :
      : "I"(_SFR_IO_ADDR(MCUSR)), "M"(_SFR_MEM_ADDR(WDTCSR)),
        "r"((uint8_t)(_BV(WDCE) | _BV(WDE))));
}

#if ENABLE_WATCHDOG
ISR(WDT_vect) {
  gWdtCookie = kWdtCookie;
  // Interrupt consumed this timeout. Arm a short system reset so Optiboot
  // still has time to disable the watchdog. Then spin.
  wdt_enable(WDTO_60MS);
  for (;;) {
  }
}

static void armWatchdog() {
  cli();
  wdt_reset();
  MCUSR = (uint8_t)(MCUSR & (uint8_t)~_BV(WDRF));
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  // 8 s, interrupt and reset. WDP3|WDP0 is the 1024K prescaler.
  WDTCSR = _BV(WDIE) | _BV(WDE) | _BV(WDP3) | _BV(WDP0);
  sei();
}
#endif

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

static void showCfgFail() {
  const __FlashStringHelper *why = radar.configError();
  if (why == 0) {
    why = F("config");
  }
  uiStartup(tft, BANNER_CFGFAIL, why, F("not saved"));
}

static void showNoRx() {
  uiStartup(tft, BANNER_NORX, F("No bytes"), F("sensor silent"));
}

static void showCmdRx() {
#if RADAR_LINK == RADAR_LINK_SOFT
  uiStartup(tft, BANNER_CMDRX, F("Check D7"), F("commands, frames ok"));
#elif RADAR_LINK == RADAR_LINK_SERIAL1
  uiStartup(tft, BANNER_CMDRX, F("Check pin 18"), F("commands, frames ok"));
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  uiStartup(tft, BANNER_CMDRX, F("Check pin 16"), F("commands, frames ok"));
#else
  uiStartup(tft, BANNER_CMDRX, F("Check pin 14"), F("commands, frames ok"));
#endif
}

static void showWrongMode() {
  uiStartup(tft, BANNER_MODE, F("Module mode"), F("not the wires"));
}

#if FEATURE_BATTERY
static uint16_t readVccMv() {
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) {
  }
  (void)ADC;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 4; i++) {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
    sum += ADC;
  }
  const uint16_t raw = (uint16_t)(sum / 4UL);
  if (raw == 0) {
    return 5000;
  }
  return (uint16_t)(BATTERY_CAL_SCALE / raw);
}

static uint32_t gLastBatMs = 0;

static void sampleBattery(Adafruit_ILI9341 &panel, uint32_t now, bool force) {
  if (!force && gLastBatMs != 0 && (uint32_t)(now - gLastBatMs) < 3000UL) {
    return;
  }
  gLastBatMs = now;
  (void)analogRead(BATTERY_PIN);
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    sum += analogRead(BATTERY_PIN);
    wdt_reset();
  }
  const uint16_t adc = (uint16_t)(sum / 8UL);
  static uint16_t filt = 0;
  static bool seeded = false;
  static uint16_t shownMv = 0;
  static bool low = false;
  // USB power and no pack: the divider sits at ground. Do not show 0.0 V LOW.
  if (adc < 8) {
    filt = 0;
    seeded = false;
    shownMv = 0;
    low = false;
    uiBattery(panel, 0, 0, false, true);
    return;
  }
  const uint32_t vcc = readVccMv();
  const uint16_t sample =
      (uint16_t)(((uint32_t)adc * vcc * 147UL) / (1024UL * 47UL));
  if (!seeded) {
    filt = sample;
    seeded = true;
  } else {
    filt = (uint16_t)(((uint32_t)filt * 7UL + sample) / 8UL);
  }
  if (shownMv == 0 || (uint16_t)abs((int)filt - (int)shownMv) >= 50) {
    shownMv = filt;
  }
  if (filt <= LOW_BATTERY_MV) {
    low = true;
  } else if (filt >= LOW_BATTERY_CLEAR_MV) {
    low = false;
  }
  int pct = 0;
  if (shownMv >= 8400) {
    pct = 100;
  } else if (shownMv > 6000) {
    pct = (int)(((uint32_t)shownMv - 6000UL) * 100UL / 2400UL);
  }
  uiBattery(panel, shownMv, pct, low, false);
}
#endif

#if FEATURE_STATUS_LED
void statusLedPoll() {
  const uint32_t now = millis();
  const bool fault = !gReady || radar.configUnverified();
  if (fault) {
    digitalWrite(STATUS_LED_PIN, ((now / 150UL) & 1UL) ? HIGH : LOW);
  } else {
    digitalWrite(STATUS_LED_PIN, ((now / 500UL) & 1UL) ? HIGH : LOW);
  }
}
#endif

static void holdWithWdt(uint16_t ms) {
  const uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < ms) {
    wdt_reset();
    C4001_STATUS_POLL();
    delay(20);
  }
}

static void tryBringUp() {
  if (sBanner != BANNER_NOSENSOR && sBanner != BANNER_NORX &&
      sBanner != BANNER_CMDRX && sBanner != BANNER_CFGFAIL &&
      sBanner != BANNER_MODE) {
    showSearch();
  }
  // A stale timestamp must not count as a live sensor.
  if (!radar.probe()) {
    showNoSensor();
    logIfRoom(F("no radar frame"));
    gReady = false;
    return;
  }
#if RADAR_RX_ONLY
  gConfigTried = true;
#else
  if (!gConfigTried) {
    gConfigTried = true;
    if (sBanner != BANNER_NORX && sBanner != BANNER_CMDRX &&
        sBanner != BANNER_CFGFAIL && sBanner != BANNER_MODE) {
      uiStartup(tft, BANNER_CONFIG, F("Checking"), F("radar settings"));
    }
    wdt_reset();
    const bool running = radar.configure();
    wdt_reset();
    if (!running) {
      // Stay here and retry. Do not arm the live screen on a dead command.
      if (!radar.heardBytes()) {
        showNoRx();
        logIfRoom(F("sensor silent"));
      } else {
        showCmdRx();
        logIfRoom(F("NO SENSOR RX"));
      }
      gConfigTried = false;
      gReady = false;
      return;
    }
    if (radar.modeMismatch()) {
      showWrongMode();
      logIfRoom(F("WRONG MODE"));
      holdWithWdt(1200);
    } else if (radar.configUnverified() && radar.configError() != 0 &&
               !radar.rxTimedOut()) {
      showCfgFail();
      logIfRoom(radar.configError() ? radar.configError() : F("config failed"));
      holdWithWdt(1200);
    }
    if (radar.lastFrameMs() == 0) {
      showNoSensor();
      logIfRoom(F("no frame after config"));
      gConfigTried = false;
      gReady = false;
      return;
    }
  }
#endif
  smoother.reset();
  gReady = true;
  gLastPresent = false;
  gLastDist = -1;
  uiArmRunning(tft);
#if FEATURE_BATTERY
  sampleBattery(tft, millis(), true);
#endif
  if (radar.modeMismatch()) {
    uiWarn(tft, F("WRONG MODE"));
    logIfRoom(F("WRONG MODE"));
  } else if (radar.configUnverified()) {
    uiWarn(tft, radar.configError() ? radar.configError() : F("CFG UNVERIFIED"));
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
  // "PRESENT 2500cm -32768cm/s e=65535" is about 36 bytes. The old 32-byte
  // guard dropped that line or sliced it across a full USB buffer.
  if (Serial.availableForWrite() < 48) {
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
#if ENABLE_WATCHDOG
  const bool wdtBoot = gWdtCookie == kWdtCookie;
#else
  const bool wdtBoot = false;
#endif
  gWdtCookie = 0;
  Serial.begin(115200);
  holdShieldIdlePins();
#if FEATURE_STATUS_LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
#endif
#if FEATURE_BACKLIGHT_PWM
  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, 255);
#endif
#if FEATURE_MODE_BUTTON
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
#endif
  tft.begin();
  tft.setRotation(TFT_ROTATION);
#if ENABLE_WATCHDOG
  armWatchdog();
#else
  wdt_disable();
#endif
  uiBegin(tft);
  if (wdtBoot) {
    uiStartup(tft, BANNER_START, F("WDT RESET"), F("recovering"));
    logIfRoom(F("WDT RESET"));
    holdWithWdt(1500);
  }
#if FEATURE_MODE_BUTTON
  if (digitalRead(MODE_BUTTON_PIN) == LOW) {
    delay(40);
    wdt_reset();
    if (digitalRead(MODE_BUTTON_PIN) == LOW) {
      uiStartup(tft, BANNER_START, F("SELF TEST"), F("upload self_test"));
      holdWithWdt(1200);
    }
  }
#endif
  showSearch();
  radar.begin();
  tryBringUp();
  gNextTryMs = millis() + 1200;
}

static void serviceExtras(const TargetFix &raw, const TargetFix &shown,
                          bool haveFix, uint32_t now) {
#if FEATURE_SERIAL_LOG
  static bool csvHeader = false;
  if (!csvHeader && Serial.availableForWrite() >= 52) {
    csvHeader = true;
    Serial.println(F("ms,present,raw_cm,raw_cms,raw_e,sm_cm,sm_cms,sm_e"));
  }
  if (haveFix && csvHeader) {
    char line[64];
    uint8_t n = 0;
    char tmp[12];
    ultoa(now, tmp, 10);
    for (uint8_t c = 0; tmp[c] != 0 && (unsigned)n + 1U < sizeof(line); c++) {
      line[n++] = tmp[c];
    }
    const long fields[7] = {
        raw.present ? 1L : 0L,
        raw.hasDistance ? (long)raw.distanceCm : -1L,
        raw.hasSpeed ? (long)raw.speedCms : -1L,
        raw.hasEnergy ? (long)raw.energy : -1L,
        shown.hasDistance ? (long)shown.distanceCm : -1L,
        shown.hasSpeed ? (long)shown.speedCms : -1L,
        shown.hasEnergy ? (long)shown.energy : -1L};
    for (uint8_t i = 0; i < 7; i++) {
      if ((unsigned)n + 1U < sizeof(line)) {
        line[n++] = ',';
      }
      ltoa(fields[i], tmp, 10);
      for (uint8_t c = 0; tmp[c] != 0 && (unsigned)n + 1U < sizeof(line); c++) {
        line[n++] = tmp[c];
      }
    }
    line[n] = 0;
    if (Serial.availableForWrite() > (int)n + 2) {
      Serial.println(line);
    }
  }
#else
  (void)raw;
  (void)shown;
  (void)haveFix;
#endif
#if FEATURE_BATTERY
  if (gReady) {
    sampleBattery(tft, now, false);
  }
#endif
#if FEATURE_BACKLIGHT_PWM
  static uint32_t lastTargetMs = 0;
  static uint8_t lastLevel = 255;
  if (!gReady) {
    lastTargetMs = now;
  } else if (haveFix && shown.present) {
    lastTargetMs = now;
  }
  const bool dimOk =
      gReady && lastTargetMs != 0 &&
      (uint32_t)(now - lastTargetMs) > BACKLIGHT_IDLE_MS;
  const uint8_t level = dimOk ? (uint8_t)BACKLIGHT_DIM : 255;
  if (level != lastLevel) {
    lastLevel = level;
    analogWrite(BACKLIGHT_PIN, level);
  }
#endif
#if FEATURE_STATUS_LED
  const bool fault = !gReady || radar.configUnverified();
  if (fault) {
    digitalWrite(STATUS_LED_PIN, ((now / 150UL) & 1UL) ? HIGH : LOW);
  } else if (haveFix && shown.present) {
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else {
    digitalWrite(STATUS_LED_PIN, ((now / 500UL) & 1UL) ? HIGH : LOW);
  }
#endif
  (void)now;
}

void loop() {
  const uint32_t now = millis();
  wdt_reset();
  if (!gReady) {
    serviceExtras(TargetFix(), TargetFix(), false, now);
    if ((int32_t)(now - gNextTryMs) < 0) {
      return;
    }
    tryBringUp();
    gNextTryMs = millis() + 1200;
    return;
  }
  if (radar.lastFrameMs() == 0 ||
      (uint32_t)(now - radar.lastFrameMs()) > LINK_TIMEOUT_MS) {
    const bool wrongMode = radar.modeMismatch();
    radar.invalidate();
    gConfigTried = false;
    gReady = false;
    smoother.reset();
    if (wrongMode) {
      showWrongMode();
      logIfRoom(F("WRONG MODE"));
    } else {
      showNoSensor();
      logIfRoom(F("radar link lost"));
    }
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
    if (radar.modeMismatch()) {
      uiWarn(tft, F("WRONG MODE"));
    }
    serviceExtras(TargetFix(), TargetFix(), false, now);
    return;
  }
  uiHeartbeat(tft);
  smoother.apply(raw, shown);
  uiRefresh(tft, shown, capsFor(radar));
  logFix(shown);
  serviceExtras(raw, shown, true, now);
}
