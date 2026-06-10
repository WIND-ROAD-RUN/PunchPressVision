#pragma once

#include <atomic>

#include "global/GlobalInterface.hpp"

namespace inf
{
	class ControlModule;

	// 光源 IO 模块：上/下光源开关控制（FR-021、FR-022）。
	// 底层通过 ControlModule 写 Modbus 线圈/寄存器实现。
	class LightIOModule
		: public global::IInfrastructure
	{
	public:
		void build() override;
		void destroy() override;

		// 注入 PLC 控制网关（由 infrastructure 聚合根装配）
		void setControlModule(ControlModule* control);

		void setUpperLight(bool on);
		void setLowerLight(bool on);
		bool getUpperLightState() const;
		bool getLowerLightState() const;

	private:
		ControlModule* control_{ nullptr };
		std::atomic_bool upperLight_{ false };
		std::atomic_bool lowerLight_{ false };
	};
}
