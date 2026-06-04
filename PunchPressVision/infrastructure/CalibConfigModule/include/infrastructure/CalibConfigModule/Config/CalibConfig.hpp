#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace inf
{
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
			void saveInDir(const std::string& dirPath);
			void loadInDir(const std::string& dirPath);
		};
	}
}
