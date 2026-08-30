// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki
//
// Alternative candle flicker brightness engine: a 2D random walk over a
// tileable 64x64 toroidal brightness map (torus_map.h), with an optional
// random strobe dip layered on top. See scripts/generate_torus.py for how
// the map itself is generated.
#pragma once
#include "esphome.h"
#include "brightness_engine.h"
#include "flicker_rand.h"
#include "torus_map.h"

class ChandelierFlickerEngine {
private:
    flicker_math::XorshiftRng rng_;
    uint16_t x_fp = 0;
    uint16_t y_fp = 0;

    // Velocities adjusted slightly for the 65.5ms nominal baseline window
    int16_t base_dx = 32;
    int16_t base_dy = 16;

    uint32_t last_execution_time_us = 0;
    uint32_t trajectory_timer_us = 0;

    // --- Pure Integer Strobe Engine States ---
    // The strobe dip is an optional embellishment layered on top of the base
    // toroidal-walk brightness; see set_strobe_enabled().
    bool strobe_enabled_ = true;
    bool strobe_active = false;
    uint32_t strobe_event_timer_us = 0;
    uint32_t strobe_phase_accumulator = 0;
    uint32_t strobe_duration_limit_us = 0;
    uint16_t strobe_frequency_step = 250;

public:
    ChandelierFlickerEngine() {
        x_fp = rng_.next() & 0xFFFF;
        y_fp = rng_.next() & 0xFFFF;
    }

    // Disabling prevents any *new* strobe from being triggered and forces
    // the multiplier back to baseline; an in-progress strobe is allowed to
    // finish naturally rather than cutting out abruptly.
    void set_strobe_enabled(bool enabled) {
        strobe_enabled_ = enabled;
        if (!enabled) strobe_active = false;
    }

    uint8_t get_next_frame() {
        uint32_t current_time_us = esp_timer_get_time();
        if (last_execution_time_us == 0) {
            last_execution_time_us = current_time_us;
            return 220;
        }

        uint32_t delta_us = current_time_us - last_execution_time_us;
        last_execution_time_us = current_time_us;

        // 1. FIXED-POINT SCALER OPTIMIZATION (Bypasses Floating-Point Division)
        // Shifting right by 6 scales the microsecond delta directly.
        // A nominal target loop of 65,536us results in an integer scaler value of exactly 1024 (1.0x).
        uint32_t time_scaler_fp = delta_us >> 6;

        // 2. Apply the Scaler using an Integer Multiply and a 10-bit Shift (Divides by 1024)
        int32_t stepped_dx = ((int32_t)base_dx * time_scaler_fp) >> 10;
        int32_t stepped_dy = ((int32_t)base_dy * time_scaler_fp) >> 10;

        x_fp += (int16_t)stepped_dx;
        y_fp += (int16_t)stepped_dy;

        uint8_t x0 = (x_fp >> 8) & 63;
        uint8_t y0 = (y_fp >> 8) & 63;
        uint8_t x1 = (x0 + 1) & 63;
        uint8_t y1 = (y0 + 1) & 63;
        uint8_t f_x = x_fp & 0xFF;
        uint8_t f_y = y_fp & 0xFF;

        // Fetch values from flash
        uint8_t p00 = pgm_read_byte(&(TORUS_MAP[y0][x0]));
        uint8_t p10 = pgm_read_byte(&(TORUS_MAP[y0][x1]));
        uint8_t p01 = pgm_read_byte(&(TORUS_MAP[y1][x0]));
        uint8_t p11 = pgm_read_byte(&(TORUS_MAP[y1][x1]));

        // Smooth Bilinear LERP
        uint32_t top = ((uint32_t)p00 << 8) + (int32_t)(p10 - p00) * f_x;
        uint32_t bot = ((uint32_t)p01 << 8) + (int32_t)(p11 - p01) * f_x;
        uint32_t final_val = (top << 8) + (int32_t)(bot - top) * f_y;
        uint8_t base_brightness = (final_val >> 16) & 0xFF;

        // 3. Pure Integer Multiplicative Strobe Engine
        uint32_t strobe_multiplier_fp = 1024; // 1024 represents a 1.0x baseline scale

        if (strobe_active) {
            // Scale phase movement cleanly using our integer time scaler
            strobe_phase_accumulator += (strobe_frequency_step * time_scaler_fp) >> 10;

            // Extract a triangular modulation wave using fast bitwise masks
            uint8_t wave_position = (strobe_phase_accumulator >> 3) & 0xFF;
            uint8_t wave_tri = abs(128 - wave_position); // Range: 0 to 128

            // Fixed-point scaling math: Dips brightness down to roughly ~65% intensity
            // 665 represents 0.65x (in 1024 base), and 358 represents 0.35x
            strobe_multiplier_fp = 665 + ((358 * wave_tri) >> 7);

            strobe_event_timer_us += delta_us;
            if (strobe_event_timer_us >= strobe_duration_limit_us) {
                strobe_active = false;
                strobe_event_timer_us = 0;
            }
        }

        // 4. Trajectory Updates and Draft Event Triggering via Microseconds
        trajectory_timer_us += delta_us;
        if (trajectory_timer_us >= 2097152) { // 2,097,152 us is a power-of-two close to ~2.1 seconds
            trajectory_timer_us = 0;
            uint32_t raw_rng = rng_.next();

            base_dx = (int16_t)(((raw_rng & 0x0F) - 8) * 4);
            base_dy = (int16_t)((((raw_rng >> 4) & 0x0F) - 8) * 4);
            if (base_dx == 0 && base_dy == 0) { base_dx = 32; base_dy = 16; }

            // Evaluates a ~5% chance every ~2 seconds
            if (strobe_enabled_ && !strobe_active && ((raw_rng >> 8) & 0xFF) > 242) {
                strobe_active = true;
                strobe_event_timer_us = 0;
                strobe_phase_accumulator = raw_rng & 0xFFFF;
                strobe_frequency_step = 220 + (raw_rng & 0x3F);

                // Randomize strobe duration boundaries between 2,097,152 and 6,291,456 us (~2 to 6 seconds)
                strobe_duration_limit_us = 2097152 + ((raw_rng >> 15) & 0x1FFFFF);
            }
        }

        // Final fixed-point multiplication: shift right by 10 to clear the 1024 base scaling
        return (uint8_t)(((uint32_t)base_brightness * strobe_multiplier_fp) >> 10);
    }
};

// Adapts ChandelierFlickerEngine to the shared BrightnessEngine interface so
// it can be used as a drop-in replacement for GustBrightnessEngine in
// flicker_render.h's pipeline (see toroidal_walk.yaml). `wind` is ignored:
// this engine's brightness comes entirely from the toroidal walk + strobe,
// not from the candle_intensity input.
namespace flicker_math {

class ToroidalWalkBrightnessEngine : public BrightnessEngine {
 public:
  void set_strobe_enabled(bool enabled) { engine_.set_strobe_enabled(enabled); }

  float update(float wind) override {
    (void)wind;
    return static_cast<float>(engine_.get_next_frame()) / 255.0f;
  }

 private:
  ChandelierFlickerEngine engine_;
};

}  // namespace flicker_math
