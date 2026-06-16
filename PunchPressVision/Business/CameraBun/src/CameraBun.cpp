#include "Business/CameraBun/CameraBun.hpp"

namespace bun
{
	CameraBun::CameraBun(inf::infrastructure& inf, infTool::infTool& infTool)
		: inf_(inf)
		, inf_tool_(infTool)
	{
		// 构造阶段连接信号（确保 build 阶段 CameraModule 发射状态时已就绪）
		if (inf_.camera_module_)
		{
			QObject::connect(inf_.camera_module_.get(), &inf::CameraModule::cameraConnectionStateChanged,
				this, &CameraBun::cameraConnectionStateChanged, Qt::QueuedConnection);
		}

		// 从 TwoCameraSpliceInfTool 接收已处理的图像流（畸变矫正 + 拼接/平铺 已完成）
		if (inf_tool_.two_camera_splice_bun)
		{
			QObject::connect(inf_tool_.two_camera_splice_bun.get(), &infTool::TwoCameraSpliceInfTool::callBackFunc,
				this, &CameraBun::onSplicedFrame, Qt::QueuedConnection);
		}
	}

	void CameraBun::build()
	{
	}

	void CameraBun::destroy()
	{
		if (inf_.camera_module_)
			QObject::disconnect(inf_.camera_module_.get(), nullptr, this, nullptr);
		if (inf_tool_.two_camera_splice_bun)
			QObject::disconnect(inf_tool_.two_camera_splice_bun.get(), nullptr, this, nullptr);
	}

	void CameraBun::start()
	{
		// 启动相机取流（帧经 infTool 流水线处理后由 onSplicedFrame 接收）
		if (inf_.camera_module_)
			inf_.camera_module_->startMonitor();
	}

	void CameraBun::stop()
	{
		if (inf_.camera_module_)
			inf_.camera_module_->stopMonitor();
	}

	void CameraBun::onSplicedFrame(HalconCpp::HImage img)
	{
		// 纯转发：图像处理已在 infTool 层（CalibInfTool → TwoCameraSpliceInfTool）完成
		if (img.IsInitialized())
			emit callBackFunWithCalib(img);
	}
}
