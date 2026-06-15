#pragma once

#include "halconcpp/HalconCpp.h"
#include "opencv2/core/mat.hpp"

namespace infTool
{
	struct CameraImgConvert
	{
		// 深拷贝版本：返回的 HImage 与 cv::Mat 生命周期无关，安全但多一次内存复制
		static HalconCpp::HImage cvMatToHImage(const cv::Mat& mat);
		// 零拷贝版本：HImage 直接引用 cv::Mat 内存，要求 mat 在 HImage 使用期间保持有效
		static HalconCpp::HImage cvMatToHImageFast(const cv::Mat& mat);
	};
}
