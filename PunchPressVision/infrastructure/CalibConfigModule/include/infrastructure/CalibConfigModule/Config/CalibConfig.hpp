#pragma once
#include <string>
#include <unordered_map>

#include "global/GlobalType.hpp"
#include "halconcpp/HalconCpp.h"

namespace Config
{
	/// 单个相机的标定参数
	class CalibConfigItem
	{
	public:
		// Intrinsic camera parameters for distortion correction
		HalconCpp::HTuple cameraParameters;
		// Extrinsic camera pose for distortion correction
		HalconCpp::HTuple cameraPose;

		// Calibration board description file path
		std::string calibBoardDescrPath{ "" };

		// Camera exposure
		double cameraExposure{ 10000 };
		// Camera gain
		double cameraGain{ 5 };

		// Calibration errors from CalibrateCameras
		HalconCpp::HTuple calibrationErrors;
		// Reference image index used during calibration
		int calibrationReferenceIndex{ 0 };
	};

	/// 双相机标定参数集合
	class CalibConfig
	{
	public:
		std::unordered_map<global::CameraIndex, CalibConfigItem> _calibConfigMap;

		/// 获取指定相机的配置项（不存在时自动创建默认值）
		CalibConfigItem& item(global::CameraIndex idx)
		{
			return _calibConfigMap[idx];
		}

		const CalibConfigItem& item(global::CameraIndex idx) const
		{
			static CalibConfigItem s_default;
			auto it = _calibConfigMap.find(idx);
			return it != _calibConfigMap.end() ? it->second : s_default;
		}

		void saveInDir(const std::string& dirPath);
		void loadInDir(const std::string& dirPath);
	};
}
