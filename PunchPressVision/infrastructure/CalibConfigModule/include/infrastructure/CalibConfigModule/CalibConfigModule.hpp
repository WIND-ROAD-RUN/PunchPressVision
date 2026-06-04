#pragma once

#include"infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

namespace inf
{
	class CalibConfigModule
	{
	public:
		Config::CalibConfig calibConfig;
	public:
		void build();
		void destroy();
		void save();
	};
}
