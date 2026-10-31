#pragma once

#include <Adafruit_ILI9341.h>
#include <stdlib.h>
#include <string.h>
#include "cone_view.h"
#include "target.h"

// Layout for a 240x320 panel held at arm's length.
// Status and distance use the largest font that still fits.

static const int16_t kScreenW = 240;
static const int16_t kBannerH = 46;
static const int16_t kDistY = 46;
static const int16_t kDistH = 52;
static const int16_t kConeY = 98;
static const int16_t kConeH = 122;
static const int16_t kSpeedY = 220;
static const int16_t kRowH = 32;
static const int16_t kEnergyY = 252;
static const int16_t kNoteY = 292;

enum BannerKind : uint8_t {
  BANNER_START = 0,
  BANNER_SEARCH,
  BANNER_CONFIG,
  BANNER_PRESENT,
  BANNER_CLEAR,
  BANNER_NOSENSOR
};

struct ViewCaps {
  bool providesRange;
  bool providesAngle;
  uint16_t maxRangeCm;
  uint8_t beamDeg;
};

static int16_t uiTextWidth(size_t chars, uint8_t size) {
  return (int16_t)chars * 6 * (int16_t)size;
}

static void uiCenter(Adafruit_ILI9341 &tft, const char *text, int16_t y,
                     uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextWrap(false);
  tft.setTextColor(color);
  int16_t x = (int16_t)((kScreenW - uiTextWidth(strlen(text), size)) / 2);
  if (x < 0) {
    x = 0;
  }
  tft.setCursor(x, y);
  tft.print(text);
}

static void uiCenterFlash(Adafruit_ILI9341 &tft, const __FlashStringHelper *text,
                          int16_t y, uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextWrap(false);
  tft.setTextColor(color);
  const size_t chars = strlen_P(reinterpret_cast<const char *>(text));
  int16_t x = (int16_t)((kScreenW - uiTextWidth(chars, size)) / 2);
  if (x < 0) {
    x = 0;
  }
  tft.setCursor(x, y);
  tft.print(text);
}

static uint8_t sBanner = 255;
static bool sLive = false;
static bool sConeForce = true;
static char sDist[12] = "";
static char sSpeed[16] = "";
static char sEnergy[12] = "";
static char sNote[20] = "";

static void uiDrawBanner(Adafruit_ILI9341 &tft, uint8_t kind) {
  if (kind == sBanner) {
    return;
  }
  sBanner = kind;
  uint16_t bg = ILI9341_BLACK;
  uint16_t fg = ILI9341_WHITE;
  const char *text = "STARTING";
  switch (kind) {
    case BANNER_PRESENT:
      bg = ILI9341_GREEN;
      fg = ILI9341_BLACK;
      text = "PRESENT";
      break;
    case BANNER_CLEAR:
      bg = ILI9341_RED;
      fg = ILI9341_WHITE;
      text = "CLEAR";
      break;
    case BANNER_SEARCH:
      bg = tft.color565(0, 40, 90);
      fg = ILI9341_WHITE;
      text = "SEARCHING";
      break;
    case BANNER_CONFIG:
      bg = tft.color565(90, 50, 0);
      fg = ILI9341_WHITE;
      text = "CONFIG";
      break;
    case BANNER_NOSENSOR:
      bg = ILI9341_YELLOW;
      fg = ILI9341_BLACK;
      text = "NO SENSOR";
      break;
    case BANNER_START:
    default:
      bg = tft.color565(20, 20, 20);
      fg = ILI9341_WHITE;
      text = "STARTING";
      break;
  }
  tft.fillRect(0, 0, kScreenW, kBannerH, bg);
  uiCenter(tft, text, 8, 4, fg);
}

static void uiBegin(Adafruit_ILI9341 &tft) {
  sBanner = 255;
  sLive = false;
  sConeForce = true;
  sDist[0] = '\0';
  sSpeed[0] = '\0';
  sEnergy[0] = '\0';
  sNote[0] = '\0';
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  uiDrawBanner(tft, BANNER_START);
}

static void uiStartup(Adafruit_ILI9341 &tft, uint8_t kind,
                      const __FlashStringHelper *lineA,
                      const __FlashStringHelper *lineB) {
  sLive = false;
  uiDrawBanner(tft, kind);
  tft.fillRect(0, kDistY, kScreenW, 320 - kDistY, ILI9341_BLACK);
  uiCenterFlash(tft, lineA, 130, 3, ILI9341_WHITE);
  uiCenterFlash(tft, lineB, 170, 2, tft.color565(180, 220, 255));
}

static void uiDrawLabels(Adafruit_ILI9341 &tft) {
  const uint16_t label = tft.color565(170, 210, 230);
  tft.setTextSize(2);
  tft.setTextColor(label);
  tft.setCursor(4, kSpeedY + 8);
  tft.print(F("SPEED"));
  tft.setCursor(4, kEnergyY + 8);
  tft.print(F("ENERGY"));
}

static void uiArmRunning(Adafruit_ILI9341 &tft) {
  sLive = true;
  sBanner = 255;
  sDist[0] = '\0';
  sSpeed[0] = '\0';
  sEnergy[0] = '\0';
  sNote[0] = '\0';
  sConeForce = true;
  tft.fillRect(0, kDistY, kScreenW, 320 - kDistY, ILI9341_BLACK);
  uiDrawLabels(tft);
}

static void trimLeading(char *text) {
  char *src = text;
  while (*src == ' ') {
    src++;
  }
  if (src != text) {
    memmove(text, src, strlen(src) + 1);
  }
}

static void uiValue(Adafruit_ILI9341 &tft, int16_t y, const char *text,
                    char *previous, size_t previousLen, uint16_t color) {
  if (strcmp(text, previous) == 0) {
    return;
  }
  strncpy(previous, text, previousLen - 1);
  previous[previousLen - 1] = '\0';
  tft.fillRect(78, y, kScreenW - 78, kRowH, ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(color);
  tft.setCursor(78, y + 4);
  tft.print(text);
}

static void uiRefresh(Adafruit_ILI9341 &tft, const TargetFix &fix,
                      const ViewCaps &caps) {
  if (!sLive) {
    return;
  }

  uiDrawBanner(tft, fix.present ? BANNER_PRESENT : BANNER_CLEAR);

  char dist[12];
  if (fix.present && fix.hasDistance) {
    dtostrf(fix.distanceM, 3, 1, dist);
    trimLeading(dist);
    const size_t len = strlen(dist);
    if (len + 3 < sizeof(dist)) {
      dist[len] = ' ';
      dist[len + 1] = 'm';
      dist[len + 2] = '\0';
    }
  } else if (fix.present) {
    strncpy(dist, "IN BEAM", sizeof(dist) - 1);
    dist[sizeof(dist) - 1] = '\0';
  } else {
    strncpy(dist, "--", sizeof(dist) - 1);
    dist[sizeof(dist) - 1] = '\0';
  }

  if (strcmp(dist, sDist) != 0) {
    strncpy(sDist, dist, sizeof(sDist) - 1);
    sDist[sizeof(sDist) - 1] = '\0';
    tft.fillRect(0, kDistY, kScreenW, kDistH, ILI9341_BLACK);
    const uint8_t size = strlen(dist) > 8 ? 3 : 5;
    const int16_t textY = (int16_t)(kDistY + (size == 5 ? 6 : 12));
    uiCenter(tft, dist, textY, size,
             fix.present ? ILI9341_YELLOW : ILI9341_WHITE);
  }

  char speed[16];
  if (fix.present && fix.hasSpeed) {
    dtostrf(fix.speedMps, 3, 1, speed);
    trimLeading(speed);
    strncat(speed, " m/s", sizeof(speed) - strlen(speed) - 1);
  } else {
    strncpy(speed, "--", sizeof(speed) - 1);
    speed[sizeof(speed) - 1] = '\0';
  }
  uiValue(tft, kSpeedY, speed, sSpeed, sizeof(sSpeed), ILI9341_WHITE);

  char energy[12];
  if (fix.present && fix.hasEnergy) {
    ultoa(fix.energy, energy, 10);
  } else {
    strncpy(energy, "--", sizeof(energy) - 1);
    energy[sizeof(energy) - 1] = '\0';
  }
  uiValue(tft, kEnergyY, energy, sEnergy, sizeof(sEnergy), ILI9341_WHITE);

  ConeFrame frame;
  frame.x = 0;
  frame.y = kConeY;
  frame.w = kScreenW;
  frame.h = kConeH;
  coneDraw(tft, frame, fix, caps.providesAngle, caps.maxRangeCm, caps.beamDeg,
           sConeForce);
  sConeForce = false;

  const char *note = caps.providesAngle ? "DIRECTION" : "RANGE ONLY";
  if (strcmp(note, sNote) != 0) {
    strncpy(sNote, note, sizeof(sNote) - 1);
    sNote[sizeof(sNote) - 1] = '\0';
    tft.fillRect(0, kNoteY, kScreenW, 320 - kNoteY, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 210, 60));
    tft.setCursor(8, kNoteY + 2);
    tft.print(note);
  }
}
