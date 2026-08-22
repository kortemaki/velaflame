// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Pure math for the candle flicker algorithm.
// Ensure this stays free of ESPHome dependencies for test portability.

#pragma once
#include <cmath>

namespace flicker_math {

struct RGB  { float r, g, b; };
struct HSV  { float h, s, v; };
struct Lean { float d2, d3, d4; };

// -- RGB ↔ HSV (matching ESPHome's 0.0–1.0 convention) ----------------------

inline HSV rgb_to_hsv(float r, float g, float b) {
  float cmax  = r; if (g > cmax) cmax = g; if (b > cmax) cmax = b;
  float cmin  = r; if (g < cmin) cmin = g; if (b < cmin) cmin = b;
  float delta = cmax - cmin;

  float hue = 0.0f;
  float sat = (cmax > 0.001f) ? (delta / cmax) : 0.0f;
  float val = cmax;

  if (delta > 0.001f) {
    if      (cmax == r) hue = fmodf((g - b) / delta, 6.0f) / 6.0f;
    else if (cmax == g) hue = ((b - r) / delta + 2.0f) / 6.0f;
    else                hue = ((r - g) / delta + 4.0f) / 6.0f;
    if (hue < 0.0f) hue += 1.0f;
  }
  return {hue, sat, val};
}

inline RGB hsv_to_rgb(float h, float s, float v) {
  float h6 = h * 6.0f;
  int   hi = static_cast<int>(h6);
  float f  = h6 - static_cast<float>(hi);

  float p = v * (1.0f - s);
  float q = v * (1.0f - s * f);
  float t = v * (1.0f - s * (1.0f - f));

  switch (hi % 6) {
    case 0:  return {v, t, p};
    case 1:  return {q, v, p};
    case 2:  return {p, v, t};
    case 3:  return {p, q, v};
    case 4:  return {t, p, v};
    default: return {v, p, q};
  }
}

// -- Brightness clamping -----------------------------------------------------

inline float clamp_brightness(float bri_mult) {
  if (bri_mult < 0.15f) bri_mult = 0.15f;
  if (bri_mult > 1.0f)  bri_mult = 1.0f;
  return bri_mult;
}

// -- Lean projection (120°-separated cosine for D2/D3/D4) --------------------

inline Lean lean_factors(float angle_deg, float wind) {
  constexpr float D2R = 0.017453293f;
  float raw2 = cosf(angle_deg * D2R) * 0.5f + 0.5f;
  float raw3 = cosf((angle_deg + 120.0f) * D2R) * 0.5f + 0.5f;
  float raw4 = cosf((angle_deg + 240.0f) * D2R) * 0.5f + 0.5f;

  float rmax = raw2; if (raw3 > rmax) rmax = raw3; if (raw4 > rmax) rmax = raw4;
  if (rmax < 0.001f) rmax = 0.001f;

  float lean_amp = wind * 0.45f;
  return {
    1.0f - lean_amp * (1.0f - raw2 / rmax),
    1.0f - lean_amp * (1.0f - raw3 / rmax),
    1.0f - lean_amp * (1.0f - raw4 / rmax),
  };
}

// -- Per-LED hue + saturation shift ------------------------------------------
// Dim → lower hue (longer wavelength), higher saturation.
// Cool LEDs (lean < 1.0) get an extra redward shift and sat boost.

struct HueSat { float hue, sat; };

inline HueSat apply_hue_sat_shift(float hue, float sat,
                                  float global_hue_shift, float global_sat_boost,
                                  float wind, float lean) {
  float led_hue = hue + global_hue_shift + (lean - 1.0f) * wind * 0.05f;
  if (led_hue < 0.0f) led_hue = 0.0f;
  if (led_hue > 1.0f) led_hue = 1.0f;

  float sat_led = sat + global_sat_boost + (1.0f - lean) * wind * 0.10f;
  if (sat_led > 1.0f) sat_led = 1.0f;

  return {led_hue, sat_led};
}

// -- Global shift amounts (derived from brightness and wind) -----------------

inline float global_hue_shift(float bri_mult, float wind) {
  return -(1.0f - bri_mult) * wind * 0.10f;
}

inline float global_sat_boost(float bri_mult, float wind) {
  return  (1.0f - bri_mult) * wind * 0.20f;
}

// -- Full per-LED output (hue/sat shift → HSV→RGB → scale → gamma) -----------
// Returns gamma-corrected RGBW ready for set_level().

struct RGBW { float r, g, b, w; };

inline RGBW compute_led_output(float hue, float sat, float val_h,
                               float ghs, float gsb, float wind, float lean,
                               float rem_bri, float bri_mult,
                               float rem_col, float rem_w) {
  HueSat hs = apply_hue_sat_shift(hue, sat, ghs, gsb, wind, lean);
  RGB    c  = hsv_to_rgb(hs.hue, hs.sat, val_h);

  float scale  = rem_bri * bri_mult * lean * rem_col;
  if (scale  > 1.0f) scale  = 1.0f;
  float wscale = rem_bri * bri_mult * lean * rem_w;
  if (wscale > 1.0f) wscale = 1.0f;

  // gamma 2.0
  float vr = scale * c.r;
  float vg = scale * c.g;
  float vb = scale * c.b;
  return {vr * vr, vg * vg, vb * vb, wscale * wscale};
}

}  // namespace flicker_math
