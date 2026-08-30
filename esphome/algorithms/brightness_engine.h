// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki
//
// Common interface for a "brightness engine": a self-contained algorithm
// that computes the flame's overall brightness multiplier once per tick.
// Keeping this behind an interface is what lets flicker_1st.yaml and
// toroidal_walk.yaml share the same lean/color/output pipeline
// (flicker_render.h) while swapping only how brightness is generated.
#pragma once

namespace flicker_math {

class BrightnessEngine {
 public:
  virtual ~BrightnessEngine() = default;

  // Advances internal state by one tick and returns the brightness
  // multiplier for this tick, in [0.0, 1.0].
  //
  // `wind` is the 0.4-1.0 turbulence/intensity input (candle_intensity);
  // engines that don't model wind are free to ignore it.
  virtual float update(float wind) = 0;
};

}  // namespace flicker_math
