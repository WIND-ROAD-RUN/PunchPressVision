#pragma once

#include "infrastructure/NinePointModule/Config/NinePointConfig.hpp"

namespace inf
{
	class NinePointModule
	{
	public:
		Config::NinePointCfg ninePointConfig;
	public:
		void build();
		void destroy();
		void save();
	};
}
