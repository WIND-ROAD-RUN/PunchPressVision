#pragma once

#include <QDialog>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "UI/HalconView.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DlgJiudianbiaodingClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	// 九点标定对话框（FR-015 ~ FR-018）。复用旧 DlgJiudianbiaoding.ui。
	class NinePointCalibDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit NinePointCalibDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~NinePointCalibDialog() override;

	private slots:
		void onFrameReady(HalconCpp::HImage image, global::CameraIndex camIdx);
		void onStartCalibration();
		void onCalibrationProgress(int currentPoint, int totalPoints);
		void onCalibrationFinished(bool success, const QString& message);

	private:
		void buildConnections();

		Ui::DlgJiudianbiaodingClass* ui;
		app::PunchPressApp& app_;
		HalconCpp::HImage lastFrame_;
		HalconView view_;
	};
}
