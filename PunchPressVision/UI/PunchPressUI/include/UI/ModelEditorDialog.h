#pragma once

#include <QDialog>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "UI/ShapeEditor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_createshapemodelClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }
class QPushButton;

namespace ui
{
	// 模板训练对话框（FR-023 ~ FR-026、FR-028）。复用旧 Dlg_createshapemodel.ui。
	class ModelEditorDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit ModelEditorDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~ModelEditorDialog() override;

	private slots:
		void onFrameReady(HalconCpp::HImage image);
		void onToolChanged(ShapeEditor::Tool tool);

		// 形状编辑
		void onPaintRegion();
		void onPaintMask();
		void onPaintCenterPoint();
		void onClearRegion();
		void onUndo();

		// 相机参数
		void onGain1Clicked();
		void onExposure1Clicked();
		void onGain2Clicked();
		void onExposure2Clicked();

		// 模型参数
		void onOpeningClicked();
		void onClosingClicked();
		void onMeanClicked();
		void onContrastAutoToggled(bool checked);

		// 操作
		void onCreateModel();
		void onReadImage();

	private:
		void buildConnections();
		void loadCameraParams();
		void updateCameraParamButtons();

		// 数字键盘输入辅助
		bool inputIntParam(QPushButton* button, int& value, int min, int max,
		                   const QString& title = QString());
		bool inputDoubleParam(QPushButton* button, double& value, double min, double max,
		                      int decimals, const QString& title = QString());

		// 模型参数回写
		void applyExposure(global::CameraIndex idx, int value,
		                   int Config::cameraCfg::* member);
		void applyGain(global::CameraIndex idx, int value,
		               int Config::cameraCfg::* member);

		void updateToolButtons(ShapeEditor::Tool tool);
		void updateContrastVisibility();

		Ui::Dlg_createshapemodelClass* ui;
		app::PunchPressApp& app_;
		ui::ShapeEditor* shapeEditor_{ nullptr };
		HalconCpp::HImage lastFrame_;
		global::RunMode previousMode_{ global::RunMode::Idle };

		// 本地缓存的相机配置
		Config::cameraCfg cameraCfg_;

		// 模型训练参数
		int openingSize_{ 5 };
		int closingSize_{ 5 };
		int meanSize_{ 5 };
		bool useOpening_{ false };
		bool useClosing_{ false };
		bool useMean_{ false };
		bool contrastAuto_{ true };
		int contrast_{ 30 };
		int minContrast_{ 10 };
	};
}
