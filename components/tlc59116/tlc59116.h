// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Brian Alvarez

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tlc59116 {

// TLC59116 registers
static const uint8_t REG_MODE1 = 0x00;
static const uint8_t REG_MODE2 = 0x01;
static const uint8_t REG_PWM0 = 0x02;      // PWM0-PWM15: 0x02-0x11
static const uint8_t REG_GRPPWM = 0x12;
static const uint8_t REG_GRPFREQ = 0x13;
static const uint8_t REG_LEDOUT0 = 0x14;   // LEDOUT0-LEDOUT3: 0x14-0x17

class TLC59116Output : public Component, public i2c::I2CDevice {
 public:
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }

  void setup() override {
    // Drive enable pin HIGH and wait for the chip to be ready
    if (this->enable_pin_ != nullptr) {
      this->enable_pin_->setup();
      this->enable_pin_->digital_write(true);
      delay(5);
      ESP_LOGI("tlc59116", "Enable pin asserted");
    }

    // MODE1: normal mode (OSC on)
    uint8_t mode1 = 0x00;

    // Retry a few times — the enable pin may have just been asserted
    bool ok = false;
    for (uint8_t attempt = 0; attempt < 5; attempt++) {
      if (this->write_register(REG_MODE1, &mode1, 1) == i2c::ERROR_OK) {
        ok = true;
        break;
      }
      ESP_LOGW("tlc59116", "MODE1 write failed, retrying (%u/5)...", attempt + 1);
      delay(10);
    }
    if (!ok) {
      ESP_LOGE("tlc59116", "Failed to write MODE1 after retries - check I2C address and wiring");
      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->digital_write(false);
        ESP_LOGW("tlc59116", "Enable pin de-asserted due to setup failure");
      }
      this->mark_failed();
      return;
    }

    // MODE2: default settings
    uint8_t mode2 = 0x00;
    this->write_register(REG_MODE2, &mode2, 1);

    // Set all channels to individual PWM control
    // LEDOUT bits: 00=off, 01=on, 10=individual PWM, 11=group PWM
    // 0xAA = 10 10 10 10 = individual PWM for 4 channels per register
    uint8_t ledout = 0xAA;
    this->write_register(REG_LEDOUT0, &ledout, 1);
    this->write_register(REG_LEDOUT0 + 1, &ledout, 1);
    this->write_register(REG_LEDOUT0 + 2, &ledout, 1);
    this->write_register(REG_LEDOUT0 + 3, &ledout, 1);

    // Set all PWM channels to 0
    for (uint8_t i = 0; i < 16; i++) {
      uint8_t zero = 0;
      this->write_register(REG_PWM0 + i, &zero, 1);
    }

    ESP_LOGI("tlc59116", "TLC59116 initialized at address 0x%02X", this->address_);
  }

  void set_channel_value(uint8_t channel, float state) {
    if (channel > 15)
      return;
    uint8_t pwm = static_cast<uint8_t>(state * 255.0f);
    this->write_register(REG_PWM0 + channel, &pwm, 1);
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  GPIOPin *enable_pin_{nullptr};
};

class TLC59116Channel : public output::FloatOutput, public Component {
 public:
  void set_parent(TLC59116Output *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void dump_config() override {
    ESP_LOGCONFIG("tlc59116", "  Channel: %u", this->channel_);
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override {
    this->parent_->set_channel_value(this->channel_, state);
  }

  TLC59116Output *parent_;
  uint8_t channel_;
};

}  // namespace tlc59116
}  // namespace esphome
