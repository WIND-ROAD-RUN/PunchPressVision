#include "infrastructure/CameraModule/CameraModule.hpp"

namespace inf
{
	void CameraModule::build()
	{
		//TODO:这里构建相机
		//参考如下
        /*auto& camPtr = cameraMap[item.workType];
        if (!camPtr)
        {
            camPtr = std::make_unique<rw::rqwc::MVSCameraPassive>();
        }

        auto& cameraCfg = getCameraConfig(item.workType);

        camPtr->setIP(cameraCfg.ip);
        const auto isConnect = camPtr->connectCamera();

        if (!isConnect)
        {
            const auto reason = QString("connect failed, ip=%1").arg(QString::fromStdString(cameraCfg.ip));
            connectionState_[item.workType] = false;
            connectionReason_[item.workType] = reason;
            emit cameraConnectionStateChanged(item.workType, false, reason);
            continue;
        }

        connectionState_[item.workType] = true;
        connectionReason_[item.workType] = "ok";
        emit cameraConnectionStateChanged(item.workType, true, "ok");

        camPtr->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::ON);
        camPtr->setHeartbeatTime(cameraCfg.heartbeatTime);
        camPtr->setFrameRate(60);

        QObject::connect(camPtr.get(), &rw::rqwc::MVSCameraPassive::callBackFuncPost,
            this, item.slot, Qt::DirectConnection);

        camPtr->registerCallBackFunc();*/
	}

	void CameraModule::destroy()
	{
		//TODO:这里销毁相机
	}
}
