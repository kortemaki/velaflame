// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Off-target unit tests for flicker_rand.h. These verify that
// flicker_math::random_unit() imports and calls the correct underlying
// random source depending on whether VELAFLAME_BATTERY_POWERED is defined,
// by compiling this file twice, once per mode.
//
// Build & run:
//   Windows (MSVC Developer Command Prompt):
//     REM Default (esp_random()) mode -- needs the test-only stub, since the
//     REM real ESP-IDF header isn't available off-target:
//     cl /std:c++14 /EHsc /I..\esphome\algorithms /Istubs ^
//       test_flicker_rand.cpp /Fe:test_flicker_rand_default.exe
//     test_flicker_rand_default.exe
//
//     REM Battery-powered (rand()) mode -- deliberately built WITHOUT
//     REM /Istubs, so a successful compile also proves this mode never
//     REM references esp_random():
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
// Test-only stub; resolves to tests/stubs/esp_random.h via -Istubs. Not
// available off-target otherwise, so building this mode without -Istubs
// would fail to compile -- which is itself proof that the battery-powered
// mode below never references esp_random().
#include <esp_random.h>
#endif

#include "flicker_rand.h"

#ifdef VELAFLAME_BATTERY_POWERED

// ---------------------------------------------------------------------------
// VELAFLAME_BATTERY_POWERED defined: random_unit() must use libc rand().
// ---------------------------------------------------------------------------

TEST_CASE("random_unit: battery-powered mode matches libc rand()") {
    srand(1234);
    float actual = flicker_math::random_unit();

    // Reproduce the same rand() call from the same seed to confirm
    // random_unit() is drawing directly from libc's rand() sequence.
    srand(1234);
    float expected = static_cast<float>(rand() % 1024) / 1024.0f;

    CHECK(actual == doctest::Approx(expected));
}

TEST_CASE("random_unit: battery-powered mode advances the rand() sequence") {
    srand(99);
    float first = flicker_math::random_unit();
    float second = flicker_math::random_unit();
    // Extremely unlikely to collide if each call is really consuming a new
    // rand() draw rather than returning a cached/constant value.
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

#else

// ---------------------------------------------------------------------------
// VELAFLAME_BATTERY_POWERED undefined: random_unit() must use esp_random().
// ---------------------------------------------------------------------------

TEST_CASE("random_unit: default mode calls esp_random() exactly once") {
    esp_random_test_set_next(777);
    int calls_before = esp_random_test_call_count();

    float actual = flicker_math::random_unit();

    CHECK(esp_random_test_call_count() == calls_before + 1);
    CHECK(actual == doctest::Approx(777.0f / 1024.0f));
}

TEST_CASE("random_unit: default mode reflects whatever esp_random() returns") {
    esp_random_test_set_next(0);
    CHECK(flicker_math::random_unit() == doctest::Approx(0.0f));

    esp_random_test_set_next(1023);
    CHECK(flicker_math::random_unit() == doctest::Approx(1023.0f / 1024.0f));

    // esp_random() returns a uint32_t; random_unit() must reduce it modulo
    // 1000 rather than overflowing or truncating some other way.
    esp_random_test_set_next(0xFFFFFFFFu);
    CHECK(flicker_math::random_unit() ==
          doctest::Approx(static_cast<float>(0xFFFFFFFFu % 1024) / 1024.0f));
}

TEST_CASE("random_unit: default mode result stays in [0, 1)") {
    for (uint32_t seed : {0u, 1u, 999u, 1000u, 123456u, 0xFFFFFFFFu}) {
        esp_random_test_set_next(seed);
        float v = flicker_math::random_unit();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

#endif
