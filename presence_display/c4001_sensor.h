#pragma once

// The sketch sets RADAR_LINK before including this file.
#ifndef RADAR_LINK_SOFT
#define RADAR_LINK_SOFT 0
#define RADAR_LINK_SERIAL1 1
#define RADAR_LINK_SERIAL2 2
#define RADAR_LINK_SERIAL3 3
#endif
#ifndef RADAR_LINK
#define RADAR_LINK RADAR_LINK_SOFT
#endif
#ifndef C4001_BAUD
#define C4001_BAUD 9600
#endif
#ifndef RADAR_RX_PIN
#define RADAR_RX_PIN 6
#endif
#ifndef RADAR_TX_PIN
#define RADAR_TX_PIN 7
#endif

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include "presence_sensor.h"

// UART driver for the C4001. DFRobot_C4001::sensorStop() loops forever and
// every setter calls it, so commands are sent here with a timeout.
// SoftwareSerial's RX buffer is 64 bytes inside that library .cpp. A #define
// in the sketch does not resize it. read() drains the port and keeps the
// newest complete frame in an 80-byte buffer (one frame is under 50 bytes,
// so 80 still holds the next frame if two arrive before a poll).

// Uno, Nano, Pro Mini, and other ATmega328P/168 boards have one UART,
// and it is the USB port. Anything else here is expected to have Serial1.
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(ESP8266)
#if RADAR_LINK != RADAR_LINK_SOFT
#error "No free hardware UART on " ARDUINO_BOARD ". Set RADAR_LINK to RADAR_LINK_SOFT."
#endif
#include <SoftwareSerial.h>
#define RADAR_PORT_IS_SOFT 1
#else
#if RADAR_LINK == RADAR_LINK_SOFT
#error "Hardware UART required on " ARDUINO_BOARD ". Set RADAR_LINK to RADAR_LINK_SERIAL1."
#endif
#define RADAR_PORT_IS_SOFT 0
#endif

class C4001Sensor : public PresenceSensor {
 public:
#if RADAR_PORT_IS_SOFT
  C4001Sensor(uint16_t minCm, uint16_t maxCm, uint16_t threshold, bool ranging,
              bool fretting, uint8_t trigSens, uint8_t keepSens,
              uint8_t trigDelay, uint16_t keepDelay, uint8_t rxPin,
              uint8_t txPin);
#else
  C4001Sensor(uint16_t minCm, uint16_t maxCm, uint16_t threshold, bool ranging,
              bool fretting, uint8_t trigSens, uint8_t keepSens,
              uint8_t trigDelay, uint16_t keepDelay);
#endif

  bool begin() override;
  bool probe() override;
  bool needsConfig() const override { return true; }
  bool configure() override;
  bool read(TargetFix &fix) override;
  uint32_t lastFrameMs() const override { return _lastFrameMs; }
  const __FlashStringHelper *configError() const override { return _fault; }
  bool rxTimedOut() const override { return _rxTimedOut; }
  bool configUnverified() const { return _unverified; }
  bool heardBytes() const { return _heardBytes; }
  bool modeMismatch() const { return _modeMismatch; }
  void invalidate();
  const __FlashStringHelper *name() const override { return F("C4001"); }
  const __FlashStringHelper *modeName() const override {
    return _ranging ? F("ranging") : F("presence");
  }
  bool providesAngle() const override { return false; }
  bool providesRange() const override { return _ranging; }
  uint16_t maxRangeCm() const override { return _maxCm; }
  uint8_t horizontalBeamDeg() const override { return 100; }

 private:
  // One scratch buffer (_buf) holds either a streamed frame or a command
  // reply. configure() never keeps two replies live, so the old stack of
  // ~80-byte Reply objects is gone.
  Stream &port();
  void drain();
  bool takeNewest(char *out, uint8_t outLen);
  bool parseFrame(char *line, TargetFix &fix);
  uint8_t exchange(const char *cmd, uint16_t timeoutMs, bool crlf);
  uint8_t transact(const char *cmd, uint16_t timeoutMs);
  bool waitFrame(uint16_t timeoutMs);
  bool startModule();
  static int parseCm(const char *s, bool *ok);
  static int parseIntField(const char *s, bool *ok);
  static unsigned long parseULong(const char *s, bool *ok);
  static bool replyCm(const char *text, uint8_t index, int *cm);
  static bool replyInt(const char *text, uint8_t index, int *value);
  static void catCm(char *dst, uint8_t cap, uint16_t cm);
  static void catNum(char *dst, uint8_t cap, int value);

  static const uint8_t kBufCap = 80;
  static const uint8_t kDone = 1;
  static const uint8_t kError = 2;

#if RADAR_PORT_IS_SOFT
  SoftwareSerial _port;
#endif
  uint16_t _minCm;
  uint16_t _maxCm;
  uint16_t _threshold;
  bool _ranging;
  bool _fretting;
  uint8_t _trigSens;
  uint8_t _keepSens;
  uint8_t _trigDelay;
  uint16_t _keepDelay;
  bool _knowMode;
  bool _speedMode;
  bool _rxTimedOut;
  bool _unverified;
  bool _crlf;
  bool _heardBytes;
  bool _modeMismatch;
  const __FlashStringHelper *_fault;
  uint32_t _lastFrameMs;
  char _buf[kBufCap];
  uint8_t _len;
};

#if !RADAR_PORT_IS_SOFT
static HardwareSerial &c4001Uart() {
#if RADAR_LINK == RADAR_LINK_SERIAL1
  return Serial1;
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  return Serial2;
#elif RADAR_LINK == RADAR_LINK_SERIAL3
  return Serial3;
#else
  return Serial1;
#endif
}
#endif

#if RADAR_PORT_IS_SOFT
inline C4001Sensor::C4001Sensor(uint16_t minCm, uint16_t maxCm,
                                uint16_t threshold, bool ranging, bool fretting,
                                uint8_t trigSens, uint8_t keepSens,
                                uint8_t trigDelay, uint16_t keepDelay,
                                uint8_t rxPin, uint8_t txPin)
    : _port(rxPin, txPin),
#else
inline C4001Sensor::C4001Sensor(uint16_t minCm, uint16_t maxCm,
                                uint16_t threshold, bool ranging, bool fretting,
                                uint8_t trigSens, uint8_t keepSens,
                                uint8_t trigDelay, uint16_t keepDelay)
    :
#endif
      _minCm(minCm),
      _maxCm(maxCm),
      _threshold(threshold),
      _ranging(ranging),
      _fretting(fretting),
      _trigSens(trigSens),
      _keepSens(keepSens),
      _trigDelay(trigDelay),
      _keepDelay(keepDelay),
      _knowMode(false),
      _speedMode(false),
      _rxTimedOut(false),
      _unverified(false),
      _crlf(false),
      _heardBytes(false),
      _modeMismatch(false),
      _fault(0),
      _lastFrameMs(0),
      _len(0) {
  _buf[0] = 0;
}

inline Stream &C4001Sensor::port() {
#if RADAR_PORT_IS_SOFT
  return _port;
#else
  return c4001Uart();
#endif
}

inline bool C4001Sensor::begin() {
#if RADAR_PORT_IS_SOFT
  _port.begin(C4001_BAUD);
#else
  c4001Uart().begin(C4001_BAUD);
#endif
  return true;
}

#if defined(FEATURE_STATUS_LED) && FEATURE_STATUS_LED
void statusLedPoll();
#define C4001_STATUS_POLL() statusLedPoll()
#else
#define C4001_STATUS_POLL() ((void)0)
#endif

inline void C4001Sensor::invalidate() {
  _lastFrameMs = 0;
  _knowMode = false;
  _modeMismatch = false;
  _len = 0;
  _buf[0] = 0;
  while (port().available() > 0) {
    port().read();
  }
}

inline void C4001Sensor::drain() {
  Stream &link = port();
  while (link.available() > 0) {
    const char c = (char)link.read();
    if (_len + 1 >= kBufCap) {
      uint8_t drop = 24;
      if (drop >= _len) {
        _len = 0;
      } else {
        memmove(_buf, _buf + drop, _len - drop);
        _len = (uint8_t)(_len - drop);
      }
    }
    _buf[_len++] = c;
    _buf[_len] = 0;
  }
}

inline bool C4001Sensor::takeNewest(char *out, uint8_t outLen) {
  int lastEnd = -1;
  int start = -1;
  for (uint8_t i = 0; i < _len; i++) {
    if (_buf[i] == '$') {
      start = (int)i;
    }
    if (_buf[i] == '*' && start >= 0 && (int)i > start + 5) {
      lastEnd = (int)i;
    }
  }
  if (lastEnd < 0) {
    return false;
  }
  start = 0;
  for (int i = lastEnd; i >= 0; i--) {
    if (_buf[i] == '$') {
      start = i;
      break;
    }
  }
  uint8_t n = (uint8_t)(lastEnd - start + 1);
  if (n >= outLen) {
    const uint8_t rest = (uint8_t)(_len - (lastEnd + 1));
    memmove(_buf, _buf + lastEnd + 1, rest);
    _len = rest;
    _buf[_len] = 0;
    return false;
  }
  memcpy(out, _buf + start, n);
  out[n] = 0;
  const uint8_t rest = (uint8_t)(_len - (lastEnd + 1));
  memmove(_buf, _buf + lastEnd + 1, rest);
  _len = rest;
  _buf[_len] = 0;
  return true;
}

static const char *c4Skip(const char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
    s++;
  }
  return s;
}

inline int C4001Sensor::parseCm(const char *s, bool *ok) {
  s = c4Skip(s);
  bool neg = false;
  if (*s == '-') {
    neg = true;
    s++;
  }
  long whole = 0;
  long frac = 0;
  int fd = 0;
  bool dot = false;
  bool any = false;
  while (*s != 0) {
    if (*s >= '0' && *s <= '9') {
      any = true;
      if (!dot) {
        whole = whole * 10 + (*s - '0');
      } else if (fd < 3) {
        frac = frac * 10 + (*s - '0');
        fd++;
      }
    } else if (*s == '.' && !dot) {
      dot = true;
    } else {
      break;
    }
    s++;
  }
  if (!any) {
    *ok = false;
    return 0;
  }
  long cm;
  if (!dot) {
    cm = whole * 100;
  } else {
    while (fd < 2) {
      frac *= 10;
      fd++;
    }
    while (fd > 2) {
      frac = (frac + 5) / 10;
      fd--;
    }
    cm = whole * 100 + frac;
  }
  if (cm > 32767L) {
    cm = 32767L;
  }
  *ok = true;
  const int clipped = neg ? (int)-cm : (int)cm;
  return clipped;
}

inline unsigned long C4001Sensor::parseULong(const char *s, bool *ok) {
  s = c4Skip(s);
  if (*s < '0' || *s > '9') {
    *ok = false;
    return 0;
  }
  unsigned long v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10UL + (unsigned long)(*s - '0');
    s++;
  }
  *ok = true;
  return v;
}

inline int C4001Sensor::parseIntField(const char *s, bool *ok) {
  s = c4Skip(s);
  if (*s < '0' || *s > '9') {
    *ok = false;
    return 0;
  }
  long v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    s++;
  }
  *ok = true;
  return (int)v;
}

static const char *c4Response(const char *text) {
  const char *p = strstr(text, "Response");
  if (p == 0) {
    return 0;
  }
  return p + 8;
}

static const char *c4Nth(const char *p, uint8_t index) {
  for (uint8_t i = 0; i < index; i++) {
    p = c4Skip(p);
    if (*p == 0) {
      return p;
    }
    while (*p && *p != ' ' && *p != '\r' && *p != '\n') {
      p++;
    }
  }
  return c4Skip(p);
}

inline bool C4001Sensor::replyCm(const char *text, uint8_t index, int *cm) {
  const char *p = c4Response(text);
  if (p == 0) {
    return false;
  }
  bool ok = false;
  const int v = parseCm(c4Nth(p, index), &ok);
  if (!ok) {
    return false;
  }
  *cm = v;
  return true;
}

inline bool C4001Sensor::replyInt(const char *text, uint8_t index, int *value) {
  const char *p = c4Response(text);
  if (p == 0) {
    return false;
  }
  bool ok = false;
  const int v = parseIntField(c4Nth(p, index), &ok);
  if (!ok) {
    return false;
  }
  *value = v;
  return true;
}

inline bool C4001Sensor::parseFrame(char *line, TargetFix &fix) {
  char *star = strchr(line, '*');
  if (star != 0) {
    *star = 0;
  }
  const bool speedMode = strncmp(line, "$DFDMD", 6) == 0;
  const bool presenceMode = strncmp(line, "$DFHPD", 6) == 0;
  if (!speedMode && !presenceMode) {
    return false;
  }
  char *tok[8];
  uint8_t n = 0;
  tok[n++] = line;
  for (char *p = line; *p != 0 && n < 8; p++) {
    if (*p == ',') {
      *p = 0;
      tok[n++] = p + 1;
    }
  }
  if (speedMode) {
    if (n < 6) {
      return false;
    }
    bool ok = false;
    const int count = parseIntField(tok[1], &ok);
    if (!ok || count < 0 || count > 1) {
      return false;
    }
    fix.present = count == 1;
    fix.count = (uint8_t)count;
    const int limit = (int)_maxCm + 150;
    if (count == 1) {
      bool rok = false;
      const int cm = parseCm(tok[3], &rok);
      if (rok && cm >= 5 && cm <= limit) {
        fix.hasDistance = true;
        fix.distanceCm = (uint16_t)cm;
      }
      bool sok = false;
      int spd = parseCm(tok[4], &sok);
      if (spd > 1500) {
        spd = 1500;
      }
      if (spd < -1500) {
        spd = -1500;
      }
      if (sok && spd > -1500 && spd < 1500) {
        fix.hasSpeed = true;
        fix.speedCms = (int16_t)spd;
      }
      bool eok = false;
      const unsigned long energy = parseULong(tok[5], &eok);
      if (eok && energy <= 65535UL) {
        fix.hasEnergy = true;
        fix.energy = (uint16_t)energy;
      }
    }
    _speedMode = true;
    _knowMode = true;
    return true;
  }
  if (n < 2) {
    return false;
  }
  bool ok = false;
  const int exist = parseIntField(tok[1], &ok);
  if (!ok || exist < 0 || exist > 1) {
    return false;
  }
  fix.present = exist == 1;
  fix.count = (uint8_t)exist;
  _speedMode = false;
  _knowMode = true;
  return true;
}

inline bool C4001Sensor::probe() {
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < 2000UL) {
    drain();
    char line[56];
    TargetFix junk;
    if (takeNewest(line, sizeof(line))) {
      clearTarget(junk);
      if (parseFrame(line, junk)) {
        _lastFrameMs = millis();
        return true;
      }
    }
    delay(5);
    wdt_reset();
    C4001_STATUS_POLL();
  }
  // No stream. A module left stopped is recovered with sensorStart only.
  // That command does not write the module's flash.
  port().print(F("sensorStart"));
  if (_crlf) {
    port().print(F("\r\n"));
  }
  const uint32_t kick = millis();
  while ((uint32_t)(millis() - kick) < 1000UL) {
    drain();
    char line[56];
    TargetFix junk;
    if (takeNewest(line, sizeof(line))) {
      clearTarget(junk);
      if (parseFrame(line, junk)) {
        _lastFrameMs = millis();
        return true;
      }
    }
    delay(5);
    wdt_reset();
    C4001_STATUS_POLL();
  }
  return false;
}

inline uint8_t C4001Sensor::exchange(const char *cmd, uint16_t timeoutMs,
                                      bool crlf) {
  while (port().available() > 0) {
    port().read();
  }
  _len = 0;
  _buf[0] = 0;
  port().print(cmd);
  if (crlf) {
    port().print(F("\r\n"));
  }
  char tail[8];
  uint8_t tn = 0;
  tail[0] = 0;
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    while (port().available() > 0) {
      const char c = (char)port().read();
      _heardBytes = true;
      if ((uint8_t)(_len + 1) < kBufCap) {
        _buf[_len++] = c;
        _buf[_len] = 0;
      }
      if ((uint8_t)(tn + 1) < sizeof(tail)) {
        tail[tn++] = c;
      } else {
        memmove(tail, tail + 1, sizeof(tail) - 2);
        tail[sizeof(tail) - 2] = c;
        tn = (uint8_t)(sizeof(tail) - 1);
      }
      tail[tn] = 0;
      if (strstr(tail, "Error") != 0 || strstr(_buf, "Error") != 0) {
        delay(100);
        return kError;
      }
      if (strstr(tail, "Done") != 0 || strstr(_buf, "Done") != 0) {
        delay(100);
        return kDone;
      }
    }
    delay(2);
    wdt_reset();
    C4001_STATUS_POLL();
  }
  delay(100);
  wdt_reset();
  C4001_STATUS_POLL();
  return 0;
}

inline uint8_t C4001Sensor::transact(const char *cmd, uint16_t timeoutMs) {
  uint8_t kind = exchange(cmd, timeoutMs, _crlf);
  if (kind == 0) {
    // One retry. If the bare command was silent, try CRLF (or the reverse).
    const bool other = !_crlf;
    kind = exchange(cmd, timeoutMs, other);
    if (kind != 0) {
      _crlf = other;
    }
  }
  return kind;
}

inline bool C4001Sensor::waitFrame(uint16_t timeoutMs) {
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs) {
    drain();
    char line[56];
    TargetFix junk;
    if (takeNewest(line, sizeof(line))) {
      clearTarget(junk);
      if (parseFrame(line, junk)) {
        _lastFrameMs = millis();
        return true;
      }
    }
    delay(5);
    wdt_reset();
    C4001_STATUS_POLL();
  }
  return false;
}

inline bool C4001Sensor::startModule() {
  return transact("sensorStart", 1000) == kDone;
}

inline void C4001Sensor::catNum(char *dst, uint8_t cap, int value) {
  char num[8];
  itoa(value, num, 10);
  uint8_t n = (uint8_t)strlen(dst);
  for (uint8_t i = 0; num[i] != 0 && n + 1 < cap; i++) {
    dst[n++] = num[i];
  }
  dst[n] = 0;
}

inline void C4001Sensor::catCm(char *dst, uint8_t cap, uint16_t cm) {
  const uint16_t tenths = (uint16_t)(cm / 10);
  catNum(dst, cap, (int)(tenths / 10));
  uint8_t n = (uint8_t)strlen(dst);
  if (n + 2 < cap) {
    dst[n++] = '.';
    dst[n++] = (char)('0' + (tenths % 10));
    dst[n] = 0;
  }
}

inline bool C4001Sensor::configure() {
  _fault = 0;
  _rxTimedOut = false;
  _unverified = false;
  _heardBytes = false;
  _modeMismatch = false;

  // Done: stopped now. Error: datasheet "already not running". Both are fine.
  // Timeout is a missing RX answer, not proof the module is absent.
  const uint8_t stopped = transact("sensorStop", 2000);
  if (stopped == 0) {
    _rxTimedOut = true;
    _unverified = true;
    // Zero bytes: the module went silent. Frames with no Done: commands
    // are not reaching the module (open D7) while the stream is still up.
    _fault = _heardBytes ? F("NO SENSOR RX") : F("NO RX");
    return false;
  }

  bool dirty = false;
  if (!_knowMode || _speedMode != _ranging) {
    // Datasheet §2.6: response is None. ESPHome sometimes sees Done.
    // Silence after a short wait is success. Mode is confirmed from the
    // frame prefix after sensorStart. This is not a flash write: whether
    // setRunApp survives saveConfig is not settled, so it must not force
    // saveConfig on every boot.
    exchange(_ranging ? "setRunApp 1" : "setRunApp 0", 400, _crlf);
    delay(100);
  }

  const uint8_t rangeKind = transact("getRange", 800);
  int gotMin = 0;
  int gotMax = 0;
  const bool rangeOk = rangeKind == kDone && replyCm(_buf, 0, &gotMin) &&
                       replyCm(_buf, 1, &gotMax);
  if (!rangeOk) {
    _unverified = true;
    _fault = F("getRange");
  }
  if (rangeOk && (abs(gotMin - (int)_minCm) > 15 ||
                  abs(gotMax - (int)_maxCm) > 15)) {
    char cmd[28];
    strncpy(cmd, "setRange ", sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = 0;
    catCm(cmd, sizeof(cmd), _minCm);
    uint8_t n = (uint8_t)strlen(cmd);
    if ((uint8_t)(n + 1) < sizeof(cmd)) {
      cmd[n++] = ' ';
      cmd[n] = 0;
    }
    catCm(cmd, sizeof(cmd), _maxCm);
    if (transact(cmd, 800) != kDone) {
      _unverified = true;
      _fault = F("setRange");
    } else {
      dirty = true;
    }
  }

  if (_ranging) {
    const uint8_t thrKind = transact("getThrFactor", 800);
    int gotThr = -1;
    const bool thrOk = thrKind == kDone && replyInt(_buf, 0, &gotThr);
    // Match the integer we send, or 10x that integer, so a scaled read-back
    // does not rewrite flash on every boot.
    const bool thrMatch =
        thrOk && (gotThr == (int)_threshold || gotThr == (int)_threshold * 10);
    if (!thrOk) {
      _unverified = true;
      _fault = F("getThrFactor");
    } else if (!thrMatch) {
      char cmd[20];
      strncpy(cmd, "setThrFactor ", sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = 0;
      catNum(cmd, sizeof(cmd), (int)_threshold);
      if (transact(cmd, 800) != kDone) {
        _unverified = true;
        _fault = F("setThrFactor");
      } else {
        dirty = true;
      }
    }
    const uint8_t microKind = transact("getMicroMotion", 800);
    int gotMicro = -1;
    const int wantMicro = _fretting ? 1 : 0;
    if (microKind != kDone || !replyInt(_buf, 0, &gotMicro)) {
      _unverified = true;
      _fault = F("getMicroMotion");
    } else if (gotMicro != wantMicro) {
      if (transact(wantMicro ? "setMicroMotion 1" : "setMicroMotion 0", 800) !=
          kDone) {
        _unverified = true;
        _fault = F("setMicroMotion");
      } else {
        dirty = true;
      }
    }
  } else {
    // Datasheet: trigger distance cannot be greater than max - min.
    uint16_t trigCm = 0;
    if (_maxCm > _minCm) {
      trigCm = (uint16_t)(_maxCm - _minCm);
    }
    const uint8_t trigKind = transact("getTrigRange", 800);
    int gotTrig = 0;
    if (trigKind != kDone || !replyCm(_buf, 0, &gotTrig)) {
      _unverified = true;
      _fault = F("getTrigRange");
    } else if (abs(gotTrig - (int)trigCm) > 15) {
      char cmd[24];
      strncpy(cmd, "setTrigRange ", sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = 0;
      catCm(cmd, sizeof(cmd), trigCm);
      if (transact(cmd, 800) != kDone) {
        _unverified = true;
        _fault = F("setTrigRange");
      } else {
        dirty = true;
      }
    }
    const uint8_t sensKind = transact("getSensitivity", 800);
    int gotKeep = -1;
    int gotTrigS = -1;
    if (sensKind != kDone || !replyInt(_buf, 0, &gotKeep) ||
        !replyInt(_buf, 1, &gotTrigS)) {
      _unverified = true;
      _fault = F("getSensitivity");
    } else if (gotKeep != (int)_keepSens || gotTrigS != (int)_trigSens) {
      char cmd[24];
      strncpy(cmd, "setSensitivity ", sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = 0;
      catNum(cmd, sizeof(cmd), (int)_keepSens);
      uint8_t n = (uint8_t)strlen(cmd);
      if ((uint8_t)(n + 1) < sizeof(cmd)) {
        cmd[n++] = ' ';
        cmd[n] = 0;
      }
      catNum(cmd, sizeof(cmd), (int)_trigSens);
      if (transact(cmd, 800) != kDone) {
        _unverified = true;
        _fault = F("setSensitivity");
      } else {
        dirty = true;
      }
    }
    const uint8_t latKind = transact("getLatency", 800);
    int gotTrigD = 0;
    int gotKeepD = 0;
    const uint32_t wantKeepCs32 = (uint32_t)_keepDelay * 50UL;
    const int wantKeepCs = wantKeepCs32 > 30000UL ? 30000 : (int)wantKeepCs32;
    if (latKind != kDone || !replyCm(_buf, 0, &gotTrigD) ||
        !replyCm(_buf, 1, &gotKeepD)) {
      _unverified = true;
      _fault = F("getLatency");
    } else if (abs(gotTrigD - (int)_trigDelay) > 5 ||
               abs(gotKeepD - wantKeepCs) > 50) {
      char cmd[28];
      strncpy(cmd, "setLatency ", sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = 0;
      catNum(cmd, sizeof(cmd), (int)(_trigDelay / 100));
      uint8_t n = (uint8_t)strlen(cmd);
      if ((uint8_t)(n + 4) < sizeof(cmd)) {
        cmd[n++] = '.';
        cmd[n++] = (char)('0' + ((_trigDelay / 10) % 10));
        cmd[n++] = (char)('0' + (_trigDelay % 10));
        cmd[n++] = ' ';
        cmd[n] = 0;
      }
      catNum(cmd, sizeof(cmd), wantKeepCs / 100);
      n = (uint8_t)strlen(cmd);
      if ((uint8_t)(n + 2) < sizeof(cmd)) {
        cmd[n++] = '.';
        cmd[n++] = (char)('0' + ((wantKeepCs / 10) % 10));
        cmd[n] = 0;
      }
      if (transact(cmd, 800) != kDone) {
        _unverified = true;
        _fault = F("setLatency");
      } else {
        dirty = true;
      }
    }
  }

  // A failed save must be retried next boot even if RAM read-back matches.
  // 1023 is the last byte of a 1024-byte EEPROM (Uno/Nano). Skip it if the
  // part is smaller so we do not write past the end.
  static const int kEePending = 1023;
  const bool eeOk = (unsigned)kEePending < EEPROM.length();
  const bool savePending = eeOk && EEPROM.read(kEePending) == 0xA5;
  if (dirty || savePending) {
    if (eeOk) {
      EEPROM.update(kEePending, 0xA5);
    }
    if (transact("saveConfig", 2000) == kDone) {
      if (eeOk) {
        EEPROM.update(kEePending, 0x00);
      }
    } else {
      _unverified = true;
      _fault = F("saveConfig");
    }
  }
  startModule();
  _len = 0;
  _knowMode = false;
  const bool sawFrame = waitFrame(1500);
  if (!sawFrame || _speedMode != _ranging) {
    _unverified = true;
    if (_fault == 0) {
      _fault = F("mode");
    }
    if (sawFrame && _speedMode != _ranging) {
      _modeMismatch = true;
    }
  }
  // waitFrame() stamps _lastFrameMs only when it parsed a frame. A miss
  // must not keep the probe timestamp, or the link looks healthy.
  if (!sawFrame) {
    _lastFrameMs = 0;
  }
  return true;
}

inline bool C4001Sensor::read(TargetFix &fix) {
  clearTarget(fix);
  drain();
  char line[56];
  if (!takeNewest(line, sizeof(line))) {
    return false;
  }
  char newer[56];
  while (takeNewest(newer, sizeof(newer))) {
    memcpy(line, newer, sizeof(line));
  }
  if (!parseFrame(line, fix)) {
    return false;
  }
  // Wrong-mode frames must not refresh the link timer. After
  // LINK_TIMEOUT_MS the sketch drops to NO SENSOR, then CHECKING, and
  // setRunApp runs again. Do not replace a fault we already recorded.
  if (_speedMode != _ranging) {
    _unverified = true;
    _modeMismatch = true;
    if (_fault == 0) {
      _fault = F("mode");
    }
    return false;
  }
  fix.valid = true;
  _modeMismatch = false;
  _lastFrameMs = millis();
  return true;
}
