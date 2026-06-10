#include "infrastructure/CameraModule/CameraModule.hpp"

namespace inf
{
	namespace
	{
		// global::TriggerSource → rw::rqwc::TriggerSource(底层枚举为 Software/Line0/Line2/Counter0)
		rw::rqwc::TriggerSource toRwTriggerSource(global::TriggerSource src)
		{
			switch (src)
			{
			case global::TriggerSource::Line0: return rw::rqwc::TriggerSource::Line0;
			case global::TriggerSource::Line1: return rw::rqwc::TriggerSource::Line2; // 底层无 Line1，退化到 Line2
			case global::TriggerSource::Line2: return rw::rqwc::TriggerSource::Line2;
			case global::TriggerSource::Software:
			default: return rw::rqwc::TriggerSource::Software;
			}
		}
	}

	rw::rqwc::MVSCameraPassive* CameraModule::camera(global::CameraIndex idx) const
	{
		auto it = cameras_.find(idx);
		return it == cameras_.end() ? nullptr : it->second.get();
	}

	void CameraModule::build()
	{
		// 为两台相机创建被动采集控件，注册回调桥接到 callBackFunc 信号。
		// 相机 IP 来自 baseCfg；未配置/无设备时连接失败仅告警，不阻塞启动。
		struct CamInit
		{
			global::CameraIndex index;
			std::string ip;
		};
		const std::vector<CamInit> inits = {
			{ global::CameraIndex::Camera1, baseCfg.cameraIp1 },
			{ global::CameraIndex::Camera2, baseCfg.cameraIp2 },
		};

		for (const auto& init : inits)
		{
			auto cam = std::make_unique<rw::rqwc::MVSCameraPassive>();
			const global::CameraIndex idx = init.index;

			// 回调：底层 MatInfo → Qt 信号（DirectConnection 由订阅方指定）
			cam->setCallBackFuncPost(
				[this, idx](rw::rqwc::MatInfo& matInfo)
				{
					emit callBackFunc(matInfo, idx);
				});

			// TODO(硬件): 确认实际相机 IP；当前从 baseCfg 读取。
			cam->setIP(init.ip);

			bool connected = false;
			try
			{
				connected = cam->connectCamera();
			}
			catch (...)
			{
				connected = false;
			}

			if (!connected)
			{
				const QString reason = QString("连接相机失败, ip=%1")
					.arg(QString::fromStdString(init.ip));
				emit cameraConnectionStateChanged(idx, false, reason);
				// 仍保留控件实例，便于后续重连/参数设置
				cameras_[idx] = std::move(cam);
				continue;
			}

			// 默认自由运行模式，曝光/增益取自 cameraCfg
			cam->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::OFF);
			const int exposure = (idx == global::CameraIndex::Camera1)
				? cameraCfg.exposureTime1 : cameraCfg.exposureTime2;
			const int gain = (idx == global::CameraIndex::Camera1)
				? cameraCfg.gain1 : cameraCfg.gain2;
			cam->setExposureTime(static_cast<rw::rqwc::UInt>(exposure));
			cam->setGain(static_cast<rw::rqwc::UInt>(gain));
			cam->setFrameRate(5.0f);

			(void)cam->registerCallBackFunc();
			(void)cam->startMonitor();

			emit cameraConnectionStateChanged(idx, true, "ok");
			cameras_[idx] = std::move(cam);
		}
	}

	void CameraModule::destroy()
	{
		for (auto& [idx, cam] : cameras_)
		{
			if (!cam)
				continue;
			try
			{
				(void)cam->stopMonitor();
				(void)cam->disconnectCamera();
			}
			catch (...)
			{
			}
		}
		cameras_.clear();
	}

	bool CameraModule::setFreeRunMode(global::CameraIndex idx, double fps)
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		if (!cam->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::OFF))
			return false;
		return cam->setFrameRate(static_cast<float>(fps));
	}

	bool CameraModule::setTriggerMode(global::CameraIndex idx, global::TriggerSource source, double fps)
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		if (!cam->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::ON))
			return false;
		if (!cam->setTriggerSource(toRwTriggerSource(source)))
			return false;
		return cam->setFrameRate(static_cast<float>(fps));
	}

	bool CameraModule::setExposure(global::CameraIndex idx, double microseconds)
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		return cam->setExposureTime(static_cast<rw::rqwc::UInt>(microseconds));
	}

	bool CameraModule::setGain(global::CameraIndex idx, double value)
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		return cam->setGain(static_cast<rw::rqwc::UInt>(value));
	}

	bool CameraModule::captureSingleFrame(global::CameraIndex idx, rw::rqwc::MatInfo& out)
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		// 被动相机通过软触发取一帧；结果经回调返回，此处发起触发。
		// TODO(硬件): 若需同步取帧，可改用 rw::rqwc::MVSCameraActive::captureImage。
		(void)out;
		try
		{
			return cam->executeSoftwareTrigger();
		}
		catch (...)
		{
			return false;
		}
	}

	bool CameraModule::isConnected(global::CameraIndex idx) const
	{
		auto* cam = camera(idx);
		if (!cam)
			return false;
		bool isGet = false;
		bool state = false;
		try
		{
			state = cam->getConnectState(isGet);
		}
		catch (...)
		{
			return false;
		}
		return isGet && state;
	}
}
