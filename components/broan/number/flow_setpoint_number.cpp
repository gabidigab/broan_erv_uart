#include "flow_setpoint_number.h"

namespace esphome {
namespace broan {

void FlowSetpointNumber::control(float value)
{
	this->parent_->setFlowSetpoint( m_nSpeed, m_nSide, value );
}

}  // namespace broan
}  // namespace esphome
