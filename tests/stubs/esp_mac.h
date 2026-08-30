// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki
//
// Test-only stand-in for ESP-IDF's <esp_mac.h>, used to compile and unit
// test flicker_rand.h's default (non-battery-powered) hardware-seeding path
// off-target, where the real ESP-IDF SDK is unavailable.
//
// Never used by firmware builds. It is only picked up when tests/stubs is
// added to the host compiler's -I search path ahead of the (nonexistent)
// real header, e.g. by tests/test_flicker_rand.cpp. See that file for the
// exact compile invocation.
#pragma once
#include <cstdint>
#include <cstring>

// Hooks that let tests control and observe esp_efuse_mac_get_default() calls.
inline uint8_t *esp_mac_test_bytes() {
  static uint8_t bytes[6] = {0, 0, 0, 0, 0, 0};
  return bytes;
}

inline int &esp_mac_test_calls() {
  static int calls = 0;
  return calls;
}

inline void esp_mac_test_set(const uint8_t mac[6]) {
  std::memcpy(esp_mac_test_bytes(), mac, 6);
}

inline int esp_mac_test_call_count() { return esp_mac_test_calls(); }

// Stand-in for the real esp_efuse_mac_get_default(): copies whatever bytes
// the test set via esp_mac_test_set(), and records that it was called.
inline void esp_efuse_mac_get_default(uint8_t *mac) {
  esp_mac_test_calls()++;
  std::memcpy(mac, esp_mac_test_bytes(), 6);
}
