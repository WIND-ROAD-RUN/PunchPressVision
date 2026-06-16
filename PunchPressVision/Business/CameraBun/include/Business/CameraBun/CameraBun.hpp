#pragma once

#include <QObject>

#include "global/GlobalInterface.hpp"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infTool/infTool.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	// 相机业务编排：桥接 infTool 层已完成图像处理的帧流到 App 层。
	// 图像处理（畸变矫正 + 双相机拼接/平铺）已下沉到
	//   CalibInfTool → TwoCameraSpliceInfTool 流水线，
	// CameraBun 不再做图像处理，只负责信号转发和相机监控启停。
	class CameraBun
		: public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit CameraBun(inf::infrastructure& inf, infTool::infTool& infTool);

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

	public slots:
		/// 接收 TwoCameraSpliceInfTool 输出的已处理图像（已矫正 + 已拼接/平铺）
		void onSplicedFrame(HalconCpp::HImage img, global::CameraIndex cameraIndex);

	signals:
		void cameraConnectionStateChanged(global::CameraIndex cameraIndex, bool connected, QString reason);
		/// 已处理图像流，下游（App → UI）通过此信号接收帧
		void callBackFunWithCalib(HalconCpp::HImage img, global::CameraIndex cameraIndex);

	private:
		inf::infrastructure& inf_;
		infTool::infTool& inf_tool_;
	};
}
