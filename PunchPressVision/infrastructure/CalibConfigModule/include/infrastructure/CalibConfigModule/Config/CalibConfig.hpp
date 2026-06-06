#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"

namespace Config
{
	class CalibConfig
	{
	private:
		// Intrinsic camera parameters for distortion correction
		HalconCpp::HTuple cameraParameters;
		// Extrinsic camera pose for distortion correction
		HalconCpp::HTuple cameraPose;

		// Camera exposure
		double cameraExposure{ 10000 };
		// Camera gain
		double cameraGain{ 5 };

	public:
		const HalconCpp::HTuple& getCameraParameters() const { return cameraParameters; }
		const HalconCpp::HTuple& getCameraPose() const { return cameraPose; }
		void setCameraParameters(const HalconCpp::HTuple& params) { cameraParameters = params; }
		void setCameraPose(const HalconCpp::HTuple& pose) { cameraPose = pose; }

		double getCameraExposure() const { return cameraExposure; }
		double getCameraGain() const { return cameraGain; }
		void setCameraExposure(double exposure) { cameraExposure = exposure; }
		void setCameraGain(double gain) { cameraGain = gain; }

		void saveInDir(const std::string& dirPath);
		void loadInDir(const std::string& dirPath);
	};
}
