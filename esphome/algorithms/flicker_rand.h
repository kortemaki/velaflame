// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki
//
// Random sources shared by every flicker effect (flicker_1st.yaml,
// toroidal_walk.yaml) and the wick-pulse timing jitter (velaflame.yaml's
// flicker_loop script).
//
// Two things live here:
//   1. random_unit()  - a [0.0, 1.0) float, used throughout the flicker math.
//   2. XorshiftRng     - the raw integer PRNG backing it, also used directly
//                        by toroidal_walk.h, which needs raw uint32_t draws
//                        for its trajectory and strobe-timing math.
//
// Default (no macro defined): random_unit() draws from a process-lifetime
// XorshiftRng, hardware-seeded once at startup -- NOT from esp_random() on
// every call. This matters because esp_random(), the ESP32 hardware RNG,
// mixes in ADC-sampled RF noise fed by the WiFi/BT radios; per Espressif's
// docs, its entropy pool needs sufficient time *and* radio activity between
// samples to refill. The flicker effect draws ~42 random values/sec on
// average while active (two per 50ms tick for fast jitter + flame lean,
// plus one extra every 10th tick for the slow gust layer -- see
// flicker_1st.yaml). At that rate, calling esp_random() directly on every
// draw can outrun the entropy pool's refill and block waiting for it, which
// stalls the timing loop and shows up as visible stutter in the flicker
// pattern. Seeding a fast xorshift32 generator from a single esp_random()
// draw (mixed with the device's MAC so multiple devices don't produce
// correlated patterns) avoids any per-tick blocking, while remaining
// non-deterministic across boots and across devices.
//
// Define VELAFLAME_BATTERY_POWERED to optimize for battery powered builds.
// Battery-powered builds are expected to keep the WiFi/BT radios off most of
// the time to conserve power, at which point esp_random() offers little
// quality benefit over rand() for this cosmetic effect anyway, and this mode
// avoids any dependency on ESP-IDF's RNG/eFuse APIs. In this mode,
// velaflame.yaml's on_boot handler seeds rand() once from micros() so the
// flicker pattern isn't identical on every power-up. Define the macro via an
// ESPHome/PlatformIO build flag, e.g.:
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
#include <esp_mac.h>
#include <esp_random.h>
#endif

namespace flicker_math {
  // Random draws are reduced modulo this scale, then divided by it to land in
  // [0.0, 1.0). Kept as a power of two so the compiler can turn the modulo
  // into a bitwise AND and the division into an exact multiply-by-reciprocal.
  // That matters here specifically because ESP32-C3 has no hardware FPU, so
  // every float op -- this division included -- is emulated in software.
  constexpr uint32_t kRandomScale = 2 << 9;

// Fast, non-blocking xorshift32 PRNG. See the file header for why the
// default build seeds this once from hardware rather than drawing from
// esp_random() on every call.
class XorshiftRng {
 public:
  // Hardware-seeded by default: mixes one esp_random() draw with the
  // device's MAC (default build), or two rand() draws (battery-powered
  // build, matching that mode's libc-only RNG policy).
  XorshiftRng() { seed_from_hardware(); }

  // Explicit seed, for deterministic sequences (primarily for testing).
  explicit XorshiftRng(uint32_t seed_value) { seed(seed_value); }

  void seed(uint32_t seed_value) {
    // xorshift32 can never escape an all-zero state, so guard against it.
    state_ = seed_value != 0 ? seed_value : 0xDEADBEEFu;
    // Warm up: the first few outputs of a freshly-seeded xorshift32 are
    // more correlated with the seed than later ones.
    for (uint8_t i = 0; i < 4; i++) next();
  }

  // Returns the next raw 32-bit draw.
  uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }

  // Returns a value uniformly distributed in [0.0, 1.0).
  float next_unit() {
    return static_cast<float>(next() % kRandomScale) / static_cast<float>(kRandomScale);
  }

 private:
  void seed_from_hardware() {
#if defined(VELAFLAME_BATTERY_POWERED)
    uint32_t hi = static_cast<uint32_t>(rand());
    uint32_t lo = static_cast<uint32_t>(rand());
    seed((hi << 16) ^ lo);
#else
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    esp_efuse_mac_get_default(mac);
    uint32_t mac_hash = (static_cast<uint32_t>(mac[0]) << 24) |
                         (static_cast<uint32_t>(mac[1]) << 16) |
                         (static_cast<uint32_t>(mac[2]) << 8) |
                         mac[3];
    mac_hash ^= (static_cast<uint32_t>(mac[4]) << 8) | mac[5];
    seed(mac_hash ^ esp_random());
#endif
  }

  uint32_t state_ = 0xDEADBEEFu;
};

// Returns a random value uniformly distributed in [0.0, 1.0).
inline float random_unit() {
  static XorshiftRng rng;
  return rng.next_unit();
}

}  // namespace flicker_math
