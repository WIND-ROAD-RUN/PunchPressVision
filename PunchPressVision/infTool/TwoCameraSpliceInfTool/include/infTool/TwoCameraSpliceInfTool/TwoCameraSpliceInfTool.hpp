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
			Config::CalibConfig cam1Calib, Config::CalibConfig cam2Calib,
			Config::TwoCameraSpliceCfg& spliceCfg,
			std::string* errorMsg = nullptr);
		bool pinjieImage(HalconCpp::HObject& image1, HalconCpp::HObject& image2,
			Config::TwoCameraSpliceCfg& spliceCfg,
			HalconCpp::HObject& stitchedImage);
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
