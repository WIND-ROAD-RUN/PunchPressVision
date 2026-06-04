#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace inf
{
	namespace Config
	{
		class NinePointCfg
		{
			//TODO:完成畸变矫正的序列化和反序列化以及参数


			//九点标定的参数
			//测量矩形的参数
			double MeasureLength1 = 100;
			double MeasureLength2 = 50;
			double MeasureThreshold = 1;
			double num_Measure = 5;

		//九点标定换算的矩阵
			HalconCpp::HTuple outHomMat2D ;




		public:
			void saveInDir(const std::string& dirPath);
			void loadInDir(const std::string& dirPath);
		};
	}
}
