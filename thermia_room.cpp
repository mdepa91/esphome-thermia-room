#include "thermia_room.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
#include <driver/gpio.h>
#include <esp_timer.h>
#endif

namespace esphome {
namespace thermia_room {

static const char *const TAG = "thermia_room";

// In dry run nothing is transmitted; log a frame about as often as one would go out.
static const uint32_t DRY_RUN_LOG_INTERVAL_MS = 10000;

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
namespace {

// An RMT symbol duration is a 15-bit field (max ~32.7 ms at 1 tick/us); gaps run to hundreds of ms, so a single
// gap or pulse is split across as many same-level symbols as it takes.
constexpr uint32_t RMT_SYMBOL_DURATION_MAX = 0x7FFF;

// Appends `duration_us` at `level` to `out`, packing two half-pulses per rmt_symbol_word_t. `half`/`pending`
// carry the in-progress symbol between calls.
void push_pulse(std::vector<rmt_symbol_word_t> &out, uint8_t &half, rmt_symbol_word_t &pending, bool level,
                uint32_t duration_us) {
  do {
    uint32_t chunk = std::min(duration_us, RMT_SYMBOL_DURATION_MAX);
    duration_us -= chunk;
    if (half == 0) {
      pending.level0 = level;
      pending.duration0 = chunk;
      half = 1;
    } else {
      pending.level1 = level;
      pending.duration1 = chunk;
      out.push_back(pending);
      half = 0;
    }
  } while (duration_us != 0);
}

}  // namespace

bool ThermiaRoomComponent::on_tx_done_(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                       void *user_ctx) {
  auto *self = static_cast<ThermiaRoomComponent *>(user_ctx);
  self->done_ms_ = (uint32_t) (esp_timer_get_time() / 1000);
  self->busy_ = false;
  return false;  // no higher-priority task to wake
}

void ThermiaRoomComponent::configure_rmt_() {
  rmt_tx_channel_config_t channel_config = {};
  channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_config.resolution_hz = RMT_RESOLUTION_HZ;
  channel_config.gpio_num = gpio_num_t(this->pin_->get_pin());
  channel_config.mem_block_symbols = 64;
  channel_config.trans_queue_depth = 1;
  channel_config.flags.invert_out = this->pin_->is_inverted();

  esp_err_t err = rmt_new_tx_channel(&channel_config, &this->channel_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  if (this->pin_->get_flags() & gpio::FLAG_PULLUP) {
    gpio_pullup_en(gpio_num_t(this->pin_->get_pin()));
  } else {
    gpio_pullup_dis(gpio_num_t(this->pin_->get_pin()));
  }

  rmt_tx_event_callbacks_t callbacks = {};
  callbacks.on_trans_done = on_tx_done_;
  err = rmt_tx_register_event_callbacks(this->channel_, &callbacks, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_tx_register_event_callbacks failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  rmt_copy_encoder_config_t encoder_config = {};
  err = rmt_new_copy_encoder(&encoder_config, &this->encoder_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_new_copy_encoder failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = rmt_enable(this->channel_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Park the pin at the idle level right away instead of leaving it in its reset state until the first frame.
  bool idle_level = this->polarity_invert_;
  rmt_symbol_word_t idle_symbol = {};
  idle_symbol.level0 = idle_level;
  idle_symbol.duration0 = 1;
  idle_symbol.level1 = idle_level;
  idle_symbol.duration1 = 0;
  rmt_transmit_config_t idle_config = {};
  idle_config.flags.eot_level = idle_level;
  rmt_transmit(this->channel_, this->encoder_, &idle_symbol, sizeof(idle_symbol), &idle_config);
  rmt_tx_wait_all_done(this->channel_, 50);

  this->transmit_ready_ = true;
}
#endif

void ThermiaRoomComponent::setup() {
  this->setpoint_pref_ = global_preferences->make_preference<float>(fnv1_hash("thermia_room_setpoint_v2"));
  float stored;
  if (this->setpoint_number_ != nullptr && this->setpoint_pref_.load(&stored) && stored >= SETPOINT_MIN &&
      stored <= SETPOINT_MAX)
    this->setpoint_c_ = stored;

  if (this->dry_run_)
    return;  // nothing is driven, no RMT channel needed

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  this->configure_rmt_();
#else
  ESP_LOGE(TAG, "RMT not supported on this chip - only dry_run is possible");
  this->mark_failed();
#endif
}

void ThermiaRoomComponent::set_setpoint(float setpoint_c) {
  if (setpoint_c < SETPOINT_MIN)
    setpoint_c = SETPOINT_MIN;
  if (setpoint_c > SETPOINT_MAX)
    setpoint_c = SETPOINT_MAX;
  this->setpoint_c_ = std::lround(setpoint_c * 2.0f) / 2.0f;
  this->setpoint_pref_.save(&this->setpoint_c_);
}

void ThermiaRoomComponent::update() {
  // Show what the pump is actually being told (after rounding/clamping), not just what was asked for.
  RoomFrame frame = this->current_frame_();
  if (this->room_temperature_number_ != nullptr)
    this->room_temperature_number_->publish_state(frame.integer_part + frame.decimal_part / 10.0f);
  if (this->setpoint_number_ != nullptr)
    this->setpoint_number_->publish_state(frame.setpoint_half / 2.0f);
}

void ThermiaRoomComponent::loop() {
  if (this->dry_run_) {
    const uint32_t now = millis();
    if (now - this->last_dry_run_ms_ >= DRY_RUN_LOG_INTERVAL_MS) {
      this->last_dry_run_ms_ = now;
      this->log_frame_(this->current_frame_());
    }
    return;
  }
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  if (!this->transmit_ready_ || this->busy_)
    return;
  if (this->done_ms_ != 0 && (uint32_t) (esp_timer_get_time() / 1000) - this->done_ms_ < this->frame_gap_ms_)
    return;
  this->send_frame_();
#endif
}

void ThermiaRoomComponent::send_frame_() {
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  RoomFrame frame = this->current_frame_();
  this->log_frame_(frame);

  const bool mark_level = !this->polarity_invert_;  // high = photoMOS on = line loaded
  const bool idle_level = this->polarity_invert_;

  this->symbols_.clear();
  uint8_t half = 0;
  rmt_symbol_word_t pending = {};
  // Each bit is its gap followed by its pulse: the gap BEFORE a pulse carries that pulse's bit.
  for (bool raw_bit : frame.bits) {
    push_pulse(this->symbols_, half, pending, idle_level, raw_bit ? this->space_short_us_ : this->space_long_us_);
    push_pulse(this->symbols_, half, pending, mark_level, this->mark_us_);
  }
  if (half != 0) {
    pending.level1 = idle_level;  // odd number of half-pulses: flush the last one
    pending.duration1 = 0;
    this->symbols_.push_back(pending);
  }

  rmt_transmit_config_t config = {};
  config.flags.eot_level = idle_level;
  this->busy_ = true;  // before the call: on_tx_done_ may fire before rmt_transmit() returns
  esp_err_t err = rmt_transmit(this->channel_, this->encoder_, this->symbols_.data(),
                               this->symbols_.size() * sizeof(rmt_symbol_word_t), &config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
    this->busy_ = false;
  }
#endif
}

void ThermiaRoomComponent::log_frame_(const RoomFrame &frame) const {
  ESP_LOGD(TAG, "%s %u.%u °C, setpoint %.1f °C", this->dry_run_ ? "[dry run] would send" : "sending",
           frame.integer_part, frame.decimal_part, frame.setpoint_half / 2.0f);

  // Same notation as a logic analyzer trace: "I" = short gap + pulse (raw 1), "_I" = long gap + pulse (raw 0).
  char wave[FRAME_BITS * 2 + 1];
  uint8_t pos = 0;
  for (bool raw_bit : frame.bits) {
    if (!raw_bit)
      wave[pos++] = '_';
    wave[pos++] = 'I';
  }
  wave[pos] = '\0';
  ESP_LOGV(TAG, "  checksum %u, waveform %s", frame.checksum, wave);
}

void ThermiaRoomComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Thermia room sensor emulator:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Pulse: %" PRIu32 " us, short gap: %" PRIu32 " us, long gap: %" PRIu32 " us", this->mark_us_,
                this->space_short_us_, this->space_long_us_);
  ESP_LOGCONFIG(TAG, "  Gap between frames: %" PRIu32 " ms", this->frame_gap_ms_);
  ESP_LOGCONFIG(TAG, "  Polarity inverted: %s", YESNO(this->polarity_invert_));
  ESP_LOGCONFIG(TAG, "  Setpoint: %.1f °C", this->setpoint_c_);
  if (this->dry_run_)
    ESP_LOGCONFIG(TAG, "  DRY RUN - frames are only logged, the pin is not driven");
}

}  // namespace thermia_room
}  // namespace esphome
