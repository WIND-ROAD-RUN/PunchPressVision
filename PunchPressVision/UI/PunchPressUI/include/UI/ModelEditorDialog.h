#pragma once

#include <QDialog>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "UI/ShapeEditor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_createshapemodelClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

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
		void onPaintRegion();
		void onPaintMask();
		void onPaintCenterPoint();
		void onClearRegion();
		void onUndo();
		void onCreateModel();
		void onReadImage();

	private:
		void buildConnections();
		// 执行模型训练（供按钮调用）
		bool doCreateModel();

		Ui::Dlg_createshapemodelClass* ui;
		app::PunchPressApp& app_;
		ui::ShapeEditor* shapeEditor_{ nullptr };
		HalconCpp::HImage lastFrame_;
	};
}
