#include "thermia_room_number.h"
#include "thermia_room.h"

namespace esphome {
namespace thermia_room {

void ThermiaRoomNumber::control(float value) {
  this->publish_state(value);  // optimistic - corrected by the parent's next update()
  if (this->parent_ == nullptr)
    return;
  if (this->kind_ == SETPOINT)
    this->parent_->set_setpoint(value);
  else
    this->parent_->set_room_temperature(value);
}

}  // namespace thermia_room
}  // namespace esphome
