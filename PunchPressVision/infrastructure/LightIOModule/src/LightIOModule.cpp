#include "infrastructure/LightIOModule/LightIOModule.hpp"

#include "infrastructure/ControlModule/ControlModule.hpp"

namespace inf
{
	namespace
	{
		// TODO(硬件): 确认上/下光源对应的 Modbus 线圈地址。
		constexpr int kUpperLightCoil = 0;
		constexpr int kLowerLightCoil = 1;
	}

	void LightIOModule::build()
	{
		// 初始状态关闭；硬件可用时同步到 PLC。
		setUpperLight(false);
		setLowerLight(false);
	}

	void LightIOModule::destroy()
	{
		// 退出时关闭光源（若硬件可用）
		setUpperLight(false);
		setLowerLight(false);
		control_ = nullptr;
	}

	void LightIOModule::setControlModule(ControlModule* control)
	{
		control_ = control;
	}

	void LightIOModule::setUpperLight(bool on)
	{
		upperLight_.store(on, std::memory_order_release);
		if (control_ && control_->isConnected())
			control_->writeCoil(kUpperLightCoil, on);
	}

	void LightIOModule::setLowerLight(bool on)
	{
		lowerLight_.store(on, std::memory_order_release);
		if (control_ && control_->isConnected())
			control_->writeCoil(kLowerLightCoil, on);
	}

	bool LightIOModule::getUpperLightState() const
	{
		return upperLight_.load(std::memory_order_acquire);
	}

	bool LightIOModule::getLowerLightState() const
	{
		return lowerLight_.load(std::memory_order_acquire);
	}
}
