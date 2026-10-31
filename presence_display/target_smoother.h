#pragma once

#include "target.h"

// Handheld filter. A single miss does not wipe the enter score: hits add 2,
// misses subtract 1. PRESENT latches at ENTER_READS * 2 (2 hits with the
// default of 2). During the hold, distance stays and speed/energy are hidden
// so a frozen walking speed is not left on screen.
class TargetSmoother {
 public:
  TargetSmoother(uint16_t holdMs, uint8_t enterReads, uint8_t smoothDen,
                 uint16_t jumpCm)
      : _holdMs(holdMs),
        _need((uint8_t)((enterReads < 1 ? 1 : enterReads) * 2)),
        _den(smoothDen < 2 ? 2 : smoothDen),
        _jumpCm(jumpCm),
        _latched(false),
        _score(0),
        _lastTrueMs(0),
        _haveRange(false),
        _rangeCm(0),
        _jumpHits(0),
        _jumpCmSeen(0),
        _haveSpeed(false),
        _speedCms(0),
        _haveEnergy(false),
        _energy(0),
        _count(0)
#if CONE_DIRECTIONAL
        ,
        _haveAngle(false),
        _angle(0),
        _havePos(false),
        _x(0),
        _y(0)
#endif
  {
  }

  void reset() {
    const uint16_t hold = _holdMs;
    const uint8_t need = _need;
    const uint8_t den = _den;
    const uint16_t jump = _jumpCm;
    *this = TargetSmoother(hold, (uint8_t)(need / 2), den, jump);
  }

  void apply(const TargetFix &in, TargetFix &out) {
    const uint32_t now = millis();
    if (in.present) {
      _lastTrueMs = now;
      if (_score < 12) {
        _score = (uint8_t)(_score + 2);
      }
      if (!_latched && _score >= _need) {
        _latched = true;
      }
      if (_latched) {
        _count = in.count ? in.count : 1;
        if (in.hasDistance) {
          acceptRange(in.distanceCm);
        }
        if (in.hasSpeed) {
          acceptSpeed(in.speedCms);
        }
        if (in.hasEnergy) {
          acceptEnergy(in.energy);
        }
#if CONE_DIRECTIONAL
        if (in.hasAngle) {
          _angle = in.angleDeg;
          _haveAngle = true;
        }
        if (in.hasPosition) {
          _x = in.xCm;
          _y = in.yCm;
          _havePos = true;
        }
#endif
      }
    } else {
      if (_score > 0) {
        _score--;
      }
      const bool coast =
          _latched && (uint32_t)(now - _lastTrueMs) < _holdMs;
      if (coast) {
        _haveSpeed = false;
        _haveEnergy = false;
      } else {
        clearLatch();
      }
    }

    clearTarget(out);
    out.valid = true;
    out.present = _latched;
    out.count = _latched ? _count : 0;
    out.hasDistance = _latched && _haveRange;
    out.distanceCm = _rangeCm;
    out.hasSpeed = _latched && _haveSpeed;
    out.speedCms = _speedCms;
    out.hasEnergy = _latched && _haveEnergy;
    out.energy = _energy;
#if CONE_DIRECTIONAL
    out.hasAngle = _latched && _haveAngle;
    out.angleDeg = _angle;
    out.hasPosition = _latched && _havePos;
    out.xCm = _x;
    out.yCm = _y;
#endif
  }

 private:
  void acceptRange(uint16_t sample) {
    if (sample < 5 || sample > 3000) {
      return;
    }
    if (!_haveRange) {
      _rangeCm = sample;
      _haveRange = true;
      _jumpHits = 0;
      return;
    }
    const uint16_t delta =
        sample > _rangeCm ? (uint16_t)(sample - _rangeCm)
                          : (uint16_t)(_rangeCm - sample);
    if (delta <= _jumpCm) {
      _jumpHits = 0;
      _rangeCm = (uint16_t)((_rangeCm * (uint16_t)(_den - 1) + sample) / _den);
      return;
    }
    const uint16_t jumpDelta =
        sample > _jumpCmSeen ? (uint16_t)(sample - _jumpCmSeen)
                             : (uint16_t)(_jumpCmSeen - sample);
    if (_jumpHits == 0 || jumpDelta > 40) {
      _jumpCmSeen = sample;
      _jumpHits = 1;
      return;
    }
    _jumpHits++;
    if (_jumpHits >= 2) {
      _rangeCm = sample;
      _jumpHits = 0;
    }
  }

  void acceptSpeed(int16_t sample) {
    if (sample > 1500 || sample < -1500) {
      return;
    }
    if (!_haveSpeed) {
      _speedCms = sample;
      _haveSpeed = true;
      return;
    }
    _speedCms = (int16_t)((_speedCms * (int16_t)(_den - 1) + sample) / _den);
  }

  void acceptEnergy(uint16_t sample) {
    if (!_haveEnergy) {
      _energy = sample;
      _haveEnergy = true;
      return;
    }
    _energy = (uint16_t)((_energy * (uint16_t)(_den - 1) + sample) / _den);
  }

  void clearLatch() {
    _latched = false;
    _haveRange = false;
    _haveSpeed = false;
    _haveEnergy = false;
    _count = 0;
    _jumpHits = 0;
    _rangeCm = 0;
    _speedCms = 0;
    _energy = 0;
    _score = 0;
#if CONE_DIRECTIONAL
    _haveAngle = false;
    _havePos = false;
#endif
  }

  uint16_t _holdMs;
  uint8_t _need;
  uint8_t _den;
  uint16_t _jumpCm;
  bool _latched;
  uint8_t _score;
  uint32_t _lastTrueMs;
  bool _haveRange;
  uint16_t _rangeCm;
  uint8_t _jumpHits;
  uint16_t _jumpCmSeen;
  bool _haveSpeed;
  int16_t _speedCms;
  bool _haveEnergy;
  uint16_t _energy;
  uint8_t _count;
#if CONE_DIRECTIONAL
  bool _haveAngle;
  int16_t _angle;
  bool _havePos;
  int16_t _x;
  int16_t _y;
#endif
};
