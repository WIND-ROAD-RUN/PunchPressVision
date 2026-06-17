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
		buildConnections();

		// 用 ShapeEditor 替换 ui 中的占位 QLabel
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
	}

	ModelEditorDialog::~ModelEditorDialog()
	{
		delete ui;
	}

	void ModelEditorDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &ModelEditorDialog::onFrameReady, Qt::QueuedConnection);

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
		if (shapeEditor_)
			shapeEditor_->setTool(ShapeEditor::Tool::RectangleROI);
	}

	void ModelEditorDialog::onPaintMask()
	{
		// Mask 一期暂未实现；先清空 ROI 作为提示
		if (shapeEditor_)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		rw::rqwu::MessageBox::information(this,
			QStringLiteral("提示"), QStringLiteral("屏蔽区域功能将在二期实现"));
	}

	void ModelEditorDialog::onPaintCenterPoint()
	{
		if (shapeEditor_)
			shapeEditor_->setTool(ShapeEditor::Tool::CenterPoint);
	}

	void ModelEditorDialog::onClearRegion()
	{
		if (shapeEditor_)
			shapeEditor_->clearAll();
	}

	void ModelEditorDialog::onUndo()
	{
		if (shapeEditor_)
		{
			// 一期仅支持单 ROI / 单中心点，undo 等价于 clearROI
			shapeEditor_->clearROI();
		}
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
} // namespace ui
