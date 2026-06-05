#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	class CalibBun
		: public global::IBusiness
	{
	public:
		explicit CalibBun(inf::infrastructure& inf);
	public:
		void calibCamera(const HalconCpp::HImage & himage);
		HalconCpp::HImage undistortImage(const HalconCpp::HImage& himage);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
