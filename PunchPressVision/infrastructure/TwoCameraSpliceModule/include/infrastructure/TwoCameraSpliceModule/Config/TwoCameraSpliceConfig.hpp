#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace Config
{
	class TwoCameraSpliceCfg
	{
	public:
		// Input: calibration images from both cameras
		HalconCpp::HObject camera1Piccture; // calibration image from camera 1
		HalconCpp::HObject camera2Piccture; // calibration image from camera 2

		


		// Calibration related parameters
		std::string caltabDescrPath;  // calibration plate description file path (.descr)
		double camera1Gain{ 0 };      // camera 1 gain
		double camera1Exposure{ 0 };  // camera 1 exposure time (us)
		double camera2Gain{ 0 };      // camera 2 gain
		double camera2Exposure{ 0 };  // camera 2 exposure time (us)
		double DiffHeight{ 0 };       // height difference between plate and caliper
		double OverlapInPercent{ 0 }; // overlap cropping percentage (default 70)
		double BorderInPercent{ 7 };  // border percentage (default 7)
		double DistancePlates{ 0 };   // distance between two calibration plates (default 0)

		// Pixel equivalent (PixelSize, unit: meter/pixel)
		double pixTowWorld{ 0 };

		// Output: mapping images for splicing
		HalconCpp::HObject MapSingle1; // mapping image for camera 1
		HalconCpp::HObject MapSingle2; // mapping image for camera 2

		// Output: rectified image size information
		int rectifiedWidth{ 0 };  // width after rectification
		int rectifiedHeight{ 0 }; // height after rectification

		void saveInDir(const std::string& dirPath);
		void loadInDir(const std::string& dirPath);
	};
}