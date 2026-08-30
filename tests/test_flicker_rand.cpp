// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki
//
// Off-target unit tests for flicker_rand.h. These verify that
// flicker_math::random_unit() and flicker_math::XorshiftRng import and call
// the correct underlying random source depending on whether
// VELAFLAME_BATTERY_POWERED is defined, by compiling this file twice, once
// per mode.
//
// Build & run:
//   Windows (MSVC Developer Command Prompt):
//     REM Default (hardware-seeded XorshiftRng) mode -- needs the test-only
//     REM stubs, since the real ESP-IDF headers aren't available off-target:
//     cl /std:c++14 /EHsc /I..\esphome\algorithms /Istubs ^
//       test_flicker_rand.cpp /Fe:test_flicker_rand_default.exe
//     test_flicker_rand_default.exe
//
//     REM Battery-powered (rand()) mode -- deliberately built WITHOUT
//     REM /Istubs, so a successful compile also proves this mode never
//     REM references esp_random() or esp_efuse_mac_get_default():
//     cl /std:c++14 /EHsc /I..\esphome\algorithms /DVELAFLAME_BATTERY_POWERED ^
//       test_flicker_rand.cpp /Fe:test_flicker_rand_battery.exe
//     test_flicker_rand_battery.exe
//
//   Linux / macOS:
//     g++ -std=c++14 -I../esphome/algorithms -Istubs \
//       test_flicker_rand.cpp -o test_flicker_rand_default
//     ./test_flicker_rand_default
//
//     g++ -std=c++14 -I../esphome/algorithms -DVELAFLAME_BATTERY_POWERED \
//       test_flicker_rand.cpp -o test_flicker_rand_battery
//     ./test_flicker_rand_battery

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#ifdef VELAFLAME_BATTERY_POWERED
#include <cstdlib>
#else
// Test-only stubs; resolve to tests/stubs/esp_random.h and
// tests/stubs/esp_mac.h via -Istubs. Not available off-target otherwise, so
// building this mode without -Istubs would fail to compile -- which is
// itself proof that the battery-powered mode below never references them.
#include <esp_mac.h>
#include <esp_random.h>
#endif

#include "flicker_rand.h"

using flicker_math::XorshiftRng;

// ---------------------------------------------------------------------------
// XorshiftRng: behavior shared by both build modes.
// ---------------------------------------------------------------------------

TEST_CASE("XorshiftRng: explicit seed is deterministic") {
    XorshiftRng a(12345);
    XorshiftRng b(12345);
    for (int i = 0; i < 20; i++) {
        CHECK(a.next() == b.next());
    }
}

TEST_CASE("XorshiftRng: different seeds diverge") {
    XorshiftRng a(1);
    XorshiftRng b(2);
    bool any_different = false;
    for (int i = 0; i < 20; i++) {
        if (a.next() != b.next()) any_different = true;
    }
    CHECK(any_different);
}

TEST_CASE("XorshiftRng: zero seed does not stick at zero") {
    XorshiftRng rng(0);
    for (int i = 0; i < 20; i++) {
        CHECK(rng.next() != 0);
    }
}

TEST_CASE("XorshiftRng: next_unit stays in [0, 1)") {
    XorshiftRng rng(42);
    for (int i = 0; i < 500; i++) {
        float v = rng.next_unit();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

TEST_CASE("XorshiftRng: successive draws are not constant") {
    XorshiftRng rng(7);
    uint32_t first = rng.next();
    uint32_t second = rng.next();
    CHECK(first != second);
}

#ifdef VELAFLAME_BATTERY_POWERED

// ---------------------------------------------------------------------------
// VELAFLAME_BATTERY_POWERED defined: random_unit() seeds XorshiftRng
// using rand() instead of esp_random()/MAC.
// ---------------------------------------------------------------------------

TEST_CASE("random_unit: battery-powered mode advances between calls") {
    srand(99);
    float first = flicker_math::random_unit();
    float second = flicker_math::random_unit();
    // Extremely unlikely to collide if each call is really advancing the
    // underlying XorshiftRng rather than returning a cached/constant value.
    CHECK(first != doctest::Approx(second));
}

TEST_CASE("random_unit: battery-powered mode result stays in [0, 1)") {
    srand(42);
    for (int i = 0; i < 200; i++) {
        float v = flicker_math::random_unit();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

TEST_CASE("XorshiftRng: battery-powered default constructor never touches esp_random()") {
    // No esp_random()/esp_efuse_mac_get_default stub is linked into this
    // build mode at all -- a successful compile of this whole file already
    // proves the default constructor doesn't reference them. This just
    // exercises that rand()-seeded path.
    srand(2026);
    XorshiftRng rng;
    CHECK(rng.next() != 0);
}

#else

// ---------------------------------------------------------------------------
// VELAFLAME_BATTERY_POWERED undefined: random_unit() must use a
// hardware-seeded XorshiftRng, not esp_random() directly on every draw.
// ---------------------------------------------------------------------------

TEST_CASE("random_unit: default mode result stays in [0, 1)") {
    for (int i = 0; i < 200; i++) {
        float v = flicker_math::random_unit();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

TEST_CASE("random_unit: default mode does not call esp_random() on every draw") {
    // The shared random_unit() instance may already have been seeded by an
    // earlier test case in this binary; regardless, drawing many values in a
    // row must not grow the esp_random() call count by one-per-draw -- at
    // most one more call (if this happens to be the very first draw ever).
    int calls_before = esp_random_test_call_count();
    for (int i = 0; i < 50; i++) flicker_math::random_unit();
    CHECK(esp_random_test_call_count() <= calls_before + 1);
}

TEST_CASE("XorshiftRng: default constructor seeds from esp_random() and the MAC exactly once") {
    esp_random_test_set_next(777);
    uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    esp_mac_test_set(mac);

    int rand_calls_before = esp_random_test_call_count();
    int mac_calls_before = esp_mac_test_call_count();

    XorshiftRng rng;

    CHECK(esp_random_test_call_count() == rand_calls_before + 1);
    CHECK(esp_mac_test_call_count() == mac_calls_before + 1);
    CHECK(rng.next() != 0);
}

TEST_CASE("XorshiftRng: default constructor mixes in the MAC, not just esp_random()") {
    esp_random_test_set_next(555);

    uint8_t mac_a[6] = {1, 1, 1, 1, 1, 1};
    esp_mac_test_set(mac_a);
    XorshiftRng rng_a;

    esp_random_test_set_next(555);
    uint8_t mac_b[6] = {2, 2, 2, 2, 2, 2};
    esp_mac_test_set(mac_b);
    XorshiftRng rng_b;

    CHECK(rng_a.next() != rng_b.next());
}

#endif
