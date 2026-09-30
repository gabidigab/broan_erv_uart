#include "fan_speed_select.h"

namespace esphome {
namespace broan {

void FanSpeedSelect::control(const std::string &value)
{
	this->parent_->setFanSpeed( value );
}

}  // namespace broan
}  // namespace esphome
