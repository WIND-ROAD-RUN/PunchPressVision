#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	class CameraBun
		: public global::IBusiness
	{
	public:
		explicit CameraBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	public slots:
		void callBackFunc(rw::rqwc::MatInfo matInfo, global::CameraIndex cameraIndex);
	signals:
		void cameraConnectionStateChanged(global::CameraIndex cameraIndex, bool connected, QString reason);
		void callBackFunWithCalib(HalconCpp::HImage matInfo, global::CameraIndex cameraIndex);
	};
}
