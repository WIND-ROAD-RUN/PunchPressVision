#pragma once

#include "halconcpp/HalconCpp.h"
#include "opencv2/core/mat.hpp"

namespace bun
{
	struct CameraImgConvert
	{
		static HalconCpp::HImage cvMatToHImage(const cv::Mat& mat);
	};
}
