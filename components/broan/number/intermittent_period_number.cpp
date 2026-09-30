#include "intermittent_period_number.h"

namespace esphome {
namespace broan {

void IntermittentPeriodNumber::control(float value)
{
	// Minutes per hour -> seconds
	this->parent_->setIntermittentPeriod( (uint32_t)lroundf( value * 60.f ) );
}

}  // namespace broan
}  // namespace esphome