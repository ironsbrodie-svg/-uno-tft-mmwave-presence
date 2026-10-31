#pragma once

// Compiled only when the sketch sets CONE_DIRECTIONAL to 1 before include.
// The UNO sketch does not include this file, so none of it is linked there.
// It draws a bearing dot for a future radar (for example HLK-LD2450 on a
// Mega). The C4001 driver never sets the angle or x/y fields.

#if CONE_DIRECTIONAL

#include <Adafruit_ILI9341.h>
#include "../target.h"

static int16_t dirSin256(int deg) {
  while (deg < 0) {
    deg += 360;
  }
  deg %= 360;
  int sign = 1;
  if (deg >= 180) {
    deg -= 180;
    sign = -1;
  }
  if (deg > 90) {
    deg = 180 - deg;
  }
  static const int16_t kTable[10] = {0,   44,  88,  128, 165,
                                     196, 222, 241, 252, 256};
  const int idx = deg / 10;
  const int frac = deg % 10;
  const int16_t a = kTable[idx];
  const int16_t b = kTable[idx < 9 ? idx + 1 : 9];
  const int16_t v = (int16_t)(a + (int16_t)((b - a) * frac / 10));
  return (int16_t)(sign * v);
}

static int16_t dirCos256(int deg) { return dirSin256(deg + 90); }

static int16_t dirBearing(int16_t xCm, int16_t yCm) {
  if (xCm == 0 && yCm == 0) {
    return 0;
  }
  const int32_t ax = xCm < 0 ? -xCm : xCm;
  const int32_t ay = yCm < 0 ? -yCm : yCm;
  const int16_t a = (int16_t)((ay * 90) / (ax + ay));
  int16_t fromAhead = (int16_t)(90 - a);
  if (yCm < 0) {
    fromAhead = (int16_t)(180 - fromAhead);
  }
  if (xCm < 0) {
    fromAhead = (int16_t)-fromAhead;
  }
  return fromAhead;
}

static uint16_t dirHypotCm(int16_t xCm, int16_t yCm) {
  uint32_t mag = (uint32_t)((int32_t)xCm * xCm + (int32_t)yCm * yCm);
  uint32_t root = 0;
  uint32_t bit = 1UL << 30;
  while (bit > mag) {
    bit >>= 2;
  }
  while (bit != 0) {
    if (mag >= root + bit) {
      mag -= root + bit;
      root = (root >> 1) + bit;
    } else {
      root >>= 1;
    }
    bit >>= 2;
  }
  return (uint16_t)root;
}

static int16_t sDirX = 0;
static int16_t sDirY = 0;
static bool sDirOn = false;

static void directionErase(Adafruit_ILI9341 &tft) {
  if (!sDirOn) {
    return;
  }
  tft.fillCircle(sDirX, sDirY, 8, ILI9341_BLACK);
  sDirOn = false;
}

static void directionDraw(Adafruit_ILI9341 &tft, int16_t apexX, int16_t apexY,
                          int half, int16_t radiusMax, uint16_t maxCm,
                          const TargetFix &fix) {
  if (!fix.present || (!fix.hasAngle && !fix.hasPosition)) {
    return;
  }
  int16_t ang = fix.hasAngle ? fix.angleDeg : dirBearing(fix.xCm, fix.yCm);
  uint16_t cm = fix.hasDistance ? fix.distanceCm : 0;
  if (fix.hasPosition) {
    cm = dirHypotCm(fix.xCm, fix.yCm);
  }
  if (half < 1) {
    half = 1;
  }
  if (ang > half) {
    ang = (int16_t)half;
  }
  if (ang < -half) {
    ang = (int16_t)-half;
  }
  uint16_t cap = maxCm < 50 ? 50 : maxCm;
  if (cm > cap) {
    cm = cap;
  }
  int16_t radius = (int16_t)(((int32_t)cm * radiusMax) / cap);
  if (radius < 4) {
    radius = 4;
  }
  sDirX = (int16_t)(apexX + (int16_t)(((int32_t)radius * dirSin256(ang)) / 256));
  sDirY = (int16_t)(apexY - (int16_t)(((int32_t)radius * dirCos256(ang)) / 256));
  sDirOn = true;
  tft.fillCircle(sDirX, sDirY, 7, ILI9341_YELLOW);
  tft.fillCircle(sDirX, sDirY, 3, ILI9341_WHITE);
}

#endif
