#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"
#include <vector>

namespace infTool
{
	class CalibBun
		: public global::IBusiness
	{
	public:
		explicit CalibBun(inf::infrastructure& inf);
	public:
		void calibCamera(const std::vector<HalconCpp::HImage>& himages);
		bool calibrateFromImages(const std::vector<HalconCpp::HImage>& himages,
			double focalLengthMm,
			double plateThicknessMm,
			int referenceIndex = 0,
			std::string* errorMsg = nullptr);
		bool drawCalibMarks(const HalconCpp::HImage& src,
			Config::CalibConfig& cfg,
			bool& isOk,
			HalconCpp::HObject& outMarksXld,
			HalconCpp::HObject& outMarksRegion,
			std::string* errorMsg = nullptr);
		HalconCpp::HImage undistortImage(const HalconCpp::HImage& himage);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
