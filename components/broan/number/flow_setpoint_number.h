#pragma once

#include "esphome/components/number/number.h"
#include "../broan.h"

class BroanComponent;

namespace esphome {
namespace broan {

// One flow setpoint (CFM) of the installer menu: a speed (BroanFanSpeed) and a side (BroanFlowSide).
class FlowSetpointNumber : public number::Number, public Parented<BroanComponent> {
public:
	FlowSetpointNumber() = default;
	void set_flow( uint8_t nSpeed, uint8_t nSide ) { m_nSpeed = nSpeed; m_nSide = nSide; }

protected:
	void control(float value) override;

	uint8_t m_nSpeed = 0;
	uint8_t m_nSide = 0;
};

}  // namespace broan
}  // namespace esphome
