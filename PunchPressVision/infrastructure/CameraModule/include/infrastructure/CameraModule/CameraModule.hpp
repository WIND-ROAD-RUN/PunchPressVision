#pragma once

#include <rwul/rqwcm/rqwc_m.hpp>

#include "global/GlobalType.hpp"
#include "global/GlobalInterface.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"

namespace inf
{
	class CameraModule
		: public QObject, public global::IInfrastructure
	{
		Q_OBJECT
	public:
		std::unordered_map<global::CameraIndex, std::unique_ptr<rw::rqwc::MVSCameraPassive>> cameras_{};
		Config::cameraCfg cameraCfg;
	public:
		void build() override;
		void destroy() override;
	signals:
		//所有相机的回调函数都连接到这个槽函数上，区分相机通过cameraIndex参数，信号使用Qt::DirectConnection连接
		void callBackFunc(rw::rqwc::MatInfo matInfo, global::CameraIndex cameraIndex);
		//相机连接状态改变的信号，connected表示连接状态，reason表示状态改变的原因
		void cameraConnectionStateChanged(global::CameraIndex cameraIndex, bool connected, QString reason);
	};
}
