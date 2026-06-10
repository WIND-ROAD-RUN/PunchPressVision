#pragma once

#include <QDialog>
#include <vector>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "UI/HalconView.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_jibianjiaozhengClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	// 畸变矫正标定对话框（FR-011 ~ FR-014）。复用旧 Dlg_jibianjiaozheng.ui 布局。
	class DistortionCalibDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit DistortionCalibDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~DistortionCalibDialog() override;

	private slots:
		void onFrameReady(HalconCpp::HImage image, global::CameraIndex camIdx);
		void onCapture();        // 采集当前帧加入标定图集
		void onRemoveLast();     // 移除最后一张
		void onRemoveAll();      // 清空
		void onCalibrate();      // 执行标定并持久化（FR-013/FR-014）

	private:
		void buildConnections();

		Ui::Dlg_jibianjiaozhengClass* ui;
		app::PunchPressApp& app_;
		std::vector<HalconCpp::HImage> capturedImages_;
		HalconCpp::HImage lastFrame_;
		HalconView view_;
	};
}
