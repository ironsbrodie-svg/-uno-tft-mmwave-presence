#pragma once

#include "target.h"

// Small seam between the radar and the screen.
// A second driver implements this class and the UI stays as it is.
class PresenceSensor {
 public:
  virtual ~PresenceSensor() {}

  virtual bool begin() = 0;
  // True when the module has sent at least one report frame.
  virtual bool probe() = 0;
  // Apply mode, range, and sensitivity. Can block for several seconds.
  virtual bool configure() = 0;
  // Instantaneous reading. Smoothing lives outside the driver.
  virtual bool read(TargetFix &fix) = 0;

  virtual const __FlashStringHelper *name() const = 0;
  virtual const __FlashStringHelper *modeName() const = 0;
  virtual bool providesAngle() const = 0;
  virtual bool providesRange() const = 0;
  virtual uint16_t maxRangeCm() const = 0;
  virtual uint8_t horizontalBeamDeg() const = 0;
  virtual bool rangeAccepted() const { return true; }
};
