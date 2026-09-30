#pragma once

#include "esphome/components/select/select.h"
#include "../broan.h"

class BroanComponent;

namespace esphome {
namespace broan {

class FanSpeedSelect : public select::Select, public Parented<BroanComponent> {
public:
	FanSpeedSelect() = default;

protected:
	void control(const std::string &value) override;
};

}  // namespace broan
}  // namespace esphome
