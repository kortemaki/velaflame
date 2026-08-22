// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Random source for the candle flicker effect (flicker_1st.yaml).
//
// The flicker effect draws ~42 random values/sec on average while active
// (two per 50ms tick for fast jitter + flame lean, plus one extra every
// 10th tick for the slow gust layer — see flicker_1st.yaml). That rate is
// trivial for either random source below; the choice here is about entropy
// quality and power behavior, not throughput.
//
// Default (no macro defined): esp_random(), the ESP32 hardware RNG. It is
// fed by RF noise from the WiFi/BT radios when active, falling back to an
// internal asynchronous timer otherwise. This is a much higher-quality,
// non-deterministic source than libc's rand(), which on newlib is a simple,
// low-period, unseeded LCG that produces the exact same sequence every boot.
//
// Define VELAFLAME_BATTERY_POWERED to use rand() instead. Battery-powered
// builds are expected to keep the WiFi/BT radios off most of the time to
// conserve power, so esp_random()'s entropy quality degrades to its internal
// timer fallback anyway; rand()'s lower-quality sequence is visually
// indistinguishable for this cosmetic effect. Define the macro via an
// ESPHome/PlatformIO build flag, e.g.:
//
//   esphome:
//     platformio_options:
//       build_flags:
//         - -DVELAFLAME_BATTERY_POWERED
#pragma once

#if defined(VELAFLAME_BATTERY_POWERED)
#include <cstdlib>
#else
#include <esp_random.h>
#endif

namespace flicker_math {

// Returns a random value uniformly distributed in [0.0, 1.0).
inline float random_unit() {
#if defined(VELAFLAME_BATTERY_POWERED)
  return static_cast<float>(rand() % 1000) / 1000.0f;
#else
  return static_cast<float>(esp_random() % 1000) / 1000.0f;
#endif
}

}  // namespace flicker_math
