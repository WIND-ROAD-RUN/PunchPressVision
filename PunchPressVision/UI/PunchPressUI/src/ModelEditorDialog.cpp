// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/ModelEditorDialog.h"
#include "ui_Dlg_createshapemodel.h"

#include <QFileDialog>
#include <QLayout>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

// 取消 Win32 MessageBox 宏
#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	ModelEditorDialog::ModelEditorDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_createshapemodelClass())
		, app_(app)
	{
		ui->setupUi(this);

		// 进入创建模型模式：相机自由运行取流，图像实时刷新到 ShapeEditor
		previousMode_ = app_.currentMode();
		app_.switchToMode(global::RunMode::CreateModel);

		// 先创建 ShapeEditor，后续 buildConnections 需连接其信号
		shapeEditor_ = new ShapeEditor(this);
		auto* oldLabel = ui->label_imgDisplay;
		if (auto* parentWidget = oldLabel->parentWidget())
		{
			if (auto* layout = parentWidget->layout())
			{
				layout->replaceWidget(oldLabel, shapeEditor_);
			}
		}
		oldLabel->deleteLater();
		shapeEditor_->setGeometry(0, 0, oldLabel->width(), oldLabel->height());

		buildConnections();
	}

	ModelEditorDialog::~ModelEditorDialog()
	{
		// 退出创建模型模式，回到 Idle 停止取流
		app_.switchToMode(global::RunMode::Idle);
		delete ui;
	}

	void ModelEditorDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &ModelEditorDialog::onFrameReady, Qt::QueuedConnection);

		// ShapeEditor 工具切换 → 更新按钮文字
		connect(shapeEditor_, &ShapeEditor::toolChanged,
			this, &ModelEditorDialog::onToolChanged);

		connect(ui->btn_paintRegion, &QPushButton::clicked,
			this, &ModelEditorDialog::onPaintRegion);
		connect(ui->btn_shiledRegion, &QPushButton::clicked,
			this, &ModelEditorDialog::onPaintMask);
		connect(ui->btn_paintCenterPoint, &QPushButton::clicked,
			this, &ModelEditorDialog::onPaintCenterPoint);
		connect(ui->btn_clearRegion, &QPushButton::clicked,
			this, &ModelEditorDialog::onClearRegion);
		connect(ui->btn_clearRegion2, &QPushButton::clicked,
			this, &ModelEditorDialog::onUndo);
		connect(ui->btn_createShapeModel, &QPushButton::clicked,
			this, &ModelEditorDialog::onCreateModel);
		connect(ui->btn_readImage, &QPushButton::clicked,
			this, &ModelEditorDialog::onReadImage);
		connect(ui->btn_exit, &QPushButton::clicked,
			this, &QDialog::accept);
	}

	void ModelEditorDialog::onFrameReady(HalconCpp::HImage image)
	{
		lastFrame_ = image;
		if (shapeEditor_)
			shapeEditor_->displayImage(image);
	}

	void ModelEditorDialog::onPaintRegion()
	{
		if (!shapeEditor_)
			return;
		// Toggle: 已在 RectangleROI 模式则切回 View，否则进入 RectangleROI
		if (shapeEditor_->tool() == ShapeEditor::Tool::RectangleROI)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		else
			shapeEditor_->setTool(ShapeEditor::Tool::RectangleROI);
	}

	void ModelEditorDialog::onPaintMask()
	{
		if (!shapeEditor_)
			return;
		// Toggle: 已在 RectangleMask 模式则切回 View，否则进入 RectangleMask
		if (shapeEditor_->tool() == ShapeEditor::Tool::RectangleMask)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		else
			shapeEditor_->setTool(ShapeEditor::Tool::RectangleMask);
	}

	void ModelEditorDialog::onPaintCenterPoint()
	{
		if (!shapeEditor_)
			return;
		// Toggle: 已在 CenterPoint 模式则切回 View，否则进入 CenterPoint
		if (shapeEditor_->tool() == ShapeEditor::Tool::CenterPoint)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		else
			shapeEditor_->setTool(ShapeEditor::Tool::CenterPoint);
	}

	void ModelEditorDialog::onClearRegion()
	{
		if (shapeEditor_)
			shapeEditor_->clearAll();
	}

	void ModelEditorDialog::onUndo()
	{
		// 统一回撤：移除最近一次操作（ROI 或 Mask）
		if (shapeEditor_)
			shapeEditor_->undo();
	}

	void ModelEditorDialog::onCreateModel()
	{
		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun || !lastFrame_.IsInitialized())
			return;

		bun::CreateModelRequest req;
		req.trainingImage = lastFrame_;
		if (shapeEditor_)
		{
			req.roi = shapeEditor_->roi();
			if (shapeEditor_->hasMask())
				req.mask = shapeEditor_->mask();
			if (shapeEditor_->hasCenterPoint())
			{
				req.centerPoint = shapeEditor_->centerPoint();
				req.hasCenterPoint = true;
			}
		}

		// 记录当前光源/曝光增益到模型元数据（FR-025）
		if (biz.light_control_bun)
		{
			req.upperLight = biz.light_control_bun->getUpperLightState();
			req.lowerLight = biz.light_control_bun->getLowerLightState();
		}

		Config::ShapeModelInfo outInfo;
		std::string err;
		if (!biz.shape_mode_manager_bun->createModel(req, outInfo, &err))
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("创建模型"), QString::fromStdString(err));
			return;
		}
		rw::rqwu::MessageBox::information(this, QStringLiteral("创建模型"), QStringLiteral("模型已保存"));
	}

	void ModelEditorDialog::onReadImage()
	{
		const QString path = QFileDialog::getOpenFileName(this,
			QStringLiteral("选择训练图片"),
			QString(),
			QStringLiteral("图片 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
		if (path.isEmpty())
			return;

		try
		{
			HalconCpp::HImage image(path.toStdString().c_str());
			lastFrame_ = image;
			if (shapeEditor_)
				shapeEditor_->displayImage(image);
		}
		catch (...)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("读取失败"), QStringLiteral("无法打开图片"));
		}
	}

	void ModelEditorDialog::onToolChanged(ShapeEditor::Tool tool)
	{
		updateToolButtons(tool);
	}

	void ModelEditorDialog::updateToolButtons(ShapeEditor::Tool tool)
	{
		// 根据当前工具更新按钮文字：选中时显示"退出…"，未选中时显示原标题
		ui->btn_paintRegion->setText(
			tool == ShapeEditor::Tool::RectangleROI
				? QStringLiteral("退出绘制")
				: QStringLiteral("绘制感兴趣区域"));

		ui->btn_shiledRegion->setText(
			tool == ShapeEditor::Tool::RectangleMask
				? QStringLiteral("退出屏蔽")
				: QStringLiteral("绘制屏蔽区域"));

		ui->btn_paintCenterPoint->setText(
			tool == ShapeEditor::Tool::CenterPoint
				? QStringLiteral("退出定义")
				: QStringLiteral("定义中心点"));
	}
} // namespace ui
