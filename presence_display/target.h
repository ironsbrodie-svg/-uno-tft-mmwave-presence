#pragma once

#include <Arduino.h>
#include <string.h>

// One reading, independent of which radar produced it.
// A C4001 fills presence, distance, speed, and energy.
// hasAngle and hasPosition stay false until a sensor that can
// resolve direction (for example an HLK-LD2450) is added.
struct TargetFix {
  bool valid;
  bool present;
  uint8_t count;

  bool hasDistance;
  float distanceM;

  bool hasSpeed;
  float speedMps;

  bool hasEnergy;
  uint32_t energy;

  // 0 degrees is straight ahead. Positive degrees are to the sensor's right.
  bool hasAngle;
  float angleDeg;

  // Top-down position in meters. +x is right, +y is forward.
  bool hasPosition;
  float xM;
  float yM;
};

inline void clearTarget(TargetFix &fix) { memset(&fix, 0, sizeof(fix)); }
