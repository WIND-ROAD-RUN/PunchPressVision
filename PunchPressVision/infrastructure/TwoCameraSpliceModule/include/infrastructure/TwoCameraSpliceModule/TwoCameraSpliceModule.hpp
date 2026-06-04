#pragma once

#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

namespace inf
{
	class TwoCameraSpliceModule
	{
	public:
		Config::TwoCameraSpliceCfg twoCameraSpliceConfig;
	public:
		void build();
		void destroy();
		void save();
	};
}
