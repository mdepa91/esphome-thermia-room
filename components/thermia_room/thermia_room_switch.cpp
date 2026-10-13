#include "thermia_room_switch.h"
#include "thermia_room.h"

namespace esphome {
namespace thermia_room {

void ThermiaRoomSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->set_transmit(state);
  this->publish_state(state);
}

}  // namespace thermia_room
}  // namespace esphome
