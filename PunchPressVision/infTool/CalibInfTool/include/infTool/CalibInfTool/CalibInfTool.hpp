#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"
#include "rwul/hoecm/hoec_m.hpp"

#include <vector>

namespace infTool
{
	class CalibInfTool
		:public global::IInfTool
	{
		Q_OBJECT

	public:
		explicit CalibInfTool(inf::infrastructure& inf);
	public:
		void calibCamera(const std::vector<HalconCpp::HImage>& himages,
			Config::CalibConfigItem& item);
		bool calibrateFromImages(const std::vector<HalconCpp::HImage>& himages,
			double focalLengthMm,
			double plateThicknessMm,
			Config::CalibConfigItem& item,
			int referenceIndex = 0,
			std::string* errorMsg = nullptr);
		bool drawCalibMarks(const HalconCpp::HImage& src,
			Config::CalibConfigItem& item,
			bool& isOk,
			HalconCpp::HObject& outMarksXld,
			HalconCpp::HObject& outMarksRegion,
			std::string* errorMsg = nullptr);
		HalconCpp::HImage undistortImage(const HalconCpp::HImage& himage,
			global::CameraIndex cameraIndex = global::CameraIndex::Camera1);

	public slots:
		void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);

	signals:
		void callBackFunc(HalconCpp::HImage img, global::CameraIndex cameraIndex);

	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
