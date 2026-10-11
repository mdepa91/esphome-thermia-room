#pragma once

// HA-writable number for the room temperature or the setpoint. control() only stores the value on the parent; it
// goes out with the next frame, and the parent republishes what it actually encoded (after rounding/clamping).

#include "esphome/components/number/number.h"

#include <cstdint>

namespace esphome {
namespace thermia_room {

class ThermiaRoomComponent;

class ThermiaRoomNumber : public number::Number {
 public:
  enum Kind : uint8_t { ROOM_TEMPERATURE, SETPOINT };

  void set_parent(ThermiaRoomComponent *parent) { this->parent_ = parent; }
  void set_kind(Kind kind) { this->kind_ = kind; }

 protected:
  void control(float value) override;

  ThermiaRoomComponent *parent_{nullptr};
  Kind kind_{ROOM_TEMPERATURE};
};

}  // namespace thermia_room
}  // namespace esphome
