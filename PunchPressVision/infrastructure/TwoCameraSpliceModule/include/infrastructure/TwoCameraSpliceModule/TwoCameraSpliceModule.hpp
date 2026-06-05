#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

namespace inf
{
	class TwoCameraSpliceModule
		: public global::IInfrastructure
	{
	public:
		Config::TwoCameraSpliceCfg twoCameraSpliceConfig;
	public:
		void build() override;
		void destroy() override;
		void save();
	};
}
