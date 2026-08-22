# Velaflame

Copyright (C) 2026 Brian Alvarez. Licensed under the
[GNU General Public License v3.0](LICENSE).

## Introduction

ESPHome firmware for the beautiful Velaflame candles:
<https://www.velaflame.com/>

The existing Velaflame firmware and app did not work with my specific needs
(native Home Assistant support) so I wrote my own firmware based on ESPHome.

If you are looking for attractive, dynamic LED candles, I highly recommend
these! Disclosure that I recieved a partial discount code after emailing them
and describing my desire to cut the candles up and convert them to run on
battery power (more details to come). I do not have any other financial
connection to Velaflame, they just make the best LED candle hardware on the
market today!

## Installing new firmware

To flash the new firmware, follow these [Instructions](FLASH_FIRMWARE.md)

NOTE that this will fully replace the existing firmware. Assume this is an
irreversable change.

## Installing the real ESPHome configs

Once you have ESPHome firmware installed on your Velaflame (such as the
barebones setup from the firmware flashing), you should update it to use the
[config.yaml](config.yaml) from this repo as your main config file.

Make sure to copy the generated passwords from your minimal config into the
substitutions block in this config.yaml, or your device will no longer connect
to Home Assistant.

You are welcome to change the `device_name` and `friendly_name` to meet your
needs, for example `device_name: velaflame06` or
`friendly_name: upstairs hallway sconce`

Once you have set up your config file, you can use ESPHome Builder to install
it onto your velaflame, using the `Install` -> `Wireless` flow.

Most of the logic gets imported from [velaflame.yaml](esphome/velaflame.yaml),
keeping the config file that you need to interact with pretty minimal and
focused on things you may want to change. You can obviously add additional
ESPHome configurations in this `config.yaml` if desired.

If you make any improvements (especially to the LED or Wick flicker code),
please open a PR so we can improve things for everyone.

## Battery-powered builds

By default, the flicker effect seeds its randomness from `esp_random()`, the
ESP32 hardware RNG, for higher-quality (non-repeating) flicker patterns. If
you are building a battery-powered variant that keeps the WiFi/BT radios off
most of the time, define the `VELAFLAME_BATTERY_POWERED` macro to fall back
to the plain `rand()` function instead, e.g. by adding this to your
`config.yaml`:

```yaml
esphome:
  platformio_options:
    build_flags:
      - -DVELAFLAME_BATTERY_POWERED
```

See [flicker_rand.h](esphome/algorithms/flicker_rand.h) for details.
