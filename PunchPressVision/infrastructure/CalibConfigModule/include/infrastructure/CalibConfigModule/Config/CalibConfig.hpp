#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace inf
{
	namespace Config
	{
		class CalibConfig
		{
			//TODO:完成畸变矫正的序列化和反序列化以及参数

			//畸变矫正的内参
			HalconCpp::HTuple cameraParameters ;
			//畸变矫正的外参
			HalconCpp::HTuple cameraPose ;

			//相机的曝光
			double cameraExposure {10000};
			//相机的增益
			double cameraGain {5};


		public:
			void saveInDir(const std::string & dirPath);
			void loadInDir(const std::string & dirPath);
		};
	}
}
