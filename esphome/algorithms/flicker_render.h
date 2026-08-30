// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez
//
// Shared flame-lean / color-shift / output-writing pipeline used by every
// flicker effect (flicker_1st.yaml, toroidal_walk.yaml). Each effect only
// needs to supply `bri_mult` for the current tick (typically from a
// BrightnessEngine); render_flicker_frame() does the rest: advances the
// flame-lean angle, reads the user's base color, shifts hue/saturation for
// dim LEDs, and writes gamma-corrected RGBW to the 3 physical LEDs.
#pragma once
#include "esphome.h"
#include "flicker_math.h"
#include "flicker_rand.h"

namespace flicker_math {

struct LedOutputs {
  esphome::output::FloatOutput &r;
  esphome::output::FloatOutput &g;
  esphome::output::FloatOutput &b;
  esphome::output::FloatOutput &w;
};

inline void render_flicker_frame(esphome::light::LightState &light_main,
                                  float &lean_angle, float wind,
                                  float bri_mult, const LedOutputs &d2,
                                  const LedOutputs &d3, const LedOutputs &d4) {
  // ── Flame lean: random-walk angle around the wick triangle ──
  // D2/D3/D4 are 120° apart; projecting onto them guarantees one is always
  // brighter and one always dimmer — no more three-way ties.
  // Advance ~2.5°/tick = full rotation in ~144 ticks = ~7s; jitter for
  // organic feel.
  lean_angle += 2.5f + (random_unit() * 2.0f - 1.0f) * 1.5f;
  if (lean_angle >= 360.0f) lean_angle -= 360.0f;
  if (lean_angle < 0.0f) lean_angle += 360.0f;

  auto lf = lean_factors(lean_angle, wind);

  // ── Skip output writes during color/brightness transitions ──
  // ESPHome's transition system writes uniform color to all_r/g/b/w (fan-out)
  // at the main loop rate. Our per-LED writes only happen once per tick, so
  // between our writes the transition overwrites our values — causing
  // strobing. Fix: detect an active transition by comparing current vs
  // remote values, and yield to the transition. Internal state (brightness,
  // lean) keeps advancing.
  {
    float tr = fabsf(light_main.current_values.get_brightness() - light_main.remote_values.get_brightness())
             + fabsf(light_main.current_values.get_red()        - light_main.remote_values.get_red())
             + fabsf(light_main.current_values.get_green()      - light_main.remote_values.get_green())
             + fabsf(light_main.current_values.get_blue()       - light_main.remote_values.get_blue())
             + fabsf(light_main.current_values.get_white()      - light_main.remote_values.get_white());
    if (tr > 0.02f) return;
  }

  // ── Read user base color (remote_values stable because publish=false) ──
  float rem_bri = light_main.remote_values.get_brightness();
  float rem_col = light_main.remote_values.get_color_brightness();
  float rem_r   = light_main.remote_values.get_red();
  float rem_g   = light_main.remote_values.get_green();
  float rem_bv  = light_main.remote_values.get_blue();
  float rem_w   = light_main.remote_values.get_white();

  // ── Convert base RGB to HSV ──
  auto base_hsv = rgb_to_hsv(rem_r, rem_g, rem_bv);
  float hue   = base_hsv.h;
  float sat   = base_hsv.s;
  float val_h = base_hsv.v;

  float ghs = global_hue_shift(bri_mult, wind);
  float gsb = global_sat_boost(bri_mult, wind);

  // ── Per-LED output: compute shifted color, gamma-correct, write to hardware ──
  auto write_led = [&](float lean, const LedOutputs &out) {
    auto o = compute_led_output(hue, sat, val_h, ghs, gsb, wind, lean,
                                 rem_bri, bri_mult, rem_col, rem_w);
    out.r.set_level(o.r);
    out.g.set_level(o.g);
    out.b.set_level(o.b);
    out.w.set_level(o.w);
  };

  write_led(lf.d2, d2);
  write_led(lf.d3, d3);
  write_led(lf.d4, d4);
}

}  // namespace flicker_math
