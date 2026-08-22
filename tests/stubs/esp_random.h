// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Test-only stand-in for ESP-IDF's <esp_random.h>, used to compile and unit
// test flicker_rand.h's default (non-battery-powered) code path off-target,
// where the real ESP-IDF SDK is unavailable.
//
// Never used by firmware builds. It is only picked up when tests/stubs is
// added to the host compiler's -I search path ahead of the (nonexistent)
// real header, e.g. by tests/test_flicker_rand.cpp. See that file for the
// exact compile invocation.
#pragma once
#include <cstdint>

// Hooks that let tests control and observe esp_random() calls.
inline uint32_t &esp_random_test_next_value() {
  static uint32_t value = 0;
  return value;
}

inline int &esp_random_test_calls() {
  static int calls = 0;
  return calls;
}

inline void esp_random_test_set_next(uint32_t value) {
  esp_random_test_next_value() = value;
}

inline int esp_random_test_call_count() { return esp_random_test_calls(); }

// Stand-in for the real esp_random(): returns whatever value the test set
// via esp_random_test_set_next(), and records that it was called.
inline uint32_t esp_random() {
  esp_random_test_calls()++;
  return esp_random_test_next_value();
}
