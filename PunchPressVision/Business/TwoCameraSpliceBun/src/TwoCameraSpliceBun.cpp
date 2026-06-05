#include "Business/TwoCameraSpliceBun/TwoCameraSpliceBun.hpp"

namespace bun
{
	TwoCameraSpliceBun::TwoCameraSpliceBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void TwoCameraSpliceBun::calculateTwoCameraSpliceConfig(const HalconCpp::HImage& himage1,
		const HalconCpp::HImage& himage2)
	{
		Config::TwoCameraSpliceCfg result;
		//TODO: 补充双相机拼接的算法
		inf_.two_camera_splice_module_->twoCameraSpliceConfig = result;
	}

	void TwoCameraSpliceBun::build()
	{
	}

	void TwoCameraSpliceBun::destroy()
	{
	}

	void TwoCameraSpliceBun::start()
	{
	}

	void TwoCameraSpliceBun::stop()
	{
	}
}
