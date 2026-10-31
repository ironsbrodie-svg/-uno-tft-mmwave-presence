#pragma once

#include "target.h"

// Handheld filter. The C4001 expects a fixed mount, so walking makes
// still objects look like motion. This holds presence briefly, requires
// a couple of hits before the banner flips on, and eases the range number
// so the big digits do not flicker.
class TargetSmoother {
 public:
  TargetSmoother(uint16_t holdMs, uint8_t enterReads, uint8_t smoothDen,
                 float jumpM)
      : _holdMs(holdMs),
        _enterReads(enterReads < 1 ? 1 : enterReads),
        _den(smoothDen < 2 ? 2 : smoothDen),
        _jumpM(jumpM),
        _latched(false),
        _hits(0),
        _lastTrueMs(0),
        _haveRange(false),
        _rangeM(0),
        _jumpHits(0),
        _jumpRange(0),
        _haveSpeed(false),
        _speed(0),
        _haveEnergy(false),
        _energy(0),
        _haveAngle(false),
        _angle(0),
        _havePos(false),
        _x(0),
        _y(0),
        _count(0) {}

  void reset() {
    const uint16_t hold = _holdMs;
    const uint8_t enter = _enterReads;
    const uint8_t den = _den;
    const float jump = _jumpM;
    *this = TargetSmoother(hold, enter, den, jump);
  }

  void apply(const TargetFix &in, TargetFix &out) {
    const uint32_t now = millis();

    if (in.present) {
      _lastTrueMs = now;
      if (!_latched) {
        if (_hits < 255) {
          _hits++;
        }
        if (_hits >= _enterReads) {
          _latched = true;
        }
      }
      if (_latched) {
        if (in.count > 0) {
          _count = in.count;
        } else {
          _count = 1;
        }
        if (in.hasDistance) {
          acceptRange(in.distanceM);
        }
        if (in.hasSpeed) {
          acceptSpeed(in.speedMps);
        }
        if (in.hasEnergy) {
          acceptEnergy(in.energy);
        }
        if (in.hasAngle) {
          acceptAngle(in.angleDeg);
        }
        if (in.hasPosition) {
          acceptPosition(in.xM, in.yM);
        }
      }
    } else {
      _hits = 0;
      const bool coasting =
          _latched && (uint32_t)(now - _lastTrueMs) < _holdMs;
      if (!coasting) {
        clearLatch();
      }
    }

    clearTarget(out);
    out.valid = true;
    out.present = _latched;
    out.count = _latched ? _count : 0;
    out.hasDistance = _latched && _haveRange;
    out.distanceM = _rangeM;
    out.hasSpeed = _latched && _haveSpeed;
    out.speedMps = _speed;
    out.hasEnergy = _latched && _haveEnergy;
    out.energy = _energy;
    out.hasAngle = _latched && _haveAngle;
    out.angleDeg = _angle;
    out.hasPosition = _latched && _havePos;
    out.xM = _x;
    out.yM = _y;
  }

 private:
  static float blend(float previous, float sample, uint8_t den) {
    return (previous * (float)(den - 1) + sample) / (float)den;
  }

  static float magnitude(float value) { return value < 0.0f ? -value : value; }

  void acceptRange(float sample) {
    if (sample < 0.05f || sample > 30.0f) {
      return;
    }
    if (!_haveRange) {
      _rangeM = sample;
      _haveRange = true;
      _jumpHits = 0;
      return;
    }
    const float delta = magnitude(sample - _rangeM);
    if (delta <= _jumpM) {
      _jumpHits = 0;
      _rangeM = blend(_rangeM, sample, _den);
      return;
    }
    // A single wild jump is ignored. Two samples that agree replace it.
    if (_jumpHits == 0 || magnitude(sample - _jumpRange) > 0.4f) {
      _jumpRange = sample;
      _jumpHits = 1;
      return;
    }
    _jumpHits++;
    if (_jumpHits >= 2) {
      _rangeM = sample;
      _jumpHits = 0;
    }
  }

  void acceptSpeed(float sample) {
    if (magnitude(sample) > 15.0f) {
      return;
    }
    if (!_haveSpeed) {
      _speed = sample;
      _haveSpeed = true;
      return;
    }
    _speed = blend(_speed, sample, _den);
  }

  void acceptEnergy(uint32_t sample) {
    if (!_haveEnergy) {
      _energy = sample;
      _haveEnergy = true;
      return;
    }
    _energy = (_energy * (uint32_t)(_den - 1) + sample) / _den;
  }

  // Fine for small swings. A wrap around +/-180 is not handled; the C4001
  // never reports an angle, and a later driver can replace this.
  void acceptAngle(float sample) {
    if (!_haveAngle) {
      _angle = sample;
      _haveAngle = true;
      return;
    }
    _angle = blend(_angle, sample, _den);
  }

  void acceptPosition(float x, float y) {
    if (!_havePos) {
      _x = x;
      _y = y;
      _havePos = true;
      return;
    }
    _x = blend(_x, x, _den);
    _y = blend(_y, y, _den);
  }

  void clearLatch() {
    _latched = false;
    _haveRange = false;
    _haveSpeed = false;
    _haveEnergy = false;
    _haveAngle = false;
    _havePos = false;
    _count = 0;
    _jumpHits = 0;
    _rangeM = 0;
    _speed = 0;
    _energy = 0;
  }

  uint16_t _holdMs;
  uint8_t _enterReads;
  uint8_t _den;
  float _jumpM;
  bool _latched;
  uint8_t _hits;
  uint32_t _lastTrueMs;
  bool _haveRange;
  float _rangeM;
  uint8_t _jumpHits;
  float _jumpRange;
  bool _haveSpeed;
  float _speed;
  bool _haveEnergy;
  uint32_t _energy;
  bool _haveAngle;
  float _angle;
  bool _havePos;
  float _x;
  float _y;
  uint8_t _count;
};
