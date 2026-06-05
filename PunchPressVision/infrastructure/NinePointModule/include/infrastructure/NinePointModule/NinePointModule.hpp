#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/NinePointModule/Config/NinePointConfig.hpp"

namespace inf
{
	class NinePointModule
		: public global::IInfrastructure
	{
	public:
		Config::NinePointCfg ninePointConfig;
	public:
		void build() override;
		void destroy() override;
		void save();
	};
}
