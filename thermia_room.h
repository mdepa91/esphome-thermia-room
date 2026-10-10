#pragma once

// Emulates the Thermia/Danfoss room sensor on terminals 303/304: sends the room temperature and setpoint to the
// pump, so it regulates on real values instead of its fixed ~20 °C default. Frame format: thermia_room_frame.h,
// wiring and protocol: README.md.
//
// Line encoding: pulse distance - every bit is a gap (line released) followed by a fixed-width pulse (line
// loaded through the photoMOS); a short gap is a raw 1, a long gap a raw 0. Frames are separated by an idle gap,
// which the pump uses to find the start of a frame - sending them back to back makes it lose sync.

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"

#include "thermia_room_frame.h"

#include <vector>

#if defined(USE_ESP32)
#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED
#include <driver/rmt_tx.h>
#endif
#endif

namespace esphome {
namespace thermia_room {

class ThermiaRoomComponent : public PollingComponent {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_mark_us(uint32_t us) { this->mark_us_ = us; }
  void set_space_short_us(uint32_t us) { this->space_short_us_ = us; }
  void set_space_long_us(uint32_t us) { this->space_long_us_ = us; }
  void set_frame_gap_ms(uint32_t ms) { this->frame_gap_ms_ = ms; }
  void set_polarity_invert(bool invert) { this->polarity_invert_ = invert; }
  void set_dry_run(bool dry_run) { this->dry_run_ = dry_run; }
  void set_room_temperature_number(number::Number *num) { this->room_temperature_number_ = num; }
  void set_setpoint_number(number::Number *num) { this->setpoint_number_ = num; }
  void set_setpoint_range(float min_c, float max_c) {
    this->setpoint_min_c_ = min_c;
    this->setpoint_max_c_ = max_c;
  }

  // Called from ThermiaRoomNumber::control(); the new values go out with the next frame.
  void set_room_temperature(float temp_c) { this->room_temp_c_ = temp_c; }
  void set_setpoint(float setpoint_c);  // clamped to the configured range, rounded to 0.5 °C, persisted

  void setup() override;
  void loop() override;
  void update() override;  // only republishes the entities; frames are paced by loop()
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  RoomFrame current_frame_() const { return encode_room_temperature(this->room_temp_c_, this->setpoint_c_); }
  void send_frame_();
  void log_frame_(const RoomFrame &frame) const;

  InternalGPIOPin *pin_{nullptr};
  uint32_t mark_us_{0};
  uint32_t space_short_us_{0};
  uint32_t space_long_us_{0};
  uint32_t frame_gap_ms_{0};
  bool polarity_invert_{false};
  bool dry_run_{false};

  number::Number *room_temperature_number_{nullptr};
  number::Number *setpoint_number_{nullptr};
  float room_temp_c_{20.0f};
  float setpoint_c_{DEFAULT_SETPOINT};
  float setpoint_min_c_{SETPOINT_MIN};
  float setpoint_max_c_{SETPOINT_MAX};
  ESPPreferenceObject setpoint_pref_;
  uint32_t last_dry_run_ms_{0};

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  static constexpr uint32_t RMT_RESOLUTION_HZ = 1000000;  // 1 tick = 1 us, so the *_us_ values are tick counts

  void configure_rmt_();

  bool transmit_ready_{false};
  rmt_channel_handle_t channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  // Read by the RMT hardware while a frame is on the wire - only rebuilt once busy_ is cleared.
  std::vector<rmt_symbol_word_t> symbols_;
  volatile bool busy_{false};      // set before rmt_transmit(), cleared by on_tx_done_ (RMT ISR)
  volatile uint32_t done_ms_{0};   // when the last frame finished, for frame_gap
  static bool on_tx_done_(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx);
#endif
};

}  // namespace thermia_room
}  // namespace esphome
