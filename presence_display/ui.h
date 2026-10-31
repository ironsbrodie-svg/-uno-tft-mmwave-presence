#pragma once

#include <Adafruit_ILI9341.h>
#include <string.h>
#include "cone_view.h"
#include "target.h"

static const int16_t kScreenW = 240;
static const int16_t kBannerH = 46;
static const int16_t kDistY = 46;
static const int16_t kDistH = 52;
static const int16_t kConeY = 98;
static const int16_t kConeH = 122;
static const int16_t kSpeedY = 220;
static const int16_t kRowH = 32;
static const int16_t kEnergyY = 252;

enum BannerKind : uint8_t {
  BANNER_START = 0,
  BANNER_SEARCH,
  BANNER_CONFIG,
  BANNER_PRESENT,
  BANNER_CLEAR,
  BANNER_NOSENSOR,
  BANNER_NORX,
  BANNER_CFGFAIL
};

struct ViewCaps {
  bool providesRange;
  uint16_t maxRangeCm;
  uint8_t beamDeg;
};

static int16_t uiTextWidth(size_t chars, uint8_t size) {
  return (int16_t)chars * 6 * (int16_t)size;
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

static void uiCenterRam(Adafruit_ILI9341 &tft, const char *text, int16_t y,
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

static uint8_t sBanner = 255;

static void uiDrawBanner(Adafruit_ILI9341 &tft, uint8_t kind) {
  if (kind == sBanner) {
    return;
  }
  sBanner = kind;
  uint16_t bg = ILI9341_BLACK;
  uint16_t fg = ILI9341_WHITE;
  const __FlashStringHelper *text = F("STARTING");
  uint8_t size = 4;
  switch (kind) {
    case BANNER_PRESENT:
      bg = ILI9341_GREEN;
      fg = ILI9341_BLACK;
      text = F("PRESENT");
      break;
    case BANNER_CLEAR:
      bg = ILI9341_RED;
      fg = ILI9341_WHITE;
      text = F("CLEAR");
      break;
    case BANNER_SEARCH:
      bg = tft.color565(0, 40, 90);
      text = F("SEARCHING");
      break;
    case BANNER_CONFIG:
      bg = tft.color565(90, 50, 0);
      text = F("CHECKING");
      break;
    case BANNER_NOSENSOR:
      bg = ILI9341_YELLOW;
      fg = ILI9341_BLACK;
      text = F("NO SENSOR");
      break;
    case BANNER_NORX:
      bg = ILI9341_YELLOW;
      fg = ILI9341_BLACK;
      text = F("NO RX");
      break;
    case BANNER_CFGFAIL:
      bg = ILI9341_RED;
      fg = ILI9341_WHITE;
      text = F("CFG FAIL");
      break;
    default:
      break;
  }
  tft.fillRect(0, 0, kScreenW, kBannerH, bg);
  const size_t chars = strlen_P(reinterpret_cast<const char *>(text));
  if (uiTextWidth(chars, 4) > kScreenW - 8) {
    size = 3;
  }
  uiCenterFlash(tft, text, 8, size, fg);
}

static void uiBegin(Adafruit_ILI9341 &tft) {
  sBanner = 255;
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  uiDrawBanner(tft, BANNER_START);
}

static void uiStartup(Adafruit_ILI9341 &tft, uint8_t kind,
                      const __FlashStringHelper *lineA,
                      const __FlashStringHelper *lineB) {
  if (sBanner == kind) {
    return;
  }
  uiDrawBanner(tft, kind);
  tft.fillRect(0, kDistY, kScreenW, 320 - kDistY, ILI9341_BLACK);
  const size_t charsA = strlen_P(reinterpret_cast<const char *>(lineA));
  const uint8_t sizeA = uiTextWidth(charsA, 3) <= (kScreenW - 8) ? 3 : 2;
  uiCenterFlash(tft, lineA, 120, sizeA, ILI9341_WHITE);
  uiCenterFlash(tft, lineB, 160, 2, tft.color565(180, 220, 255));
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

static bool sLive = false;
static bool sConeForce = true;
static char sDist[8] = "";
static char sSpeed[8] = "";
static char sEnergy[8] = "";

static void uiArmRunning(Adafruit_ILI9341 &tft) {
  sLive = true;
  sBanner = 255;
  sDist[0] = 0;
  sSpeed[0] = 0;
  sEnergy[0] = 0;
  sConeForce = true;
  tft.fillRect(0, kDistY, kScreenW, 320 - kDistY, ILI9341_BLACK);
  uiDrawLabels(tft);
}

static void uiPrintTail(Adafruit_ILI9341 &tft, int16_t y, const char *text,
                        char *previous, size_t previousLen, uint16_t color,
                        uint8_t size) {
  if (strcmp(text, previous) == 0) {
    return;
  }
  strncpy(previous, text, previousLen - 1);
  previous[previousLen - 1] = 0;
  tft.setTextSize(size);
  tft.setTextColor(color, ILI9341_BLACK);
  tft.setCursor(78, y + 4);
  tft.print(text);
  const int16_t x = tft.getCursorX();
  if (x < kScreenW) {
    tft.fillRect(x, y + 4, kScreenW - x, (int16_t)(8 * size), ILI9341_BLACK);
  }
}

static void uiRefresh(Adafruit_ILI9341 &tft, const TargetFix &fix,
                      const ViewCaps &caps) {
  if (!sLive) {
    return;
  }
  uiDrawBanner(tft, fix.present ? BANNER_PRESENT : BANNER_CLEAR);

  char dist[8];
  if (fix.present && fix.hasDistance) {
    const uint16_t tenths = (uint16_t)(fix.distanceCm / 10);
    itoa(tenths / 10, dist, 10);
    const uint8_t n = (uint8_t)strlen(dist);
    if (n + 3 < sizeof(dist)) {
      dist[n] = '.';
      dist[n + 1] = (char)('0' + (tenths % 10));
      dist[n + 2] = 'm';
      dist[n + 3] = 0;
    }
  } else if (fix.present) {
    strncpy(dist, "HERE", sizeof(dist) - 1);
    dist[sizeof(dist) - 1] = 0;
  } else {
    strncpy(dist, "--", sizeof(dist) - 1);
    dist[sizeof(dist) - 1] = 0;
  }
  if (strcmp(dist, sDist) != 0) {
    strncpy(sDist, dist, sizeof(sDist) - 1);
    sDist[sizeof(sDist) - 1] = 0;
    tft.fillRect(0, kDistY, kScreenW, kDistH, ILI9341_BLACK);
    uiCenterRam(tft, dist, kDistY + 6, 5,
                fix.present ? ILI9341_YELLOW : ILI9341_WHITE);
  }

  char speed[8];
  if (fix.present && fix.hasSpeed) {
    int v = fix.speedCms / 10;
    char *p = speed;
    if (v < 0) {
      *p++ = '-';
      v = -v;
    }
    itoa(v / 10, p, 10);
    uint8_t n = (uint8_t)strlen(speed);
    if (n + 2 < sizeof(speed)) {
      speed[n] = '.';
      speed[n + 1] = (char)('0' + (v % 10));
      speed[n + 2] = 0;
    }
  } else {
    strncpy(speed, "--", sizeof(speed) - 1);
    speed[sizeof(speed) - 1] = 0;
  }
  uiPrintTail(tft, kSpeedY, speed, sSpeed, sizeof(sSpeed), ILI9341_WHITE, 3);

  char energy[8];
  if (fix.present && fix.hasEnergy) {
    itoa(fix.energy, energy, 10);
  } else {
    strncpy(energy, "--", sizeof(energy) - 1);
    energy[sizeof(energy) - 1] = 0;
  }
  uiPrintTail(tft, kEnergyY, energy, sEnergy, sizeof(sEnergy), ILI9341_WHITE, 3);

  ConeFrame frame;
  frame.x = 0;
  frame.y = kConeY;
  frame.w = kScreenW;
  frame.h = kConeH;
  coneDraw(tft, frame, fix, caps.maxRangeCm, caps.beamDeg, sConeForce);
  sConeForce = false;
  (void)caps.providesRange;
}
