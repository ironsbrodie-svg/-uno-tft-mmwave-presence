#pragma once

#include <Arduino.h>
#include <string.h>

// Distances are centimeters and speeds are centimeters per second so the
// UNO build does not need software floating point.
// Angle and x/y exist only when CONE_DIRECTIONAL is 1 (Mega + a later radar).
#ifndef CONE_DIRECTIONAL
#define CONE_DIRECTIONAL 0
#endif

struct TargetFix {
  bool valid;
  bool present;
  uint8_t count;
  bool hasDistance;
  uint16_t distanceCm;
  bool hasSpeed;
  int16_t speedCms;
  bool hasEnergy;
  uint16_t energy;
#if CONE_DIRECTIONAL
  // 0 degrees is straight ahead. Positive is to the sensor's right.
  bool hasAngle;
  int16_t angleDeg;
  // Top-down centimeters. +x is right, +y is forward.
  bool hasPosition;
  int16_t xCm;
  int16_t yCm;
#endif
};

inline void clearTarget(TargetFix &fix) { memset(&fix, 0, sizeof(fix)); }
