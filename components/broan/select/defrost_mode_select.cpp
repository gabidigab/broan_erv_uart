#include "defrost_mode_select.h"

namespace esphome {
namespace broan {

void DefrostModeSelect::control(const std::string &value)
{
	this->parent_->setDefrostMode( value );
}

}  // namespace broan
}  // namespace esphome
