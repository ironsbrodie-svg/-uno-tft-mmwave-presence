#pragma once

#include <Adafruit_ILI9341.h>
#include <stdlib.h>
#include <string.h>
#include "target.h"

// Top-down beam. The sensor sits at the bottom and looks up the screen.
// With no angle (C4001) the target is a band across the whole beam at that
// distance. With hasAngle or hasPosition (a future sensor) it is a dot.

struct ConeFrame {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

static int16_t coneSin256(int deg) {
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
  // Sine * 256 at 0, 10, ... 90 degrees.
  static const int16_t kTable[10] = {0,   44,  88,  128, 165,
                                     196, 222, 241, 252, 256};
  const int idx = deg / 10;
  const int frac = deg % 10;
  const int16_t a = kTable[idx];
  const int16_t b = kTable[idx < 9 ? idx + 1 : 9];
  const int16_t v = (int16_t)(a + (int16_t)((b - a) * frac / 10));
  return (int16_t)(sign * v);
}

static int16_t coneCos256(int deg) { return coneSin256(deg + 90); }

static void conePolar(int16_t apexX, int16_t apexY, int16_t radius, int deg,
                      int16_t &outX, int16_t &outY) {
  outX = (int16_t)(apexX + (int16_t)((int32_t)radius * coneSin256(deg) / 256));
  outY = (int16_t)(apexY - (int16_t)((int32_t)radius * coneCos256(deg) / 256));
}

static void coneStroke(Adafruit_ILI9341 &tft, int16_t apexX, int16_t apexY,
                       int16_t radius, int fromDeg, int toDeg, uint16_t color) {
  if (radius < 2) {
    return;
  }
  int16_t prevX = 0;
  int16_t prevY = 0;
  bool started = false;
  for (int deg = fromDeg; deg <= toDeg; deg += 3) {
    int16_t x = 0;
    int16_t y = 0;
    conePolar(apexX, apexY, radius, deg, x, y);
    if (started) {
      tft.drawLine(prevX, prevY, x, y, color);
    }
    prevX = x;
    prevY = y;
    started = true;
  }
}

// Rough degrees, 0 = ahead (+y), positive = right (+x). Display only.
static int16_t coneBearingDeg(float xM, float yM) {
  const int32_t x = (int32_t)(xM * 100.0f);
  const int32_t y = (int32_t)(yM * 100.0f);
  if (x == 0 && y == 0) {
    return 0;
  }
  const int32_t ax = x < 0 ? -x : x;
  const int32_t ay = y < 0 ? -y : y;
  const int16_t a = (int16_t)((ay * 90) / (ax + ay));
  int16_t fromAhead = (int16_t)(90 - a);
  if (y < 0) {
    fromAhead = (int16_t)(180 - fromAhead);
  }
  if (x < 0) {
    fromAhead = (int16_t)(-fromAhead);
  }
  return fromAhead;
}

static void coneDraw(Adafruit_ILI9341 &tft, const ConeFrame &frame,
                     const TargetFix &fix, bool providesAngle,
                     uint16_t maxRangeCm, uint8_t beamDeg, bool force) {
  int half = (int)beamDeg / 2;
  if (half < 15) {
    half = 15;
  }
  if (half > 70) {
    half = 70;
  }

  const bool directional =
      fix.present && (fix.hasAngle || fix.hasPosition);
  const bool band = fix.present && fix.hasDistance && !directional;
  const bool inBeamOnly = fix.present && !fix.hasDistance && !directional;

  int16_t ang = 0;
  if (fix.hasAngle) {
    ang = (int16_t)fix.angleDeg;
  } else if (fix.hasPosition) {
    ang = coneBearingDeg(fix.xM, fix.yM);
  }

  float distM = 0.0f;
  if (fix.hasDistance) {
    distM = fix.distanceM;
  } else if (fix.hasPosition) {
    const float xm = fix.xM;
    const float ym = fix.yM;
    // Integer length in centimeters, then back to meters.
    const int32_t xcm = (int32_t)(xm * 100.0f);
    const int32_t ycm = (int32_t)(ym * 100.0f);
    uint32_t mag = (uint32_t)(xcm * xcm + ycm * ycm);
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
    distM = (float)root / 100.0f;
  }

  const int16_t distQ =
      fix.present && (fix.hasDistance || fix.hasPosition)
          ? (int16_t)(distM * 5.0f + 0.5f)
          : (int16_t)-1;
  const uint8_t kind = directional ? 2 : (band ? 1 : (inBeamOnly ? 3 : 0));

  static int16_t sDistQ = -32000;
  static int16_t sAngQ = -32000;
  static uint8_t sKind = 255;
  static uint16_t sMaxCm = 0;
  static uint8_t sBeam = 0;
  const int16_t angQ = (int16_t)(ang / 3);

  if (!force && kind == sKind && distQ == sDistQ && angQ == sAngQ &&
      maxRangeCm == sMaxCm && beamDeg == sBeam) {
    return;
  }
  sKind = kind;
  sDistQ = distQ;
  sAngQ = angQ;
  sMaxCm = maxRangeCm;
  sBeam = beamDeg;

  tft.fillRect(frame.x, frame.y, frame.w, frame.h, ILI9341_BLACK);

  const int16_t apexX = (int16_t)(frame.x + frame.w / 2);
  const int16_t apexY = (int16_t)(frame.y + frame.h - 8);
  const int16_t radiusMax = 88;
  const uint16_t dim = tft.color565(70, 110, 130);
  const uint16_t ray = tft.color565(190, 220, 235);

  coneStroke(tft, apexX, apexY, radiusMax, -half, half, dim);
  coneStroke(tft, apexX, apexY, (int16_t)(radiusMax / 3), -half, half, dim);
  coneStroke(tft, apexX, apexY, (int16_t)((radiusMax * 2) / 3), -half, half,
             dim);

  int16_t leftX = 0;
  int16_t leftY = 0;
  int16_t rightX = 0;
  int16_t rightY = 0;
  conePolar(apexX, apexY, radiusMax, -half, leftX, leftY);
  conePolar(apexX, apexY, radiusMax, half, rightX, rightY);
  tft.drawLine(apexX, apexY, leftX, leftY, ray);
  tft.drawLine(apexX, apexY, rightX, rightY, ray);
  tft.fillCircle(apexX, apexY, 3, ILI9341_WHITE);

  float maxM = (float)maxRangeCm / 100.0f;
  if (maxM < 0.5f) {
    maxM = 0.5f;
  }
  char scale[8];
  dtostrf(maxM, 2, 0, scale);
  char *scalePtr = scale;
  while (*scalePtr == ' ') {
    scalePtr++;
  }
  tft.setTextSize(2);
  tft.setTextColor(dim);
  tft.setCursor((int16_t)(frame.x + frame.w - 36), (int16_t)(apexY - 20));
  tft.print(scalePtr);
  tft.print('m');

  if (fix.present && (fix.hasDistance || fix.hasPosition) && distM > 0.0f) {
    float frac = distM / maxM;
    if (frac < 0.0f) {
      frac = 0.0f;
    }
    if (frac > 1.0f) {
      frac = 1.0f;
    }
    int16_t radius = (int16_t)(frac * (float)radiusMax + 0.5f);
    if (radius < 12) {
      radius = 12;
    }
    if (band || directional) {
      const uint16_t bandColor =
          directional ? tft.color565(90, 70, 20) : ILI9341_YELLOW;
      for (int8_t extra = -2; extra <= 4; extra = (int8_t)(extra + 2)) {
        coneStroke(tft, apexX, apexY, (int16_t)(radius + extra), -half, half,
                   bandColor);
      }
    }
    if (directional) {
      int drawAng = ang;
      if (drawAng > half) {
        drawAng = half;
      }
      if (drawAng < -half) {
        drawAng = -half;
      }
      int16_t dotX = 0;
      int16_t dotY = 0;
      conePolar(apexX, apexY, radius, drawAng, dotX, dotY);
      tft.fillCircle(dotX, dotY, 7, ILI9341_YELLOW);
      tft.fillCircle(dotX, dotY, 3, ILI9341_WHITE);
    }
  } else if (inBeamOnly) {
    coneStroke(tft, apexX, apexY, (int16_t)(radiusMax / 2), -half, half,
               ILI9341_GREEN);
  }

  char caption[22];
  caption[0] = '\0';
  if (!fix.present) {
    strncpy(caption, "beam empty", sizeof(caption));
  } else if (directional) {
    char num[8];
    dtostrf(distM, 3, 1, num);
    char *numPtr = num;
    while (*numPtr == ' ') {
      numPtr++;
    }
    strncpy(caption, numPtr, sizeof(caption) - 1);
    caption[sizeof(caption) - 1] = '\0';
    const size_t len = strlen(caption);
    if (len + 6 < sizeof(caption)) {
      caption[len] = 'm';
      caption[len + 1] = ' ';
      caption[len + 2] = '\0';
    }
    if (ang > 8) {
      strncat(caption, "right", sizeof(caption) - strlen(caption) - 1);
    } else if (ang < -8) {
      strncat(caption, "left", sizeof(caption) - strlen(caption) - 1);
    } else {
      strncat(caption, "ahead", sizeof(caption) - strlen(caption) - 1);
    }
  } else if (band) {
    char num[8];
    dtostrf(distM, 3, 1, num);
    char *numPtr = num;
    while (*numPtr == ' ') {
      numPtr++;
    }
    strncpy(caption, numPtr, sizeof(caption) - 1);
    caption[sizeof(caption) - 1] = '\0';
    strncat(caption, "m full beam", sizeof(caption) - strlen(caption) - 1);
  } else {
    strncpy(caption, "in the beam", sizeof(caption));
  }
  caption[sizeof(caption) - 1] = '\0';

  tft.setTextSize(2);
  tft.setTextColor(providesAngle ? ILI9341_WHITE : ILI9341_YELLOW);
  tft.setCursor(frame.x + 4, frame.y + 2);
  tft.print(caption);
}
