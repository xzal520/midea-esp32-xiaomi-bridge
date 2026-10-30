#pragma once
#include <stdint.h>
#include <cmath>

// MIoT profiles without AUTO need a UI-compatible heat/cool estimate. This is
// never the physical Midea mode or evidence that its compressor is running.
class MiotAutoModeDisplay {
 public:
  // Reference thresholds from a published Midea split-AC service manual;
  // not verified for every model. Keep the previous display in the neutral
  // band, rather than reporting FAN and hiding the app's temperature control.
  static constexpr float COOL_DELTA = 2.0f;
  static constexpr float HEAT_DELTA = -3.0f;

  void update(bool known, bool power, uint8_t mode, bool roomKnown,
              float room, float target) {
    if (!known || !power) return;
    if (mode == 2) { last_ = 2; return; }
    if (mode == 4) { last_ = 5; return; }
    if (mode != 1 || !roomKnown || !std::isfinite(room) ||
        !std::isfinite(target)) return;
    const float delta = room - target;
    if (delta > COOL_DELTA) last_ = 2;
    else if (delta < HEAT_DELTA) last_ = 5;
    else if (last_ < 0 && delta != 0) last_ = delta > 0 ? 2 : 5;
    // At initial exact balance, or without a valid sensor/history, don't guess.
  }

  int mapped(uint8_t realMode) const {
    return realMode == 1 ? last_ : realMode == 2 ? 2 : realMode == 3 ? 3 :
           realMode == 5 ? 4 : realMode == 4 ? 5 : -1;
  }

 private:
  int last_ = -1;
};
