#include "infTool/CameraBun/CameraImgConvert.hpp"

#include <stdexcept>
#include <cstring>

namespace infTool
{
	HalconCpp::HImage CameraImgConvert::cvMatToHImage(const cv::Mat& mat)
	{
		if (mat.empty())
			return HalconCpp::HImage();

		const int width = mat.cols;
		const int height = mat.rows;
		const int channels = mat.channels();

		// 确保数据连续，否则需要 clone
		cv::Mat continuousMat = mat.isContinuous() ? mat : mat.clone();

		if (channels == 1)
		{
			// 灰度图
			if (continuousMat.type() == CV_8UC1)
			{
				HalconCpp::HImage temp;
				HalconCpp::GenImage1(&temp, "byte", width, height, (Hlong)continuousMat.data);
				return temp.CopyImage();
			}
			else if (continuousMat.type() == CV_16UC1)
			{
				HalconCpp::HImage temp;
				HalconCpp::GenImage1(&temp, "uint2", width, height, (Hlong)continuousMat.data);
				return temp.CopyImage();
			}
		}
		else if (channels == 3)
		{
			// 彩色图 (OpenCV 默认 BGR 排列)
			if (continuousMat.type() == CV_8UC3)
			{
				HalconCpp::HImage temp;
				HalconCpp::GenImageInterleaved(&temp, (Hlong)continuousMat.data, "bgr",
					width, height, 0, "byte", width, height, 0, 0, -1, 0);
				return temp.CopyImage();
			}
		}
		else if (channels == 4)
		{
			// 带透明通道 (BGRA)
			if (continuousMat.type() == CV_8UC4)
			{
				HalconCpp::HImage temp;
				HalconCpp::GenImageInterleaved(&temp, (Hlong)continuousMat.data, "bgra",
					width, height, 0, "byte", width, height, 0, 0, -1, 0);
				return temp.CopyImage();
			}
		}

		throw std::runtime_error("Unsupported cv::Mat format for HImage conversion");
	}

	HalconCpp::HImage CameraImgConvert::cvMatToHImageFast(const cv::Mat& mat)
	{
		// 零拷贝：直接包装 mat.data 指针。调用方须保证 mat 在 HImage 使用期间有效。
		if (mat.empty())
			return HalconCpp::HImage();

		const int width = mat.cols;
		const int height = mat.rows;

		HalconCpp::HImage temp;
		switch (mat.type())
		{
		case CV_8UC1:
			HalconCpp::GenImage1(&temp, "byte", width, height, (Hlong)mat.data);
			return temp;
		case CV_16UC1:
			HalconCpp::GenImage1(&temp, "uint2", width, height, (Hlong)mat.data);
			return temp;
		case CV_8UC3:
			HalconCpp::GenImageInterleaved(&temp, (Hlong)mat.data, "bgr",
				width, height, 0, "byte", width, height, 0, 0, -1, 0);
			return temp;
		case CV_8UC4:
			HalconCpp::GenImageInterleaved(&temp, (Hlong)mat.data, "bgra",
				width, height, 0, "byte", width, height, 0, 0, -1, 0);
			return temp;
		default:
			throw std::runtime_error("Unsupported cv::Mat format for HImage conversion");
		}
	}
}
