#pragma once

#include "global/GlobalInterface.hpp"
#include"infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

namespace inf
{
	class CalibConfigModule
		:public global::IInfrastructure
	{
	public:
		Config::CalibConfig calibConfig;
	public:
		void build() override;
		void destroy() override;
		void save();
	};
}
