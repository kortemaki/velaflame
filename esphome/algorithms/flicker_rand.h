// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Random source shared by the candle flicker effect (flicker_1st.yaml) and
// the wick-pulse timing jitter (velaflame.yaml's flicker_loop script).
//
// The flicker effect draws ~42 random values/sec on average while active
// (two per 50ms tick for fast jitter + flame lean, plus one extra every
// 10th tick for the slow gust layer — see flicker_1st.yaml). The wick-pulse
// timing draws one value per pulse cycle (on the order of once every few
// hundred ms). Both rates are trivial for either random source below; the
// choice here is about entropy quality and power behavior, not throughput.
//
// Default (no macro defined): esp_random(), the ESP32 hardware RNG. Per
// Espressif's docs, it produces true random numbers when the WiFi/BT radios
// are active (they feed it ADC-sampled RF noise); otherwise its output
// should be treated as pseudo-random, based only on a secondary internal
// oscillator entropy source. Either way, it's still a higher-quality,
// non-deterministic source than libc's rand(), which on newlib is a simple,
// low-period LCG that -- unless explicitly seeded -- produces the exact
// same sequence every boot.
//
// Define VELAFLAME_BATTERY_POWERED to use rand() instead. Battery-powered
// builds are expected to keep the WiFi/BT radios off most of the time to
// conserve power, at which point esp_random() offers little quality benefit
// over rand() for this cosmetic effect anyway. In this mode, velaflame.yaml's
// on_boot handler seeds rand() once from micros() so the flicker pattern
// isn't identical on every power-up. Define the macro via an ESPHome/
// PlatformIO build flag, e.g.:
//
//   esphome:
//     platformio_options:
//       build_flags:
//         - -DVELAFLAME_BATTERY_POWERED
#pragma once

#include <cstdint>

#if defined(VELAFLAME_BATTERY_POWERED)
#include <cstdlib>
#else
#include <esp_random.h>
#endif
namespace flicker_math {
  // Random draws are reduced modulo this scale, then divided by it to land in
  // [0.0, 1.0). Kept as a power of two so the compiler can turn the modulo
  // into a bitwise AND and the division into an exact multiply-by-reciprocal.
  // That matters here specifically because ESP32-C3 has no hardware FPU, so
  // every float op -- this division included -- is emulated in software.
  constexpr uint32_t kRandomScale = 2 << 9;

// Returns a random value uniformly distributed in [0.0, 1.0).
inline float random_unit() {
  #if defined(VELAFLAME_BATTERY_POWERED)
  // rand() returns a non-negative int by contract; cast to unsigned so the
  // modulo-by-power-of-two -> bitwise AND optimization applies here too.
  uint32_t draw = static_cast<uint32_t>(rand());
  #else
  uint32_t draw = esp_random();
  #endif
  return static_cast<float>(draw % kRandomScale) / static_cast<float>(kRandomScale);
}

}  // namespace flicker_math
