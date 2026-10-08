#pragma once

// Pure, host-testable encoder for the room sensor frame (no hardware access - see thermia_room.h/.cpp for the line
// driver, README.md for the protocol and room_sensor_context.md for how it was reverse-engineered).
//
// 29 bits, transmission order, every field LSB-first:
//
//   start(1) | setpoint(8, half degrees) | integer(8) | decimal(4) | checksum(5) | tail(3)
//   1        | 21.0 C -> 42              | 0..31      | 0..9       |             | 011
//
//   - "raw" bit = what goes on the wire (raw 1 = short gap before the pulse, raw 0 = long gap); every field value is
//     the INVERTED raw bits read LSB-first.
//   - setpoint: room setpoint in 0.5 C steps (value = setpoint * 2). While the sensor is accepted the pump uses it as
//     its indoor target (r03/r04) instead of its own HUONE/r32 setting.
//   - checksum = (integer ^ decimal ^ setpoint_half) & 0x1F. The pump checks it and silently drops frames that fail.

#include <stdint.h>

namespace esphome {
namespace thermia_room {

constexpr bool FRAME_START[] = {1};
constexpr bool FRAME_TAIL[] = {0, 1, 1};
constexpr uint8_t SETPOINT_BITS = 8;
constexpr uint8_t INTEGER_BITS = 8;
constexpr uint8_t DECIMAL_BITS = 4;
constexpr uint8_t CHECKSUM_BITS = 5;
constexpr uint8_t FRAME_BITS =
    sizeof(FRAME_START) + SETPOINT_BITS + INTEGER_BITS + DECIMAL_BITS + CHECKSUM_BITS + sizeof(FRAME_TAIL);
static_assert(FRAME_BITS == 29, "frame layout must match the line captures");

// What a real sensor reports until someone changes it.
constexpr float DEFAULT_SETPOINT = 21.0f;
constexpr float SETPOINT_MIN = 5.0f;
constexpr float SETPOINT_MAX = 35.0f;

struct RoomFrame {
  bool bits[FRAME_BITS];  // raw bits, transmission order
  uint8_t setpoint_half;  // setpoint in 0.5 C steps (21.0 C -> 42)
  uint8_t integer_part;   // 0-31 (whole degrees C)
  uint8_t decimal_part;   // 0-9 (tenths of a degree C)
  uint8_t checksum;       // 0-31
};

// Clamps temp_c to 0.0-31.9 °C (rounded to 0.1 °C) and setpoint_c to SETPOINT_MIN..SETPOINT_MAX (rounded to 0.5 °C).
RoomFrame encode_room_temperature(float temp_c, float setpoint_c = DEFAULT_SETPOINT);

// Reads the fields back out of a frame's raw bits. Does not validate the checksum - see frame_checksum_ok().
void decode_room_frame(const bool bits[FRAME_BITS], uint8_t &setpoint_half, uint8_t &integer_part,
                       uint8_t &decimal_part, uint8_t &checksum);

bool frame_checksum_ok(uint8_t integer_part, uint8_t decimal_part, uint8_t checksum,
                       uint8_t setpoint_half = (uint8_t) (DEFAULT_SETPOINT * 2));

}  // namespace thermia_room
}  // namespace esphome
