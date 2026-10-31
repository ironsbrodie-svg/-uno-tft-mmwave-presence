#pragma once

#include "target.h"

// Small seam between the radar and the screen.
// A second driver implements this class and the UI stays as it is.
class PresenceSensor {
 public:
  virtual ~PresenceSensor() {}

  virtual bool begin() = 0;
  // True when the module has sent at least one '$' (TX path, module to board).
  virtual bool probe() = 0;
  // True when the saved settings already match this sketch.
  virtual bool needsConfig() const = 0;
  // Apply mode and range. Times out. Does not loop forever.
  virtual bool configure() = 0;
  // Non-blocking. True when a new valid frame was parsed.
  virtual bool read(TargetFix &fix) = 0;

  virtual uint32_t lastFrameMs() const = 0;
  virtual const __FlashStringHelper *configError() const = 0;
  virtual bool rxTimedOut() const = 0;

  virtual const __FlashStringHelper *name() const = 0;
  virtual const __FlashStringHelper *modeName() const = 0;
  virtual bool providesAngle() const = 0;
  virtual bool providesRange() const = 0;
  virtual uint16_t maxRangeCm() const = 0;
  virtual uint8_t horizontalBeamDeg() const = 0;
};
