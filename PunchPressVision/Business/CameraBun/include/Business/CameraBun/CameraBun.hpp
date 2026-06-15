#pragma once

#include <atomic>
#include <mutex>

#include <QObject>

#include "global/GlobalInterface.hpp"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infTool/infTool.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	// 相机业务编排：原始帧 → 格式转换 → 畸变矫正 → 九点变换 →（双相机拼接）→ 信号
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

		// 是否在流水线中启用拼接（仅双相机参数就绪时有效）
		void setSpliceEnabled(bool enabled);

	public slots:
		void callBackFunc(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);

	signals:
		void cameraConnectionStateChanged(global::CameraIndex cameraIndex, bool connected, QString reason);
		void callBackFunWithCalib(HalconCpp::HImage matInfo, global::CameraIndex cameraIndex);

	private:
		inf::infrastructure& inf_;
		infTool::infTool& inf_tool_;

		// 图像处理流水线
		HalconCpp::HImage applyUndistort(const HalconCpp::HImage& image);
		HalconCpp::HImage applyNinePointTransform(const HalconCpp::HImage& image);

		// 双相机拼接缓存
		std::mutex stitchMutex_;
		HalconCpp::HImage cam1Buffer_;
		HalconCpp::HImage cam2Buffer_;
		std::atomic_bool cam1Ready_{ false };
		std::atomic_bool cam2Ready_{ false };
		std::atomic_bool spliceEnabled_{ false };
	};
}
