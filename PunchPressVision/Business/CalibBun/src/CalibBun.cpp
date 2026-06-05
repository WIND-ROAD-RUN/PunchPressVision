#include "Business/CalibBun/CalibBun.hpp"

namespace bun
{
	CalibBun::CalibBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void CalibBun::calibCamera(const HalconCpp::HImage& himage)
	{
		Config::CalibConfig calibConfig;
		//TODO:实现通过himage标定相机畸变矫正结果

		inf_.calib_config_module_->calibConfig = calibConfig;
	}

	HalconCpp::HImage CalibBun::undistortImage(const HalconCpp::HImage& himage)
	{
		HalconCpp::HImage result;
		auto& cfg = inf_.calib_config_module_->calibConfig;
		//TODO:实现通过cfg中的相机内参和外参对himage进行畸变矫正，并将结果保存在result中

		return result;
	}

	void CalibBun::build()
	{
	}

	void CalibBun::destroy()
	{
	}

	void CalibBun::start()
	{
	}

	void CalibBun::stop()
	{
	}
}
