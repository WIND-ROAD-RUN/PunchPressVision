// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

#include "UI/ModelEditorDialog.h"
#include "ui_Dlg_createshapemodel.h"

#include <QFileDialog>
#include <QLayout>
#include <QShowEvent>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	// ===== 构造 / 析构 ========================================================

	ModelEditorDialog::ModelEditorDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_createshapemodelClass())
		, app_(app)
	{
		ui->setupUi(this);

		previousMode_ = app_.currentMode();
		app_.switchToMode(global::RunMode::CreateModel);

		// ShapeEditor 替换占位 QLabel
		shapeEditor_ = new ShapeEditor(this);
		auto* oldLabel = ui->label_imgDisplay;
		if (auto* parentWidget = oldLabel->parentWidget())
		{
			if (auto* layout = parentWidget->layout())
				layout->replaceWidget(oldLabel, shapeEditor_);
		}
		oldLabel->deleteLater();
		shapeEditor_->setGeometry(0, 0, oldLabel->width(), oldLabel->height());

		loadCameraParams();
		updateCameraParamButtons();
		updateContrastVisibility();

		buildConnections();
	}

	ModelEditorDialog::~ModelEditorDialog()
	{
		app_.switchToMode(global::RunMode::Idle);
		delete ui;
	}

	void ModelEditorDialog::showEvent(QShowEvent* e)
	{
		QDialog::showEvent(e);
		if (parentWidget())
			resize(parentWidget()->size());
	}

	// ===== 信号连接 ============================================================

	void ModelEditorDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &ModelEditorDialog::onFrameReady, Qt::QueuedConnection);

		connect(shapeEditor_, &ShapeEditor::toolChanged,
			this, &ModelEditorDialog::onToolChanged);

		// 形状编辑
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

		// 相机参数
		connect(ui->btn_zengyi1, &QPushButton::clicked,
			this, &ModelEditorDialog::onGain1Clicked);
		connect(ui->btn_baoguang1, &QPushButton::clicked,
			this, &ModelEditorDialog::onExposure1Clicked);
		connect(ui->btn_zengyi2, &QPushButton::clicked,
			this, &ModelEditorDialog::onGain2Clicked);
		connect(ui->btn_baoguang2, &QPushButton::clicked,
			this, &ModelEditorDialog::onExposure2Clicked);

		// 模型参数
		connect(ui->btn_opening, &QPushButton::clicked,
			this, &ModelEditorDialog::onOpeningClicked);
		connect(ui->btn_closing, &QPushButton::clicked,
			this, &ModelEditorDialog::onClosingClicked);
		connect(ui->btn_mean, &QPushButton::clicked,
			this, &ModelEditorDialog::onMeanClicked);

		connect(ui->rbtn_auto, &QRadioButton::toggled,
			this, &ModelEditorDialog::onContrastAutoToggled);
		connect(ui->btn_contrast, &QPushButton::clicked, this, [this]()
		{
			if (inputIntParam(ui->btn_contrast, contrast_, 1, 255,
				QStringLiteral("最大对比度")))
				ui->btn_contrast->setText(QString::number(contrast_));
		});
		connect(ui->btn_mincontrast, &QPushButton::clicked, this, [this]()
		{
			if (inputIntParam(ui->btn_mincontrast, minContrast_, 0, 255,
				QStringLiteral("最小对比度")))
				ui->btn_mincontrast->setText(QString::number(minContrast_));
		});

		// 操作
		connect(ui->btn_recognize, &QPushButton::clicked,
			this, &ModelEditorDialog::onRecognize);
		connect(ui->btn_createShapeModel, &QPushButton::clicked,
			this, &ModelEditorDialog::onCreateModel);
		connect(ui->btn_readImage, &QPushButton::clicked,
			this, &ModelEditorDialog::onReadImage);
		connect(ui->btn_exit, &QPushButton::clicked,
			this, &QDialog::accept);
	}

	// ===== 帧回调 ==============================================================

	void ModelEditorDialog::onFrameReady(HalconCpp::HImage image)
	{
		lastFrame_ = image;
		if (shapeEditor_)
			shapeEditor_->displayImage(image);
	}

	// ===== 形状编辑 ============================================================

	void ModelEditorDialog::onPaintRegion()
	{
		if (!shapeEditor_) return;
		if (shapeEditor_->tool() == ShapeEditor::Tool::RectangleROI)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		else
			shapeEditor_->setTool(ShapeEditor::Tool::RectangleROI);
	}

	void ModelEditorDialog::onPaintMask()
	{
		if (!shapeEditor_) return;
		if (shapeEditor_->tool() == ShapeEditor::Tool::RectangleMask)
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		else
			shapeEditor_->setTool(ShapeEditor::Tool::RectangleMask);
	}

	void ModelEditorDialog::onPaintCenterPoint()
	{
		if (!shapeEditor_) return;
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
		if (shapeEditor_)
			shapeEditor_->undo();
	}

	// ===== ROI 校验 ============================================================

	bool ModelEditorDialog::requireROI(const QString& action) const
	{
		if (shapeEditor_ && shapeEditor_->hasROI())
			return true;
		rw::rqwu::MessageBox::warning(const_cast<ModelEditorDialog*>(this),
			QStringLiteral("提示"),
			QStringLiteral("%1前请先绘制感兴趣区域（ROI）").arg(action));
		return false;
	}

	// ===== 构建请求（识别和创建共用）===========================================

	bun::CreateModelRequest ModelEditorDialog::buildRequest(
		const HalconCpp::HImage& image) const
	{
		bun::CreateModelRequest req;
		req.trainingImage = image;

		if (shapeEditor_)
		{
			req.roiRects = shapeEditor_->roiRects();
			req.maskRects = shapeEditor_->maskRects();
			if (shapeEditor_->hasROI())
				req.roi = shapeEditor_->roi();
			if (shapeEditor_->hasMask())
				req.mask = shapeEditor_->mask();
			if (shapeEditor_->hasCenterPoint())
			{
				req.centerPoint = shapeEditor_->centerPoint();
				req.hasCenterPoint = true;
			}
		}

		auto& biz = app_.business();
		if (biz.light_control_bun)
		{
			req.upperLight = biz.light_control_bun->getUpperLightState();
			req.lowerLight = biz.light_control_bun->getLowerLightState();
		}
		req.exposure = static_cast<double>(cameraCfg_.exposureTime1);
		req.gain = static_cast<double>(cameraCfg_.gain1);

		req.imageChannelType = ui->comboBox_ImageType->currentIndex();
		req.useOpening = ui->ckb_opening->isChecked();
		req.openingSize = openingSize_;
		req.useClosing = ui->ckb_closing->isChecked();
		req.closingSize = closingSize_;
		req.useMean = ui->ckb_mean->isChecked();
		req.meanSize = meanSize_;

		req.contrast = contrastAuto_ ? 0 : contrast_;
		req.minContrast = minContrast_;

		return req;
	}

	// ===== 识别 ================================================================

	void ModelEditorDialog::onRecognize()
	{
		if (!requireROI(QStringLiteral("识别")))
			return;

		if (!lastFrame_.IsInitialized())
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("提示"),
				QStringLiteral("请先载入图像"));
			return;
		}

		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun)
			return;

		const auto req = buildRequest(lastFrame_);
		std::string err;
		const auto result = biz.shape_mode_manager_bun->testRecognize(req, &err);

		if (shapeEditor_)
		{
			if (result.found)
			{
				shapeEditor_->drawRecognitionMarker(
					result.row, result.column, result.angle, result.score);
			}
			else
			{
				shapeEditor_->clearMarker();
				rw::rqwu::MessageBox::information(this,
					QStringLiteral("识别结果"),
					err.empty()
						? QStringLiteral("未匹配到模板\n请调整 ROI 或模型参数后重试")
						: QString::fromStdString(err));
			}
		}
	}

	// ===== 创建模型 ============================================================

	void ModelEditorDialog::onCreateModel()
	{
		if (!requireROI(QStringLiteral("创建模型")))
			return;

		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun || !lastFrame_.IsInitialized())
			return;

		const auto req = buildRequest(lastFrame_);

		Config::ShapeModelInfo outInfo;
		std::string err;
		if (!biz.shape_mode_manager_bun->createModel(req, outInfo, &err))
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("创建模型"),
				QString::fromStdString(err));
			return;
		}
		rw::rqwu::MessageBox::information(this, QStringLiteral("创建模型"),
			QStringLiteral("模型已保存"));
	}

	// ===== 相机参数 ============================================================

	void ModelEditorDialog::loadCameraParams()
	{
		const auto& inf = app_.business().infrastructure();
		if (!inf.camera_module_)
			return;
		cameraCfg_ = inf.camera_module_->cameraCfg;
	}

	void ModelEditorDialog::updateCameraParamButtons()
	{
		ui->btn_zengyi1->setText(QString::number(cameraCfg_.gain1));
		ui->btn_baoguang1->setText(QString::number(cameraCfg_.exposureTime1));
		ui->btn_zengyi2->setText(QString::number(cameraCfg_.gain2));
		ui->btn_baoguang2->setText(QString::number(cameraCfg_.exposureTime2));
	}

	void ModelEditorDialog::onGain1Clicked()
	{
		if (!inputIntParam(ui->btn_zengyi1, cameraCfg_.gain1, 0, 100, QStringLiteral("增益1")))
			return;
		applyGain(global::CameraIndex::Camera1, cameraCfg_.gain1, &Config::cameraCfg::gain1);
	}

	void ModelEditorDialog::onExposure1Clicked()
	{
		if (!inputIntParam(ui->btn_baoguang1, cameraCfg_.exposureTime1, 1, 10000000, QStringLiteral("曝光1")))
			return;
		applyExposure(global::CameraIndex::Camera1, cameraCfg_.exposureTime1, &Config::cameraCfg::exposureTime1);
	}

	void ModelEditorDialog::onGain2Clicked()
	{
		if (!inputIntParam(ui->btn_zengyi2, cameraCfg_.gain2, 0, 100, QStringLiteral("增益2")))
			return;
		applyGain(global::CameraIndex::Camera2, cameraCfg_.gain2, &Config::cameraCfg::gain2);
	}

	void ModelEditorDialog::onExposure2Clicked()
	{
		if (!inputIntParam(ui->btn_baoguang2, cameraCfg_.exposureTime2, 1, 10000000, QStringLiteral("曝光2")))
			return;
		applyExposure(global::CameraIndex::Camera2, cameraCfg_.exposureTime2, &Config::cameraCfg::exposureTime2);
	}

	void ModelEditorDialog::applyExposure(global::CameraIndex idx, int value,
		int Config::cameraCfg::* member)
	{
		cameraCfg_.*member = value;
		auto& inf = app_.business().infrastructure();
		if (inf.camera_module_)
		{
			inf.camera_module_->cameraCfg.*member = value;
			inf.camera_module_->setExposure(idx, static_cast<double>(value));
		}
		if (inf.config_module_)
		{
			inf.config_module_->cameraCfg.*member = value;
			inf.config_module_->save();
		}
		updateCameraParamButtons();
	}

	void ModelEditorDialog::applyGain(global::CameraIndex idx, int value,
		int Config::cameraCfg::* member)
	{
		cameraCfg_.*member = value;
		auto& inf = app_.business().infrastructure();
		if (inf.camera_module_)
		{
			inf.camera_module_->cameraCfg.*member = value;
			inf.camera_module_->setGain(idx, static_cast<double>(value));
		}
		if (inf.config_module_)
		{
			inf.config_module_->cameraCfg.*member = value;
			inf.config_module_->save();
		}
		updateCameraParamButtons();
	}

	// ===== 模型参数 ============================================================

	void ModelEditorDialog::onOpeningClicked()
	{
		if (inputIntParam(ui->btn_opening, openingSize_, 1, 99, QStringLiteral("开运算核大小")))
			ui->btn_opening->setText(QString::number(openingSize_));
	}

	void ModelEditorDialog::onClosingClicked()
	{
		if (inputIntParam(ui->btn_closing, closingSize_, 1, 99, QStringLiteral("闭运算核大小")))
			ui->btn_closing->setText(QString::number(closingSize_));
	}

	void ModelEditorDialog::onMeanClicked()
	{
		if (inputIntParam(ui->btn_mean, meanSize_, 1, 99, QStringLiteral("均值滤波核大小")))
			ui->btn_mean->setText(QString::number(meanSize_));
	}

	void ModelEditorDialog::onContrastAutoToggled(bool checked)
	{
		contrastAuto_ = checked;
		updateContrastVisibility();
	}

	void ModelEditorDialog::updateContrastVisibility()
	{
		ui->widget_contrastHide->setVisible(!contrastAuto_);
	}

	// ===== 数字键盘 ============================================================

	bool ModelEditorDialog::inputIntParam(QPushButton* button, int& value,
		int min, int max, const QString& title)
	{
		Q_UNUSED(title);
		QString input = QString::number(value);

		rw::rqwu::NumberKeyboard::InputDataConfig cfg;
		cfg.isUsingMin = true;
		cfg.isUsingMax = true;
		cfg.min = static_cast<double>(min);
		cfg.max = static_cast<double>(max);

		const auto result = rw::rqwu::NumberKeyboard::inputDataOnQPushButton(button, input, cfg);
		if (result != rw::rqwu::NumberKeyboard::Accept)
			return false;

		bool ok = false;
		const int newValue = input.toInt(&ok);
		if (!ok) return false;

		value = newValue;
		button->setText(QString::number(newValue));
		return true;
	}

	bool ModelEditorDialog::inputDoubleParam(QPushButton* button, double& value,
		double min, double max, int decimals, const QString& title)
	{
		Q_UNUSED(title);
		QString input = QString::number(value, 'f', decimals);

		rw::rqwu::NumberKeyboard::InputDataConfig cfg;
		cfg.isUsingMin = true;
		cfg.isUsingMax = true;
		cfg.min = min;
		cfg.max = max;

		const auto result = rw::rqwu::NumberKeyboard::inputDataOnQPushButton(button, input, cfg);
		if (result != rw::rqwu::NumberKeyboard::Accept)
			return false;

		bool ok = false;
		const double newValue = input.toDouble(&ok);
		if (!ok) return false;

		value = newValue;
		button->setText(QString::number(value, 'f', decimals));
		return true;
	}

	// ===== 按钮文字 ============================================================

	void ModelEditorDialog::onToolChanged(ShapeEditor::Tool tool)
	{
		updateToolButtons(tool);
	}

	void ModelEditorDialog::updateToolButtons(ShapeEditor::Tool tool)
	{
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

	// ===== 读取图片 ============================================================

	void ModelEditorDialog::onReadImage()
	{
		const QString path = QFileDialog::getOpenFileName(this,
			QStringLiteral("选择训练图片"),
			QString(),
			QStringLiteral("图片 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
		if (path.isEmpty()) return;

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
