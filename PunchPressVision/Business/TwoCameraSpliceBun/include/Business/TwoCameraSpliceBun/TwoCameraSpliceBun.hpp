#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	class TwoCameraSpliceBun
		: public global::IBusiness
	{
	public:
		explicit TwoCameraSpliceBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void calculateTwoCameraSpliceConfig(const HalconCpp::HImage& himage1, const HalconCpp::HImage& himage2);
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
