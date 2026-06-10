#pragma once

#include <QDialog>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "UI/HalconView.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_ImageStitchingClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	// 双相机拼接对话框（FR-019 ~ FR-020）。复用旧 Dlg_ImageStitching.ui。
	class SpliceDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit SpliceDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~SpliceDialog() override;

	private slots:
		void onFrameReady(HalconCpp::HImage image, global::CameraIndex camIdx);
		void onSplice();   // 执行拼接标定（FR-020）

	private:
		void buildConnections();

		Ui::Dlg_ImageStitchingClass* ui;
		app::PunchPressApp& app_;
		HalconCpp::HImage cam1Frame_;
		HalconCpp::HImage cam2Frame_;
		HalconView view1_;
		HalconView view2_;
	};
}
