// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Off-target unit tests for the candle flicker algorithm math.
// These compile and run the real C++ from flicker_math.h directly.
//
// Build & run:
//   Windows (MSVC Developer Command Prompt):
//     cl /std:c++14 /EHsc /I..\esphome\algorithms test_flicker_math.cpp /Fe:test_flicker_math.exe
//     test_flicker_math.exe
//
//   Linux / macOS:
//     g++ -std=c++14 -I../esphome/algorithms test_flicker_math.cpp -o test_flicker_math
//     ./test_flicker_math

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "flicker_math.h"

using namespace flicker_math;

// ---------------------------------------------------------------------------
// RGB -> HSV
// ---------------------------------------------------------------------------

TEST_CASE("rgb_to_hsv: pure primaries") {
    auto r = rgb_to_hsv(1.0f, 0.0f, 0.0f);
    CHECK(r.h == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(r.s == doctest::Approx(1.0f));
    CHECK(r.v == doctest::Approx(1.0f));

    auto g = rgb_to_hsv(0.0f, 1.0f, 0.0f);
    CHECK(g.h == doctest::Approx(1.0f / 3.0f).epsilon(1e-6));

    auto b = rgb_to_hsv(0.0f, 0.0f, 1.0f);
    CHECK(b.h == doctest::Approx(2.0f / 3.0f).epsilon(1e-6));
}

TEST_CASE("rgb_to_hsv: white has zero saturation") {
    auto hsv = rgb_to_hsv(1.0f, 1.0f, 1.0f);
    CHECK(hsv.s == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(hsv.v == doctest::Approx(1.0f));
}

TEST_CASE("rgb_to_hsv: black") {
    auto hsv = rgb_to_hsv(0.0f, 0.0f, 0.0f);
    CHECK(hsv.v == doctest::Approx(0.0f));
    CHECK(hsv.s == doctest::Approx(0.0f));
}

TEST_CASE("rgb_to_hsv: warm amber boot default (1.0, 0.635, 0.0)") {
    auto hsv = rgb_to_hsv(1.0f, 0.635f, 0.0f);
    CHECK(hsv.v == doctest::Approx(1.0f));
    CHECK(hsv.s == doctest::Approx(1.0f));
    CHECK(hsv.h > 0.05f);
    CHECK(hsv.h < 0.15f);  // orange/amber range
}

TEST_CASE("rgb_to_hsv: yellow") {
    auto hsv = rgb_to_hsv(1.0f, 1.0f, 0.0f);
    CHECK(hsv.h == doctest::Approx(1.0f / 6.0f).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// HSV -> RGB
// ---------------------------------------------------------------------------

TEST_CASE("hsv_to_rgb: pure primaries") {
    auto r = hsv_to_rgb(0.0f, 1.0f, 1.0f);
    CHECK(r.r == doctest::Approx(1.0f).epsilon(1e-6));
    CHECK(r.g == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(r.b == doctest::Approx(0.0f).epsilon(1e-6));

    auto g = hsv_to_rgb(1.0f / 3.0f, 1.0f, 1.0f);
    CHECK(g.r == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(g.g == doctest::Approx(1.0f).epsilon(1e-6));

    auto b = hsv_to_rgb(2.0f / 3.0f, 1.0f, 1.0f);
    CHECK(b.b == doctest::Approx(1.0f).epsilon(1e-6));
    CHECK(b.r == doctest::Approx(0.0f).epsilon(1e-6));
}

TEST_CASE("hsv_to_rgb: white (zero saturation)") {
    auto c = hsv_to_rgb(0.0f, 0.0f, 1.0f);
    CHECK(c.r == doctest::Approx(1.0f).epsilon(1e-6));
    CHECK(c.g == doctest::Approx(1.0f).epsilon(1e-6));
    CHECK(c.b == doctest::Approx(1.0f).epsilon(1e-6));
}

TEST_CASE("hsv_to_rgb: zero value is black") {
    auto c = hsv_to_rgb(0.5f, 1.0f, 0.0f);
    CHECK(c.r == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(c.g == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(c.b == doctest::Approx(0.0f).epsilon(1e-6));
}

// ---------------------------------------------------------------------------
// Roundtrip
// ---------------------------------------------------------------------------

TEST_CASE("RGB -> HSV -> RGB roundtrip") {
    float cases[][3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f},
        {1.0f, 0.5f, 0.0f},
        {0.8f, 0.2f, 0.6f},
    };
    for (auto& c : cases) {
        auto hsv = rgb_to_hsv(c[0], c[1], c[2]);
        auto rgb = hsv_to_rgb(hsv.h, hsv.s, hsv.v);
        CHECK(rgb.r == doctest::Approx(c[0]).epsilon(1e-4));
        CHECK(rgb.g == doctest::Approx(c[1]).epsilon(1e-4));
        CHECK(rgb.b == doctest::Approx(c[2]).epsilon(1e-4));
    }
}

// ---------------------------------------------------------------------------
// Brightness clamping
// ---------------------------------------------------------------------------

TEST_CASE("clamp_brightness: passthrough in range") {
    CHECK(clamp_brightness(0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("clamp_brightness: floor at 0.15") {
    CHECK(clamp_brightness(0.0f) == doctest::Approx(0.15f));
    CHECK(clamp_brightness(-1.0f) == doctest::Approx(0.15f));
    CHECK(clamp_brightness(0.15f) == doctest::Approx(0.15f));
}

TEST_CASE("clamp_brightness: ceiling at 1.0") {
    CHECK(clamp_brightness(1.5f) == doctest::Approx(1.0f));
    CHECK(clamp_brightness(1.0f) == doctest::Approx(1.0f));
}

// ---------------------------------------------------------------------------
// Lean factors
// ---------------------------------------------------------------------------

TEST_CASE("lean_factors: zero wind -> all factors = 1.0") {
    for (int angle = 0; angle < 360; angle += 30) {
        auto lf = lean_factors(static_cast<float>(angle), 0.0f);
        CHECK(lf.d2 == doctest::Approx(1.0f).epsilon(1e-6));
        CHECK(lf.d3 == doctest::Approx(1.0f).epsilon(1e-6));
        CHECK(lf.d4 == doctest::Approx(1.0f).epsilon(1e-6));
    }
}

TEST_CASE("lean_factors: hottest LED always = 1.0") {
    for (int angle = 0; angle < 360; angle += 5) {
        auto lf = lean_factors(static_cast<float>(angle), 0.8f);
        float mx = lf.d2;
        if (lf.d3 > mx) mx = lf.d3;
        if (lf.d4 > mx) mx = lf.d4;
        CHECK(mx == doctest::Approx(1.0f).epsilon(1e-6));
    }
}

TEST_CASE("lean_factors: max wind stays in [0.55, 1.0]") {
    for (int angle = 0; angle < 360; angle += 5) {
        auto lf = lean_factors(static_cast<float>(angle), 1.0f);
        for (float f : {lf.d2, lf.d3, lf.d4}) {
            CHECK(f >= 0.55f - 1e-6f);
            CHECK(f <= 1.0f + 1e-6f);
        }
    }
}

TEST_CASE("lean_factors: 120-degree symmetry") {
    for (int angle = 0; angle < 360; angle += 15) {
        auto a = lean_factors(static_cast<float>(angle + 120), 0.7f);
        auto b = lean_factors(static_cast<float>(angle), 0.7f);
        CHECK(a.d2 == doctest::Approx(b.d3).epsilon(1e-5));
    }
}

TEST_CASE("lean_factors: angle 0 peaks D2") {
    auto lf = lean_factors(0.0f, 0.8f);
    CHECK(lf.d2 >= lf.d3);
    CHECK(lf.d2 >= lf.d4);
}

// ---------------------------------------------------------------------------
// Hue + saturation shift
// ---------------------------------------------------------------------------

TEST_CASE("apply_hue_sat_shift: no shift at full brightness + lean") {
    float ghs = global_hue_shift(1.0f, 1.0f);
    float gsb = global_sat_boost(1.0f, 1.0f);
    auto hs = apply_hue_sat_shift(0.1f, 0.8f, ghs, gsb, 1.0f, 1.0f);
    CHECK(hs.hue == doctest::Approx(0.1f).epsilon(1e-6));
    CHECK(hs.sat == doctest::Approx(0.8f).epsilon(1e-6));
}

TEST_CASE("apply_hue_sat_shift: dim shifts hue lower") {
    float ghs_bright = global_hue_shift(1.0f, 1.0f);
    float ghs_dim    = global_hue_shift(0.5f, 1.0f);
    auto bright = apply_hue_sat_shift(0.15f, 0.8f, ghs_bright, 0.0f, 1.0f, 1.0f);
    auto dim    = apply_hue_sat_shift(0.15f, 0.8f, ghs_dim,    0.0f, 1.0f, 1.0f);
    CHECK(dim.hue < bright.hue);
}

TEST_CASE("apply_hue_sat_shift: dim boosts saturation") {
    float gsb_bright = global_sat_boost(1.0f, 1.0f);
    float gsb_dim    = global_sat_boost(0.5f, 1.0f);
    auto bright = apply_hue_sat_shift(0.15f, 0.5f, 0.0f, gsb_bright, 1.0f, 1.0f);
    auto dim    = apply_hue_sat_shift(0.15f, 0.5f, 0.0f, gsb_dim,    1.0f, 1.0f);
    CHECK(dim.sat > bright.sat);
}

TEST_CASE("apply_hue_sat_shift: cool LED extra redward shift") {
    float ghs = global_hue_shift(0.7f, 1.0f);
    auto hot  = apply_hue_sat_shift(0.15f, 0.8f, ghs, 0.0f, 1.0f, 1.0f);
    auto cool = apply_hue_sat_shift(0.15f, 0.8f, ghs, 0.0f, 1.0f, 0.6f);
    CHECK(cool.hue < hot.hue);
}

TEST_CASE("apply_hue_sat_shift: hue clamped to [0, 1]") {
    auto lo = apply_hue_sat_shift(0.01f, 0.5f, -0.5f, 0.0f, 1.0f, 0.3f);
    CHECK(lo.hue >= 0.0f);
    auto hi = apply_hue_sat_shift(0.99f, 0.5f, 0.5f, 0.0f, 1.0f, 1.0f);
    CHECK(hi.hue <= 1.0f);
}

TEST_CASE("apply_hue_sat_shift: saturation capped at 1.0") {
    auto hs = apply_hue_sat_shift(0.5f, 0.95f, 0.0f, 0.5f, 1.0f, 0.3f);
    CHECK(hs.sat <= 1.0f);
}

// ---------------------------------------------------------------------------
// Global shift helpers
// ---------------------------------------------------------------------------

TEST_CASE("global_hue_shift: zero at full brightness") {
    CHECK(global_hue_shift(1.0f, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("global_hue_shift: negative when dim") {
    CHECK(global_hue_shift(0.5f, 1.0f) < 0.0f);
}

TEST_CASE("global_sat_boost: zero at full brightness") {
    CHECK(global_sat_boost(1.0f, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("global_sat_boost: positive when dim") {
    CHECK(global_sat_boost(0.5f, 1.0f) > 0.0f);
}

TEST_CASE("global shifts: zero at zero wind") {
    CHECK(global_hue_shift(0.5f, 0.0f) == doctest::Approx(0.0f));
    CHECK(global_sat_boost(0.5f, 0.0f) == doctest::Approx(0.0f));
}

// ---------------------------------------------------------------------------
// Full pipeline: compute_led_output
// ---------------------------------------------------------------------------

TEST_CASE("compute_led_output: pure red, full brightness") {
    auto hsv = rgb_to_hsv(1.0f, 0.0f, 0.0f);
    auto out = compute_led_output(
        hsv.h, hsv.s, hsv.v,
        0.0f, 0.0f,       // no global shifts
        0.0f, 1.0f,       // no wind, full lean
        1.0f, 1.0f,       // full brightness
        1.0f, 0.0f        // full color, no white
    );
    CHECK(out.r == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(out.g == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(out.b == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(out.w == doctest::Approx(0.0f).epsilon(1e-4));
}

TEST_CASE("compute_led_output: outputs stay in [0, 1]") {
    auto out = compute_led_output(
        0.5f, 1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 1.0f
    );
    for (float ch : {out.r, out.g, out.b, out.w}) {
        CHECK(ch >= 0.0f - 1e-6f);
        CHECK(ch <= 1.0f + 1e-6f);
    }
}

TEST_CASE("compute_led_output: very low brightness -> near-dark") {
    auto out = compute_led_output(
        0.1f, 0.8f, 1.0f,
        0.0f, 0.0f,
        0.5f, 1.0f,
        0.1f, 0.15f,  // very low brightness
        1.0f, 0.0f
    );
    CHECK(out.r < 0.01f);
    CHECK(out.g < 0.01f);
    CHECK(out.b < 0.01f);
}

TEST_CASE("compute_led_output: gamma darkens midrange (0.5 -> 0.25)") {
    auto out = compute_led_output(
        0.0f, 1.0f, 1.0f,  // pure red
        0.0f, 0.0f,         // no shifts
        0.0f, 1.0f,         // no wind, full lean
        0.5f, 1.0f,         // rem_bri=0.5
        1.0f, 0.0f          // full color, no white
    );
    // scale = 0.5 * 1.0 * 1.0 * 1.0 = 0.5; red = 1.0; v = 0.5; gamma = 0.25
    CHECK(out.r == doctest::Approx(0.25f).epsilon(1e-4));
}

TEST_CASE("compute_led_output: white channel independent of RGB color") {
    auto out = compute_led_output(
        0.0f, 1.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 1.0f   // rem_col=0 (no color), rem_w=1.0
    );
    // RGB scale = 1*1*1*0 = 0
    CHECK(out.r == doctest::Approx(0.0f).epsilon(1e-6));
    // wscale = 1*1*1*1 = 1; gamma(1) = 1
    CHECK(out.w == doctest::Approx(1.0f).epsilon(1e-4));
}
