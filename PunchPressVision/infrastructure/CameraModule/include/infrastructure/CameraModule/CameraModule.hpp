#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <unordered_map>

#include <rwul/hoecm/hoec_m.hpp>

#include "global/GlobalType.hpp"
#include "global/GlobalInterface.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "infrastructure/ConfigModule/Config/baseCfg.hpp"

namespace inf
{
	class CameraModule
		: public QObject, public global::IInfrastructure
	{
		Q_OBJECT
	public:
		std::unordered_map<global::CameraIndex, std::unique_ptr<rw::hoec::MVSCameraPassive>> cameras_{};
		Config::cameraCfg cameraCfg;
		// 相机 IP / PLC 等基础配置（由 ConfigModule 注入）
		Config::BaseCfg baseCfg;
	public:
		void build() override;
		void destroy() override;

		// 相机控制接口（FR-005 / FR-008 / FR-011）
		bool setFreeRunMode(global::CameraIndex idx, double fps);
		bool setTriggerMode(global::CameraIndex idx, global::TriggerSource source, double fps);
		bool setExposure(global::CameraIndex idx, double microseconds);
		bool setGain(global::CameraIndex idx, double value);
		bool captureSingleFrame(global::CameraIndex idx, rw::hoec::MatInfo& out);

		// 查询连接状态
		bool isConnected(global::CameraIndex idx) const;

	private:
		rw::hoec::MVSCameraPassive* camera(global::CameraIndex idx) const;

	signals:
		//所有相机的回调函数都连接到这个槽函数上，区分相机通过cameraIndex参数，信号使用Qt::DirectConnection连接
		void callBackFunc(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
		//相机连接状态改变的信号，connected表示连接状态，reason表示状态改变的原因
		void cameraConnectionStateChanged(global::CameraIndex cameraIndex, bool connected, QString reason);
	};
}
