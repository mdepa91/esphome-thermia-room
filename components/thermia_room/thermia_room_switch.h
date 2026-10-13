#pragma once

// HA switch that turns the frames on and off. While it is off the line stays idle, as if no room sensor were
// connected: the pump raises the room sensor alarm and regulates to its own fallback setpoint (r32).

#include "esphome/components/switch/switch.h"

namespace esphome {
namespace thermia_room {

class ThermiaRoomComponent;

class ThermiaRoomSwitch : public switch_::Switch {
 public:
  void set_parent(ThermiaRoomComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  ThermiaRoomComponent *parent_{nullptr};
};

}  // namespace thermia_room
}  // namespace esphome
