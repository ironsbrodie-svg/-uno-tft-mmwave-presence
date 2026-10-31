#pragma once

#include <Adafruit_ILI9341.h>
#include "target.h"

#if CONE_DIRECTIONAL
#include "future/directional_view.h"
#endif

// Top-down beam. The sensor sits at the bottom and looks up the screen.
// The C4001 has no bearing, so a target is a band across the whole beam.
// Rings and rays are redrawn only where the band was erased, not by
// blanking the whole cone.

struct ConeFrame {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

static const int16_t kConeR = 96;

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
  static const int16_t kTable[10] = {0,   44,  88,  128, 165,
                                     196, 222, 241, 252, 256};
  const int idx = deg / 10;
  const int frac = deg % 10;
  const int16_t a = kTable[idx];
  const int16_t b = kTable[idx < 9 ? idx + 1 : 9];
  return (int16_t)(sign * (a + (int16_t)((b - a) * frac / 10)));
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
  for (int deg = fromDeg; deg <= toDeg; deg += 4) {
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
  if (started && prevX != 0) {
    int16_t endX = 0;
    int16_t endY = 0;
    conePolar(apexX, apexY, radius, toDeg, endX, endY);
    if (endX != prevX || endY != prevY) {
      tft.drawLine(prevX, prevY, endX, endY, color);
    }
  }
}

static void coneBand(Adafruit_ILI9341 &tft, int16_t apexX, int16_t apexY,
                     int16_t radius, int half, uint16_t color) {
  for (int8_t extra = -2; extra <= 2; extra = (int8_t)(extra + 2)) {
    const int16_t r = (int16_t)(radius + extra);
    if (r > 1) {
      coneStroke(tft, apexX, apexY, r, -half, half, color);
    }
  }
}

static void coneStatic(Adafruit_ILI9341 &tft, int16_t apexX, int16_t apexY,
                       int half, uint16_t ring, uint16_t ray) {
  coneStroke(tft, apexX, apexY, kConeR, -half, half, ring);
  coneStroke(tft, apexX, apexY, (int16_t)(kConeR / 3), -half, half, ring);
  coneStroke(tft, apexX, apexY, (int16_t)((kConeR * 2) / 3), -half, half, ring);
  int16_t x = 0;
  int16_t y = 0;
  conePolar(apexX, apexY, kConeR, -half, x, y);
  tft.drawLine(apexX, apexY, x, y, ray);
  conePolar(apexX, apexY, kConeR, half, x, y);
  tft.drawLine(apexX, apexY, x, y, ray);
  tft.fillCircle(apexX, apexY, 3, ILI9341_WHITE);
}

static void coneDraw(Adafruit_ILI9341 &tft, const ConeFrame &frame,
                     const TargetFix &fix, uint16_t maxRangeCm, uint8_t beamDeg,
                     bool force) {
  int half = (int)beamDeg / 2;
  if (half < 15) {
    half = 15;
  }
  if (half > 70) {
    half = 70;
  }
  if (maxRangeCm < 50) {
    maxRangeCm = 50;
  }

  const bool band = fix.present && fix.hasDistance;
  const bool inBeam = fix.present && !fix.hasDistance;
  int16_t radius = 0;
  if (band) {
    uint16_t cm = fix.distanceCm;
    if (cm > maxRangeCm) {
      cm = maxRangeCm;
    }
    radius = (int16_t)(((int32_t)cm * kConeR) / maxRangeCm);
    if (radius < 4) {
      radius = 4;
    }
  } else if (inBeam) {
    radius = (int16_t)(kConeR / 2);
  }

  static int16_t sRadius = -1;
  static uint8_t sKind = 255;
  static uint16_t sMax = 0;
  static uint8_t sBeam = 0;
  static bool sLabel = false;
  const uint8_t kind = band ? 1 : (inBeam ? 3 : 0);
  if (!force && kind == sKind && radius == sRadius && maxRangeCm == sMax &&
      beamDeg == sBeam) {
    return;
  }

  const int16_t apexX = (int16_t)(frame.x + frame.w / 2);
  const int16_t apexY = (int16_t)(frame.y + frame.h - 6);
  const uint16_t ring = tft.color565(120, 160, 180);
  const uint16_t ray = ILI9341_WHITE;

  if (force || !sLabel || maxRangeCm != sMax || beamDeg != sBeam) {
    tft.fillRect(frame.x, frame.y, frame.w, frame.h, ILI9341_BLACK);
    coneStatic(tft, apexX, apexY, half, ring, ray);
    char digits[6];
    itoa(maxRangeCm / 100, digits, 10);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor((int16_t)(frame.x + frame.w - 36), (int16_t)(apexY - 18));
    tft.print(digits);
    tft.print('m');
    sLabel = true;
  } else {
#if CONE_DIRECTIONAL
    directionErase(tft);
#endif
    if (sRadius > 1) {
      coneBand(tft, apexX, apexY, sRadius, half, ILI9341_BLACK);
    }
    const int16_t rings[3] = {(int16_t)(kConeR / 3),
                              (int16_t)((kConeR * 2) / 3), kConeR};
    for (uint8_t i = 0; i < 3; i++) {
      const int16_t rr = rings[i];
      const bool nearOld = sRadius > 1 && abs(rr - sRadius) <= 4;
      const bool nearNew = radius > 1 && abs(rr - radius) <= 4;
      if (nearOld || nearNew) {
        coneStroke(tft, apexX, apexY, rr, -half, half, ring);
      }
    }
    int16_t x = 0;
    int16_t y = 0;
    conePolar(apexX, apexY, kConeR, -half, x, y);
    tft.drawLine(apexX, apexY, x, y, ray);
    conePolar(apexX, apexY, kConeR, half, x, y);
    tft.drawLine(apexX, apexY, x, y, ray);
    tft.fillCircle(apexX, apexY, 3, ILI9341_WHITE);
  }

  if (band) {
    coneBand(tft, apexX, apexY, radius, half, ILI9341_YELLOW);
  } else if (inBeam) {
    coneStroke(tft, apexX, apexY, radius, -half, half, ILI9341_GREEN);
  }
#if CONE_DIRECTIONAL
  directionDraw(tft, apexX, apexY, half, kConeR, maxRangeCm, fix);
#endif

  sRadius = (band || inBeam) ? radius : (int16_t)-1;
  sKind = kind;
  sMax = maxRangeCm;
  sBeam = beamDeg;
}
