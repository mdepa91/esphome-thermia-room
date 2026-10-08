#include "thermia_room_frame.h"

#include <cmath>

namespace esphome {
namespace thermia_room {

namespace {

void push_lsb_first_raw(bool *bits, uint8_t &pos, uint8_t value, uint8_t n_bits) {
  for (uint8_t k = 0; k < n_bits; k++)
    bits[pos++] = !((value >> k) & 0x1);  // raw bit = inverted logical bit
}

uint8_t pull_lsb_first_raw(const bool *bits, uint8_t &pos, uint8_t n_bits) {
  uint8_t value = 0;
  for (uint8_t k = 0; k < n_bits; k++) {
    if (!bits[pos++])
      value |= (1u << k);
  }
  return value;
}

uint8_t checksum_for(uint8_t integer_part, uint8_t decimal_part, uint8_t setpoint_half) {
  return (integer_part ^ decimal_part ^ setpoint_half) & 0x1F;
}

}  // namespace

RoomFrame encode_room_temperature(float temp_c, float setpoint_c) {
  if (temp_c < 0.0f)
    temp_c = 0.0f;
  if (temp_c > 31.9f)
    temp_c = 31.9f;
  int tenths = (int) std::lround(temp_c * 10.0f);
  if (tenths > 319)
    tenths = 319;  // guard the rounding-up-at-the-boundary case

  if (setpoint_c < SETPOINT_MIN)
    setpoint_c = SETPOINT_MIN;
  if (setpoint_c > SETPOINT_MAX)
    setpoint_c = SETPOINT_MAX;

  RoomFrame f{};
  f.setpoint_half = (uint8_t) std::lround(setpoint_c * 2.0f);
  f.integer_part = (uint8_t) (tenths / 10);
  f.decimal_part = (uint8_t) (tenths % 10);
  f.checksum = checksum_for(f.integer_part, f.decimal_part, f.setpoint_half);

  uint8_t pos = 0;
  for (bool b : FRAME_START)
    f.bits[pos++] = b;
  push_lsb_first_raw(f.bits, pos, f.setpoint_half, SETPOINT_BITS);
  push_lsb_first_raw(f.bits, pos, f.integer_part, INTEGER_BITS);
  push_lsb_first_raw(f.bits, pos, f.decimal_part, DECIMAL_BITS);
  push_lsb_first_raw(f.bits, pos, f.checksum, CHECKSUM_BITS);
  for (bool b : FRAME_TAIL)
    f.bits[pos++] = b;
  return f;
}

void decode_room_frame(const bool bits[FRAME_BITS], uint8_t &setpoint_half, uint8_t &integer_part,
                       uint8_t &decimal_part, uint8_t &checksum) {
  uint8_t pos = sizeof(FRAME_START);
  setpoint_half = pull_lsb_first_raw(bits, pos, SETPOINT_BITS);
  integer_part = pull_lsb_first_raw(bits, pos, INTEGER_BITS);
  decimal_part = pull_lsb_first_raw(bits, pos, DECIMAL_BITS);
  checksum = pull_lsb_first_raw(bits, pos, CHECKSUM_BITS);
}

bool frame_checksum_ok(uint8_t integer_part, uint8_t decimal_part, uint8_t checksum, uint8_t setpoint_half) {
  return checksum_for(integer_part, decimal_part, setpoint_half) == checksum;
}

}  // namespace thermia_room
}  // namespace esphome
