// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// The "1st flicker" brightness engine: a slow gust layer that retargets
// every ~500ms with a cubic distribution for rare deep dips, plus a fast
// jitter layer that adds turbulence every tick. Extracted from
// flicker_1st.yaml so it can be swapped for alternate BrightnessEngine
// implementations (e.g. toroidal_walk.h) via flicker_render.h.
#pragma once
#include "brightness_engine.h"
#include "flicker_math.h"
#include "flicker_rand.h"

namespace flicker_math {

class GustBrightnessEngine : public BrightnessEngine {
 public:
  float update(float wind) override {
    // Slow gust layer: retarget every ~10 ticks (~500ms at the 50ms tick
    // rate flicker_1st.yaml runs at).
    slow_timer_++;
    if (slow_timer_ >= 10) {
      slow_timer_ = 0;
      float r = random_unit();
      r = r * r * r;  // cubic: mostly near 1.0, rare dramatic dips
      slow_target_ = 1.0f - r * (wind * 0.35f);
    }
    // Asymmetric smoothing: slow to dim, fast to recover.
    float alpha = (slow_b_ > slow_target_) ? (0.08f + wind * 0.12f) : 0.30f;
    slow_b_ = slow_b_ * (1.0f - alpha) + slow_target_ * alpha;

    // Fast jitter layer: small turbulence every tick.
    float fast_noise = random_unit();
    float fast_target = 1.0f - fast_noise * (wind * 0.06f);
    fast_b_ = fast_b_ * 0.65f + fast_target * 0.35f;

    return clamp_brightness(slow_b_ * fast_b_);
  }

 private:
  float slow_b_ = 1.0f;
  float fast_b_ = 1.0f;
  float slow_target_ = 1.0f;
  int slow_timer_ = 0;
};

}  // namespace flicker_math
