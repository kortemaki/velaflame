# Home Assistant Entities

Copyright (C) 2026 Brian Alvarez. Licensed under the
[GNU General Public License v3.0](../LICENSE).

## Overview

Velaflame is an ESPHome device, so Home Assistant discovers its entities
automatically via the ESPHome native API — there is no separate MQTT
discovery payload to maintain. All entities are declared in
[esphome/velaflame.yaml](../esphome/velaflame.yaml), with the LED flicker math
implemented in [esphome/algorithms/flicker_1st.yaml](../esphome/algorithms/flicker_1st.yaml)
and [esphome/algorithms/flicker_math.h](../esphome/algorithms/flicker_math.h).

Each physical candle exposes **11 entities** to Home Assistant:

| Domain | Count | Entities |
| --- | --- | --- |
| `light` | 4 | Main, D2, D3, D4 |
| `number` | 5 | Candle Flicker Intensity, Wick Pulse Duration, Wick Flicker Intensity, Wick Flicker Min/Max Interval |
| `button` | 1 | Wick Pulse |
| `switch` | 1 | Wick Flicker |

## Lights (4)

| Entity name | id | Default visibility | Purpose |
| --- | --- | --- | --- |
| `${friendly_name}` (e.g. "VelaFlame") | `light_main` | Enabled | Primary user-facing RGBW light |
| `${friendly_name} D2` | `light_d2` | Disabled by default | Direct control of LED D2 |
| `${friendly_name} D3` | `light_d3` | Disabled by default | Direct control of LED D3 |
| `${friendly_name} D4` | `light_d4` | Disabled by default | Direct control of LED D4 |

- **`light_main`** is an RGBW light whose four output channels
  (`all_r`/`all_g`/`all_b`/`all_w`) are template outputs that fan out to all
  three physical LEDs (D2, D3, D4) at once. This is the entity users interact
  with day-to-day: its color and brightness represent the *base* flame color
  and overall intensity, and it carries the `"1st flicker"` effect
  (`esphome/algorithms/flicker_1st.yaml`) that animates the candle. On boot,
  if no cached color exists yet, it is initialized to a warm amber
  (`red: 100%`, `green: 63.5%`, `blue: 0%`) at 40% brightness; on later boots
  it restores the last color but always comes back on at 40% brightness.
- **`light_d2` / `light_d3` / `light_d4`** are RGBW lights that map directly
  to one physical LED's channels each (e.g. `light_d2` → `d2_r`/`d2_g`/`d2_b`/`d2_w`).
  They are hidden by default and exist for advanced diagnostics or manual
  per-LED color testing. Because the flicker effect writes directly to the
  same underlying `FloatOutput`s while running, these individual light
  entities' own tracked state can fall out of sync with the hardware while
  `light_main`'s effect is active — they're best used only while the effect
  is off.

All four lights use `gamma_correct: 2.0` so that perceived brightness scales
linearly with the light's brightness slider.

## Number Controls (5)

| Entity name | id | Range | Default | Purpose |
| --- | --- | --- | --- | --- |
| Candle Flicker Intensity | `candle_intensity` | 0.4-1.0 (step 0.05) | 0.5 | "Wind" strength for the LED flicker effect |
| Wick Pulse Duration | `pulse_duration` | 10-500 ms (step 10) | 200 | How long each electromagnet pulse stays energized |
| Wick Flicker Intensity | `flicker_intensity` | 0.1-1.0 (step 0.05) | 0.5 | PWM level applied to the H-bridge during a wick pulse |
| Wick Flicker Min Interval | `flicker_min_interval` | 10-500 ms (step 10) | 100 | Minimum delay between automatic wick pulses |
| Wick Flicker Max Interval | `flicker_max_interval` | 10-1000 ms (step 10) | 500 | Maximum delay between automatic wick pulses |

All five are `optimistic` template numbers with `restore_value: True`, so
Home Assistant's set value is trusted immediately and survives reboots.

- **`candle_intensity`** is read once per 50 ms tick by the LED flicker
  lambda as `wind`. It scales *every* layer of the LED animation: how deep
  the slow "gust" dips go, how much fast jitter is applied, how strongly the
  flame "leans" toward one LED, and how much the color hue/saturation shifts
  when dim. It does **not** affect the physical wick.
- **`pulse_duration`** is used by the `pulse_wick` script as the `delay`
  (in ms) between energizing and de-energizing the H-bridge, i.e. how long
  the electromagnet pulls the wick per pulse.
- **`flicker_intensity`** is used by the `pulse_wick` script as the PWM
  `level` written to `hbridge_in1`, i.e. how hard/far each wick pulse
  swings the wick.
- **`flicker_min_interval`** / **`flicker_max_interval`** bound a random
  delay (`min_ms + random(max_ms - min_ms)`) computed inside the
  `flicker_loop` script between successive automatic wick pulses, so pulses
  arrive at irregular, candle-like intervals rather than a fixed rate.

## Button (1)

| Entity name | Action |
| --- | --- |
| Wick Pulse | Executes the `pulse_wick` script once |

Pressing this button fires a single manual wick pulse (energize H-bridge,
hold for `pulse_duration` at `flicker_intensity`, then release), independent
of whether the automatic "Wick Flicker" switch is on. Useful for testing or
a one-off "flicker" on demand.

## Switch (1)

| Entity name | id | Restore mode |
| --- | --- | --- |
| Wick Flicker | `flicker_switch` | `RESTORE_DEFAULT_OFF` |

Toggles the automatic wick-pulsing loop:

- **Turn on:** sets the `flicker_running` global to `true`, powers the
  H-bridge driver (`hbridge_nsleep`), and starts the `flicker_loop` script,
  which repeatedly calls `pulse_wick` and waits a randomized interval
  (bounded by the min/max interval numbers) between pulses.
- **Turn off:** sets `flicker_running` to `false`, stops `flicker_loop`,
  zeroes both H-bridge PWM outputs, and puts the driver to sleep.

## How the Flame Effect Is Produced

The "flame effect" is actually two independent systems running together:
an LED color/brightness animation, and a physical wick-flicking mechanism.

### LED animation (`light_main` + `candle_intensity`)

The `"1st flicker"` lambda effect runs every 50 ms and layers several
effects, all scaled by `candle_intensity` ("wind"):

1. **Slow gust layer** — retargets roughly every 500 ms using a cubic random
   distribution, producing mostly-subtle brightness with rare deep dips.
2. **Fast jitter layer** — adds small, every-tick brightness turbulence.
3. **Flame lean** — a slowly rotating angle (~7 s per revolution, with
   jitter) is projected onto three virtual axes 120° apart
   (`flicker_math::lean_factors`), one per physical LED (D2/D3/D4). This
   guarantees one LED is always brightest and one dimmest at any moment,
   simulating a flame leaning to one side.
4. **HSV color shift** — dimmer/cooler LEDs (lower `lean`) are shifted
   toward a lower hue (longer wavelength/redder) and boosted in saturation,
   mimicking how a real flame looks more orange/red where it's dimmer.

The effect reads the user's chosen base color/brightness from
`light_main`'s `remote_values` (so it doesn't fight the user's own
adjustments or an in-flight transition), converts it to HSV, applies the
per-LED lean and hue/saturation shift, converts back to RGB, applies gamma
correction, and writes the result directly to each LED's `FloatOutput`s
(`d2_r/g/b/w`, `d3_r/g/b/w`, `d4_r/g/b/w`).

### Wick animation (button, switch, 4 wick-related numbers)

Independently of the LEDs, the `DRV8210P` H-bridge physically pulses an
electromagnet to flick the wick, giving the candle real motion. A single
pulse (`pulse_wick` script) energizes the H-bridge at `flicker_intensity`
for `pulse_duration`, then releases it. When the "Wick Flicker" switch is
on, `flicker_loop` calls `pulse_wick` repeatedly with a randomized delay
between `flicker_min_interval` and `flicker_max_interval`. The "Wick Pulse"
button triggers one pulse manually, on demand, regardless of switch state.

Together, the randomized LED brightness/color/lean animation and the
randomized physical wick pulses combine to produce a convincing flame
effect: the light dances in color and intensity while the wick itself
moves.

## RGBW Color Channel Roles

Each LED (and `light_main`) is an RGBW light, meaning four channels are
mixed per LED: Red, Green, Blue, and a dedicated White emitter.

- **Red** — the dominant channel for a warm candle color; contributes most
  of the visible flame intensity.
- **Green** — mixed with red to tune the hue along the amber/orange range
  (e.g. the default boot color is `red: 100%, green: 63.5%, blue: 0%`, a
  warm amber). During the flicker effect, dimmer/cooler LEDs shift hue
  toward the red end by reducing the effective green contribution
  (`global_hue_shift`), reproducing the way a real flame looks redder where
  it's dimmer.
- **Blue** — normally kept near zero for a traditional warm-candle look,
  since raising it pushes the hue toward white/blue. The flicker effect
  never increases the shifted hue past the user's base hue plus a
  redward-only shift, so blue only rises meaningfully if the user
  deliberately picks a bluer base color (e.g. for a themed/accent color
  instead of a traditional flame).
- **White** — a separate warm-white emitter mixed on top of the RGB color.
  It's scaled by the same overall brightness/wind/lean multiplier as the
  RGB channels (`wscale` in `flicker_math::compute_led_output`) so it
  flickers in sync with the flame, but it is not affected by the hue/
  saturation shift — it always adds a steady, desaturating warm-white fill
  light rather than shifting color.

## Reference

- [esphome/velaflame.yaml](../esphome/velaflame.yaml) — all entity
  definitions (`light`, `number`, `button`, `switch`), outputs, and the
  wick-pulse/flicker-loop scripts.
- [esphome/algorithms/flicker_1st.yaml](../esphome/algorithms/flicker_1st.yaml) —
  the LED flicker lambda effect attached to `light_main`.
- [esphome/algorithms/flicker_math.h](../esphome/algorithms/flicker_math.h) —
  pure-math helpers (HSV conversion, lean projection, hue/saturation shift,
  gamma) shared with unit tests in [tests/test_flicker_math.cpp](../tests/test_flicker_math.cpp).
