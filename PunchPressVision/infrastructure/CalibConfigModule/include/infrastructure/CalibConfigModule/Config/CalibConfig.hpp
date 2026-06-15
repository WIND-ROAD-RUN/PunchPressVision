#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"

namespace Config
{
	class CalibConfig
	{
	public:
		// Intrinsic camera parameters for distortion correction
		HalconCpp::HTuple cameraParameters;
		// Extrinsic camera pose for distortion correction
		HalconCpp::HTuple cameraPose;


		// Camera exposure
		double cameraExposure{ 10000 };
		// Camera gain
		double cameraGain{ 5 };

		// Calibration errors from CalibrateCameras
		HalconCpp::HTuple calibrationErrors;
		// Reference image index used during calibration
		int calibrationReferenceIndex{ 0 };

		void saveInDir(const std::string& dirPath);
		void loadInDir(const std::string& dirPath);
	};
}
