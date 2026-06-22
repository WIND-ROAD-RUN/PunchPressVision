#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace Config
{
	class NinePointCfg
	{
	public:
		// Measurement rectangle parameters
		double MeasureLength1 = 100;
		double MeasureLength2 = 50;
		double MeasureThreshold = 1;
		double num_Measure = 5;

		// Camera 1 exposure/gain
		double camera1Exposure = 5000;
		double camera1Gain = 1;

		// Camera 2 exposure/gain
		double camera2Exposure = 5000;
		double camera2Gain = 1;

		// Homography matrix from nine-point calibration
		HalconCpp::HTuple outHomMat2D;

		// Calibration plate parameters
		int xnumber = 7;
		int ynumber = 7;
		double distance = 0.007;
		double scale = 0.5;

		int xdiantance = 500;
		int ydistance = 100;
		double xoffset = 400;

		void saveInDir(const std::string& dirPath);
		void loadInDir(const std::string& dirPath);
	public:

	};
}
