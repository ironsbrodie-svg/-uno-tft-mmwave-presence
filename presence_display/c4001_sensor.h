#pragma once

// Link ids. The sketch sets RADAR_LINK to one of these before including
// this file. Uno builds keep RADAR_LINK_SOFT. A Mega build changes one line
// to RADAR_LINK_SERIAL1 (or SERIAL2 / SERIAL3).
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
#include "presence_sensor.h"

// DFRobot_C4001's UART class is SoftwareSerial on the Uno and HardwareSerial
// on a Mega. The two builds are not interchangeable.
#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#if RADAR_LINK != RADAR_LINK_SOFT
#error "Uno C4001 builds use SoftwareSerial. Set RADAR_LINK to RADAR_LINK_SOFT at the top of the sketch."
#endif
#ifndef _SS_MAX_RX_BUFF
#define _SS_MAX_RX_BUFF 128
#endif
#define RADAR_PORT_IS_SOFT 1
#else
#if RADAR_LINK == RADAR_LINK_SOFT
#error "This board's C4001 driver uses a hardware UART. Set RADAR_LINK to RADAR_LINK_SERIAL1 (Mega pins 19 RX / 18 TX)."
#endif
#define RADAR_PORT_IS_SOFT 0
#endif

#include <DFRobot_C4001.h>

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
  bool configure() override;
  bool read(TargetFix &fix) override;

  const __FlashStringHelper *name() const override { return F("C4001"); }
  const __FlashStringHelper *modeName() const override {
    return _ranging ? F("ranging") : F("presence");
  }
  bool providesAngle() const override { return false; }
  bool providesRange() const override { return _ranging; }
  uint16_t maxRangeCm() const override { return _maxCm; }
  uint8_t horizontalBeamDeg() const override { return 100; }
  bool rangeAccepted() const override { return _rangeOk; }

 private:
  Stream &port();
  void clampPresenceRange(uint16_t &minCm, uint16_t &maxCm) const;

#if RADAR_PORT_IS_SOFT
  SoftwareSerial _port;
#endif
  DFRobot_C4001_UART _radar;
  uint16_t _minCm;
  uint16_t _maxCm;
  uint16_t _threshold;
  bool _ranging;
  bool _fretting;
  uint8_t _trigSens;
  uint8_t _keepSens;
  uint8_t _trigDelay;
  uint16_t _keepDelay;
  bool _rangeOk;
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
#error "Set RADAR_LINK to RADAR_LINK_SERIAL1, RADAR_LINK_SERIAL2, or RADAR_LINK_SERIAL3."
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
      _radar(&_port, C4001_BAUD),
#else
inline C4001Sensor::C4001Sensor(uint16_t minCm, uint16_t maxCm,
                                uint16_t threshold, bool ranging, bool fretting,
                                uint8_t trigSens, uint8_t keepSens,
                                uint8_t trigDelay, uint16_t keepDelay)
    : _radar(&c4001Uart(), C4001_BAUD),
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
      _rangeOk(false) {
}

inline Stream &C4001Sensor::port() {
#if RADAR_PORT_IS_SOFT
  return _port;
#else
  return c4001Uart();
#endif
}

inline bool C4001Sensor::begin() {
  // The library opens the UART. On the Uno it also waits about a second.
  // It cannot tell whether the module is actually connected.
  bool ok = _radar.begin();
  delay(200);
  return ok;
}

inline bool C4001Sensor::probe() {
  Stream &link = port();
  while (link.available() > 0) {
    link.read();
  }
  // Same text the library sends. No line ending.
  const char startCmd[] = "sensorStart";
  link.write(reinterpret_cast<const uint8_t *>(startCmd), sizeof(startCmd) - 1);

  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < 2000UL) {
    if (link.available() > 0 && link.read() == '$') {
      return true;
    }
  }
  return false;
}

inline void C4001Sensor::clampPresenceRange(uint16_t &minCm,
                                            uint16_t &maxCm) const {
  // setDetectionRange() rejects values outside this window.
  if (maxCm > 2000) {
    maxCm = 2000;
  }
  if (maxCm < 240) {
    maxCm = 240;
  }
  if (minCm < 30) {
    minCm = 30;
  }
  if (minCm >= maxCm) {
    minCm = 30;
  }
}

inline bool C4001Sensor::configure() {
  // Each library call stops the module, writes a command, and saves it.
  // The first successful boot can sit on CONFIG for 10 to 20 seconds.
  if (_ranging) {
    _radar.setSensorMode(eSpeedMode);
    _rangeOk = _radar.setDetectThres(_minCm, _maxCm, _threshold);
    _radar.setFrettingDetection(_fretting ? eON : eOFF);
  } else {
    _radar.setSensorMode(eExitMode);
    uint16_t minCm = _minCm;
    uint16_t maxCm = _maxCm;
    clampPresenceRange(minCm, maxCm);
    _rangeOk = _radar.setDetectionRange(minCm, maxCm, maxCm);
    if (_trigSens > 9) {
      _trigSens = 9;
    }
    if (_keepSens > 9) {
      _keepSens = 9;
    }
    _radar.setTrigSensitivity(_trigSens);
    _radar.setKeepSensitivity(_keepSens);
    _radar.setDelay(_trigDelay, _keepDelay);
  }
  return true;
}

inline bool C4001Sensor::read(TargetFix &fix) {
  clearTarget(fix);
  fix.valid = true;

  if (_ranging) {
    // getTargetNumber() has to run first. It refreshes range, speed, energy.
    // The UART report has no angle field; those flags stay false.
    const uint8_t count = _radar.getTargetNumber();
    if (count > 0) {
      fix.present = true;
      fix.count = count;
      fix.hasDistance = true;
      fix.distanceM = _radar.getTargetRange();
      fix.hasSpeed = true;
      fix.speedMps = _radar.getTargetSpeed();
      fix.hasEnergy = true;
      fix.energy = _radar.getTargetEnergy();
    }
    return true;
  }

  if (_radar.motionDetection()) {
    fix.present = true;
    fix.count = 1;
  }
  return true;
}
