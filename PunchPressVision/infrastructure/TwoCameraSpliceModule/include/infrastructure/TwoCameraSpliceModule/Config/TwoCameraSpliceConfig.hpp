#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace inf
{
	namespace Config
	{
		class TwoCameraSpliceCfg
		{
			//TODO:完成双相机拼接的序列化和反序列化以及参数

           //========== 输入：用于标定的两张图片 ==========
			HalconCpp::HObject camera1Piccture; // 相机1拍摄的标定板图片
			HalconCpp::HObject camera2Piccture; // 相机2拍摄的标定板图片

			//========== 标定相关参数 ==========
			std::string caltabDescrPath;  // 标定板描述文件路径 (.descr)
			double DiffHeight;            // 标定板到平面距离差 = ThicknessPlate - ThicknessCaliper
			double OverlapInPercent;      // 拼接图像裁切百分比 (默认70)
			double BorderInPercent;       // 边界百分比 (默认7)
			double DistancePlates;        // 两个标定板之间的距离 (默认0)

			//========== 像素当量 ==========
			double pixTowWorld;           // 像素当量 (PixelSize，单位：米/像素)

			//========== 输出：图像拼接用的映射（map） ==========
			HalconCpp::HObject MapSingle1;  // 相机1的映射图
			HalconCpp::HObject MapSingle2;  // 相机2的映射图

			//========== 输出：图像尺寸信息（可选，用于后续处理） ==========
			int rectifiedWidth;           // 矫正后图像宽度
			int rectifiedHeight;          // 矫正后图像高度



		public:
			void saveInDir(const std::string& dirPath);
			void loadInDir(const std::string& dirPath);
		};
	}
}
