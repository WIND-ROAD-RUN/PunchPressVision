#pragma once

#include <QDialog>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"
#include "UI/ShapeEditor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_createshapemodelClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }
class QPushButton;

namespace ui
{
	class ModelEditorDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit ModelEditorDialog(app::PunchPressApp& app, bool isModifyMode,
		                         const std::string& modelId = {}, QWidget* parent = nullptr);
		~ModelEditorDialog() override;

	protected:
		void showEvent(QShowEvent* e) override;

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
		void onRecognize();
		void onCreateModel();
		void onReadImage();

	private:
		void buildConnections();
		void loadCameraParams();
		void updateCameraParamButtons();

		bool inputIntParam(QPushButton* button, int& value, int min, int max,
		                   const QString& title = QString());
		bool inputDoubleParam(QPushButton* button, double& value, double min, double max,
		                      int decimals, const QString& title = QString());

		void applyExposure(global::CameraIndex idx, int value,
		                   int Config::cameraCfg::* member);
		void applyGain(global::CameraIndex idx, int value,
		               int Config::cameraCfg::* member);

		void updateToolButtons(ShapeEditor::Tool tool);
		void updateContrastVisibility();
		void refreshProcessedImage();

		/// 修改模式：从 modelId 加载已有模型，恢复原始图、ROI/屏蔽区域、中心点和参数
		void loadExistingModel(const std::string& id);
		/// 根据已加载模型的数据恢复界面参数
		void restoreParamsFromModel(const Config::ShapeModelData& data);

		HalconCpp::HImage preprocessImage(const HalconCpp::HImage& image) const;

		// 构建 CreateModelRequest（供识别和创建共用）
		bun::CreateModelRequest buildRequest(const HalconCpp::HImage& preprocessedImage,
		                                   const HalconCpp::HImage& rawImage) const;

		// ROI 缺失提示
		bool requireROI(const QString& action) const;

		Ui::Dlg_createshapemodelClass* ui;
		app::PunchPressApp& app_;
		ui::ShapeEditor* shapeEditor_{ nullptr };
		HalconCpp::HImage lastFrame_;
		global::RunMode previousMode_{ global::RunMode::Idle };

		bool isModifyMode_{ false };
		std::string modelId_;
		bool modelLoaded_{ false };   ///< 确保 loadExistingModel 仅执行一次

		Config::cameraCfg cameraCfg_;

		// 模型训练参数
		int openingSize_{ 5 };
		int closingSize_{ 5 };
		int meanSize_{ 5 };
		bool contrastAuto_{ true };
		int contrast_{ 30 };
		int minContrast_{ 10 };
	};
}
