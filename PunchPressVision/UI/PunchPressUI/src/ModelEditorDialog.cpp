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

		// 预处理参数变化时刷新显示
		connect(ui->comboBox_ImageType, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &ModelEditorDialog::refreshProcessedImage);
		connect(ui->ckb_opening, &QCheckBox::stateChanged,
			this, &ModelEditorDialog::refreshProcessedImage);
		connect(ui->ckb_closing, &QCheckBox::stateChanged,
			this, &ModelEditorDialog::refreshProcessedImage);
		connect(ui->ckb_mean, &QCheckBox::stateChanged,
			this, &ModelEditorDialog::refreshProcessedImage);

		// 操作
		connect(ui->btn_recognize, &QPushButton::clicked,
			this, &ModelEditorDialog::onRecognize);
		connect(ui->btn_createShapeModel, &QPushButton::clicked,
			this, &ModelEditorDialog::onCreateModel);
		connect(ui->btn_readImage, &QPushButton::clicked,
			this, &ModelEditorDialog::onReadImage);
		connect(ui->btn_exit, &QPushButton::clicked,
			this, &QDialog::accept);

		// 创建模型成功后显示轮廓
		auto& biz = app_.business();
		if (biz.shape_mode_manager_bun)
		{
			connect(biz.shape_mode_manager_bun.get(), &bun::ShapeModeManagerBun::modelContoursFound,
				shapeEditor_, &ShapeEditor::drawModelContours);
		}
	}

	// ===== 帧回调 ==============================================================

	void ModelEditorDialog::onFrameReady(HalconCpp::HImage image)
	{
		lastFrame_ = image;
		if (shapeEditor_)
			shapeEditor_->displayImage(preprocessImage(image));
	}

	// ===== 形状编辑 ============================================================

	void ModelEditorDialog::onPaintRegion()
	{
		if (!shapeEditor_) return;
		const auto tool = shapeEditor_->tool();
		if (tool == ShapeEditor::Tool::RectangleROI ||
		    tool == ShapeEditor::Tool::FreehandROI)
		{
			// 退出绘制工具，保持停止采集（继续使用同一张图片）
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		}
		else
		{
			// 进入绘制前：停止实时采集，当前帧保持显示
			app_.switchToMode(global::RunMode::Idle);
			if (ui->rbtn_manual_2->isChecked())
				shapeEditor_->setTool(ShapeEditor::Tool::FreehandROI);
			else
				shapeEditor_->setTool(ShapeEditor::Tool::RectangleROI);
		}
	}

	void ModelEditorDialog::onPaintMask()
	{
		if (!shapeEditor_) return;
		const auto tool = shapeEditor_->tool();
		if (tool == ShapeEditor::Tool::RectangleMask ||
		    tool == ShapeEditor::Tool::FreehandMask)
		{
			// 退出屏蔽工具，保持停止采集（继续使用同一张图片）
			shapeEditor_->setTool(ShapeEditor::Tool::View);
		}
		else
		{
			// 进入屏蔽前：停止实时采集，当前帧保持显示
			app_.switchToMode(global::RunMode::Idle);
			if (ui->rbtn_manual_2->isChecked())
				shapeEditor_->setTool(ShapeEditor::Tool::FreehandMask);
			else
				shapeEditor_->setTool(ShapeEditor::Tool::RectangleMask);
		}
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
		{
			shapeEditor_->clearAll();
			// 清空绘制区域后恢复实时采集
			app_.switchToMode(global::RunMode::CreateModel);
		}
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
			req._paintCreateRoiList = shapeEditor_->roiObjects();
			req._paintShieldRoiList = shapeEditor_->maskObjects();
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

		const auto req = buildRequest(preprocessImage(lastFrame_));
		std::string err;
		//TODO:这里面内部进行识别
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

		const auto req = buildRequest(preprocessImage(lastFrame_));

		Config::ShapeModelInfo outInfo;
		std::string err;
		//TODO:这里面内部进行创建
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
		{
			ui->btn_opening->setText(QString::number(openingSize_));
			refreshProcessedImage();
		}
	}

	void ModelEditorDialog::onClosingClicked()
	{
		if (inputIntParam(ui->btn_closing, closingSize_, 1, 99, QStringLiteral("闭运算核大小")))
		{
			ui->btn_closing->setText(QString::number(closingSize_));
			refreshProcessedImage();
		}
	}

	void ModelEditorDialog::onMeanClicked()
	{
		if (inputIntParam(ui->btn_mean, meanSize_, 1, 99, QStringLiteral("均值滤波核大小")))
		{
			ui->btn_mean->setText(QString::number(meanSize_));
			refreshProcessedImage();
		}
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

	void ModelEditorDialog::refreshProcessedImage()
	{
		if (shapeEditor_ && lastFrame_.IsInitialized())
			shapeEditor_->displayImage(preprocessImage(lastFrame_));
	}

	HalconCpp::HImage ModelEditorDialog::preprocessImage(const HalconCpp::HImage& image) const
	{
		using namespace HalconCpp;
		if (!image.IsInitialized())
			return image;

		try
		{
			HImage result = image;

			// 通道处理
			HTuple channels;
			CountChannels(image, &channels);
			const int channelType = ui->comboBox_ImageType->currentIndex();
			if (channels[0].I() >= 3)
			{
				switch (channelType)
				{
				case 0: // 灰度
					Rgb1ToGray(image, &result);
					break;
				case 1: // 红
					AccessChannel(image, &result, 1);
					break;
				case 2: // 绿
					AccessChannel(image, &result, 2);
					break;
				case 3: // 蓝
					AccessChannel(image, &result, 3);
					break;
				default:
					break;
				}
			}
			else if (channelType != 0 && channels[0].I() >= channelType)
			{
				AccessChannel(image, &result, channelType);
			}

			// 开运算
			if (ui->ckb_opening->isChecked() && openingSize_ > 0)
			{
				HImage opened;
				OpeningCircle(result, &opened, openingSize_);
				result = opened;
			}

			// 闭运算
			if (ui->ckb_closing->isChecked() && closingSize_ > 0)
			{
				HImage closed;
				ClosingCircle(result, &closed, closingSize_);
				result = closed;
			}

			// 均值滤波
			if (ui->ckb_mean->isChecked() && meanSize_ > 0)
			{
				HImage smoothed;
				const int size = meanSize_ * 2 + 1;
				MeanImage(result, &smoothed, size, size);
				result = smoothed;
			}

			return result;
		}
		catch (...)
		{
			return image;
		}
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
		const bool isRoiTool = (tool == ShapeEditor::Tool::RectangleROI ||
		                        tool == ShapeEditor::Tool::FreehandROI);
		ui->btn_paintRegion->setText(
			isRoiTool
				? QStringLiteral("退出绘制")
				: QStringLiteral("绘制感兴趣区域"));

		const bool isMaskTool = (tool == ShapeEditor::Tool::RectangleMask ||
		                         tool == ShapeEditor::Tool::FreehandMask);
		ui->btn_shiledRegion->setText(
			isMaskTool
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
				shapeEditor_->displayImage(preprocessImage(image));
		}
		catch (...)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("读取失败"), QStringLiteral("无法打开图片"));
		}
	}
} // namespace ui
