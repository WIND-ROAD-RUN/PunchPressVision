#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"
#include "infTool/CalibInfTool/CalibInfTool.hpp"

#include "halconcpp/HalconCpp.h"

namespace infTool
{
	class TwoCameraSpliceInfTool
		: public global::IInfTool
	{
		Q_OBJECT
	public:
		explicit TwoCameraSpliceInfTool(inf::infrastructure& inf, CalibInfTool& calibTool);
	private:
		inf::infrastructure& inf_;
		CalibInfTool& calib_tool_;
	public:
		void calculateTwoCameraSpliceConfig(const HalconCpp::HImage& himage1, const HalconCpp::HImage& himage2);
		bool calibImage(HalconCpp::HObject image1, HalconCpp::HObject image2,
			const Config::CalibConfigItem& cam1Calib, const Config::CalibConfigItem& cam2Calib,
			Config::TwoCameraSpliceCfg& spliceCfg,
			std::string* errorMsg = nullptr);
		bool pinjieImage(HalconCpp::HObject& image1, HalconCpp::HObject& image2,
			Config::TwoCameraSpliceCfg& spliceCfg,
			HalconCpp::HObject& stitchedImage);

	signals:
		/// 拼接/平铺后的输出图像。双相机均就绪时发射。
		void callBackFunc(HalconCpp::HImage img);

	private slots:
		/// 接收 CalibInfTool 矫正后的单相机图像，按相机索引缓存。
		/// 双路就绪时执行拼接 (pinjieImage) 或硬拼接 (TileImages) 降级。
		void onCalibFrame(HalconCpp::HImage img, global::CameraIndex cameraIndex);

	private:
		HalconCpp::HImage cam1_image_;
		HalconCpp::HImage cam2_image_;
		bool cam1_ready_{ false };
		bool cam2_ready_{ false };

	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
