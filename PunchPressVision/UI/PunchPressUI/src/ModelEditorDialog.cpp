// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

#include "UI/ModelEditorDialog.h"
#include "ui_Dlg_createshapemodel.h"

#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QShowEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModule.hpp"
#include "Business/StampPatternBun/StampPatternBun.hpp"
#include "UI/StampPatternPickerDialog.h"

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	// ===== 构造 / 析构 ========================================================

	ModelEditorDialog::ModelEditorDialog(app::PunchPressApp& app, bool isModifyMode,
		const std::string& modelId, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_createshapemodelClass())
		, app_(app)
		, isModifyMode_(isModifyMode)
		, modelId_(modelId)
	{
		ui->setupUi(this);

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setWindowState(windowState() | Qt::WindowFullScreen);
#endif

		previousMode_ = app_.currentMode();
		if (isModifyMode_)
			app_.switchToMode(global::RunMode::Idle);        // 修改模式：相机不出图
		else
			app_.switchToMode(global::RunMode::CreateModel); // 创建模式：实时取流

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
		updateStampPatternStatus();

		// 修改模式：禁用读取新图片，确保一直编辑创建时的原始图
		ui->btn_readImage->setEnabled(!isModifyMode_);

		// 修改模式下调整按钮文本
		if (isModifyMode_)
			ui->btn_createShapeModel->setText(QStringLiteral("修改模板"));

		buildConnections();
	}

	ModelEditorDialog::~ModelEditorDialog()
	{
		app_.switchToMode(previousMode_);
		delete ui;
	}

	void ModelEditorDialog::showEvent(QShowEvent* e)
	{
		QDialog::showEvent(e);
		if (parentWidget())
			resize(parentWidget()->size());

		// 修改模式：首次显示时加载已有模型（此时 Halcon 窗口已就绪）
		if (isModifyMode_ && !modelLoaded_)
		{
			modelLoaded_ = true;
			loadExistingModel(modelId_);
		}
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

		// 套版
		connect(ui->btn_selectStampPattern, &QPushButton::clicked,
			this, &ModelEditorDialog::onSelectStampPattern);

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

		// 偏差补偿（匹配结果叠加：X/Y 单位 mm，角度单位 deg）
		connect(ui->btn_offsetX, &QPushButton::clicked, this, [this]()
		{
			if (inputDoubleParam(ui->btn_offsetX, offsetX_, -1000.0, 1000.0, 2,
				QStringLiteral("左右偏差(mm)")))
				applyOffsetsToStamp();
		});
		connect(ui->btn_offsetY, &QPushButton::clicked, this, [this]()
		{
			if (inputDoubleParam(ui->btn_offsetY, offsetY_, -1000.0, 1000.0, 2,
				QStringLiteral("上下偏差(mm)")))
				applyOffsetsToStamp();
		});
		connect(ui->btn_offsetAngle, &QPushButton::clicked, this, [this]()
		{
			if (inputDoubleParam(ui->btn_offsetAngle, offsetAngle_, -180.0, 180.0, 2,
				QStringLiteral("角度偏差(°)")))
				applyOffsetsToStamp();
		});

		// 拖动/旋转套版 → 由套版相对中心点的位移推导模型偏移量（与主程序同一偏移量）
		connect(shapeEditor_, &ShapeEditor::stampPatternChanged,
			this, &ModelEditorDialog::syncOffsetsFromStamp);

		// 偏差微调按钮（步长 0.1mm / 0.1°），调整后套版中心实时跟随移动
		auto nudgeOffset = [this](QPushButton* btn, double& value,
			double delta, double minV, double maxV)
		{
			// 按 0.1 步长取整，避免浮点累加漂移
			value = qBound(minV, std::round((value + delta) * 10.0) / 10.0, maxV);
			btn->setText(QString::number(value, 'f', 2));
			applyOffsetsToStamp();
		};
		connect(ui->btn_offsetXSub, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetX, offsetX_, -0.1, -1000.0, 1000.0); });
		connect(ui->btn_offsetXAdd, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetX, offsetX_, 0.1, -1000.0, 1000.0); });
		connect(ui->btn_offsetYSub, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetY, offsetY_, -0.1, -1000.0, 1000.0); });
		connect(ui->btn_offsetYAdd, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetY, offsetY_, 0.1, -1000.0, 1000.0); });
		connect(ui->btn_offsetAngleSub, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetAngle, offsetAngle_, -0.1, -180.0, 180.0); });
		connect(ui->btn_offsetAngleAdd, &QPushButton::clicked, this, [this, nudgeOffset]()
			{ nudgeOffset(ui->btn_offsetAngle, offsetAngle_, 0.1, -180.0, 180.0); });

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
		connect(ui->btn_createShapeModel, &QPushButton::clicked,
			this, &ModelEditorDialog::onCreateModel);
		connect(ui->btn_readImage, &QPushButton::clicked,
			this, &ModelEditorDialog::onReadImage);
		connect(ui->btn_exit, &QPushButton::clicked,
			this, &ModelEditorDialog::onExit);

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
		// 修改模式始终使用创建时的原始图，不接收实时相机帧
		if (isModifyMode_)
			return;

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
			// 进入绘制前：检查图像是否就绪
			if (!requireImage(QStringLiteral("绘制")))
				return;
			// 停止实时采集，当前帧保持显示
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
			// 进入屏蔽前：检查图像是否就绪
			if (!requireImage(QStringLiteral("绘制屏蔽")))
				return;
			// 停止实时采集，当前帧保持显示
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
			// 清空绘制区域后恢复实时采集（仅创建模式）
			if (!isModifyMode_)
				app_.switchToMode(global::RunMode::CreateModel);
		}
	}

	void ModelEditorDialog::onUndo()
	{
		if (shapeEditor_)
			shapeEditor_->undo();
	}

	// ===== 套版（StampPattern）叠加与对齐 ========================================

	void ModelEditorDialog::updateStampPatternStatus()
	{
		QString text = QStringLiteral("当前未使用套版");
		QString color = QStringLiteral("rgb(141, 141, 141)");
		if (!stampPatternId_.empty())
		{
			auto& bun = app_.business().stamp_pattern_bun;
			if (bun)
			{
				const Config::StampPatternItem item = bun->getPatternItem(stampPatternId_);
				if (!item.info.getId().empty())
				{
					text = QStringLiteral("当前套版: %1")
						.arg(QString::fromStdString(item.info.base_info.name));
					color = QStringLiteral("#2196F3");
				}
			}
		}
		ui->label_stampPatternStatus->setText(text);
		ui->label_stampPatternStatus->setStyleSheet(
			QStringLiteral("QLabel { font-size: 20px; font-weight: bold; color: %1; padding: 5px 5px; }")
				.arg(color));
	}

	void ModelEditorDialog::onSelectStampPattern()
	{
		if (!shapeEditor_) return;

		// 切换前先保存当前套版的对齐参数
		persistStampPatternAlignment();

		StampPatternPickerDialog dlg(app_, this);
		dlg.setInitialPatternId(stampPatternId_);
		if (dlg.exec() != QDialog::Accepted)
			return;

		const std::string id = dlg.selectedPatternId();
		if (id.empty())
		{
			stampPatternId_.clear();
			shapeEditor_->clearStampPattern();
			if (shapeEditor_->tool() == ShapeEditor::Tool::StampPattern)
				shapeEditor_->setTool(ShapeEditor::Tool::View);
		}
		else
		{
			auto& bun = app_.business().stamp_pattern_bun;
			if (!bun) return;

			const Config::StampPatternItem item = bun->getPatternItem(id);
			if (!item.data._patternImage.IsInitialized())
			{
				rw::rqwu::MessageBox::warning(this,
					QStringLiteral("提示"), QStringLiteral("该套版缺少有效图片，无法叠加。"));
				return;
			}

			stampPatternId_ = id;
			// DXF 套版：图素单位为实际尺寸(mm)，需乘上九点标定的像素/mm 才能与图像匹配
			double scale = item.data.alignScale;
			if (item.data.fromDxf)
				scale *= bun::StampPatternBun::pixelsPerWorldUnit(
					app_.business().infrastructure());
			shapeEditor_->setStampPattern(item.data._patternImage,
				item.data.alignRow, item.data.alignCol, item.data.alignAngle,
				scale, item.data.alpha);
			// 套版中心吸附到中心点后，由位移推导偏移量（初次为 0）
			syncOffsetsFromStamp();
		}
		updateStampPatternStatus();
	}

	void ModelEditorDialog::persistStampPatternAlignment()
	{
		if (stampPatternId_.empty() || !shapeEditor_ || !shapeEditor_->hasStampPattern())
			return;

		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun) return;

		// 先取回库中现有数据，保留 fromDxf 等标记位
		Config::StampPatternData data = bun->getPatternItem(stampPatternId_).data;
		data.alignRow = shapeEditor_->stampRow();
		data.alignCol = shapeEditor_->stampCol();
		data.alignAngle = shapeEditor_->stampAngle();
		// DXF 套版：编辑器中的缩放 = alignScale × 像素/mm，写回时需除回，
		// 保证库中 alignScale 始终是用户微调系数，与标定变化解耦
		double scale = shapeEditor_->stampScale();
		if (data.fromDxf)
		{
			const double k = bun::StampPatternBun::pixelsPerWorldUnit(
				app_.business().infrastructure());
			if (k > 1e-12)
				scale /= k;
		}
		data.alignScale = scale;
		data.alpha = shapeEditor_->stampAlpha();

		bun->updatePatternData(stampPatternId_, data);
	}

	// ===== 偏移量 <-> 套版位置 同步 ==============================================

	bool ModelEditorDialog::ninePointHomMat2D(HalconCpp::HTuple& out) const
	{
		const auto& inf = app_.business().infrastructure();
		if (!inf.nine_point_module_)
			return false;
		const HalconCpp::HTuple& h = inf.nine_point_module_->ninePointConfig.outHomMat2D;
		if (h.TupleLength() < 6)
			return false;
		out = h;
		return true;
	}

	void ModelEditorDialog::syncOffsetsFromStamp()
	{
		if (!shapeEditor_ || !shapeEditor_->hasStampPattern() || !shapeEditor_->hasCenterPoint())
			return;

		using namespace HalconCpp;

		// 角度偏差：套版旋转角即模型角度偏差（与 match() 中 result.angle 减去 offsetAngle 对应）
		offsetAngle_ = shapeEditor_->stampAngle() * 180.0 / 3.14159265358979323846;

		// 位置偏差：套版中心相对定义中心点的位移，经九点标定换算为 mm
		HalconCpp::HTuple H;
		if (ninePointHomMat2D(H))
		{
			try
			{
				const QPointF cp = shapeEditor_->centerPoint();
				const QPointF sc = shapeEditor_->stampCenterImagePoint();
				HTuple r0, c0, r1, c1;
				AffineTransPoint2d(H, cp.y(), cp.x(), &r0, &c0);
				AffineTransPoint2d(H, sc.y(), sc.x(), &r1, &c1);
				// 与 ShapeModeManagerBun::match() 同一约定：
				// world(row,col) = (-offsetY, offsetX)
				offsetX_ = c1[0].D() - c0[0].D();
				offsetY_ = -(r1[0].D() - r0[0].D());
			}
			catch (...) {}
		}

		updateOffsetButtons();
	}

	void ModelEditorDialog::applyOffsetsToStamp()
	{
		if (!shapeEditor_ || !shapeEditor_->hasStampPattern() || !shapeEditor_->hasCenterPoint())
			return;

		using namespace HalconCpp;

		// 位置偏移折入定义中心点：移动后的落点即新的定义中心点，
		// 主程序匹配结果直接使用该中心点，不再叠加位置偏移
		if (offsetX_ != 0.0 || offsetY_ != 0.0)
		{
			HalconCpp::HTuple H;
			if (ninePointHomMat2D(H))
			{
				try
				{
					// 与 match() 同一约定：像素位移 = invH·(-offsetY, offsetX) - invH·(0,0)
					HTuple invH;
					HomMat2dInvert(H, &invH);
					HTuple r0, c0, r1, c1;
					AffineTransPoint2d(invH, 0.0, 0.0, &r0, &c0);
					AffineTransPoint2d(invH, -offsetY_, offsetX_, &r1, &c1);
					const double dRow = r1[0].D() - r0[0].D();
					const double dCol = c1[0].D() - c0[0].D();
					const QPointF cp = shapeEditor_->centerPoint();
					// setCenterPoint 会让套版随中心点整体平移（保持二者重合）
					shapeEditor_->setCenterPoint(QPointF(cp.x() + dCol, cp.y() + dRow));
				}
				catch (...) {}
			}
			else
			{
				rw::rqwu::MessageBox::warning(this,
					QStringLiteral("提示"),
					QStringLiteral("未检测到九点标定，位置偏差无法换算为像素，未生效。"));
			}
			offsetX_ = 0.0;
			offsetY_ = 0.0;
		}

		// 角度偏移无法折入中心点：作为套版旋转角保留，
		// 生产匹配 result.angle 仍会减去该值（中心保持重合）
		shapeEditor_->setStampOffsetFromCenter(0.0, 0.0,
			offsetAngle_ * 3.14159265358979323846 / 180.0);

		updateOffsetButtons();
	}

	void ModelEditorDialog::updateOffsetButtons()
	{
		ui->btn_offsetX->setText(QString::number(offsetX_, 'f', 2));
		ui->btn_offsetY->setText(QString::number(offsetY_, 'f', 2));
		ui->btn_offsetAngle->setText(QString::number(offsetAngle_, 'f', 2));
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
		const HalconCpp::HImage& preprocessedImage,
		const HalconCpp::HImage& rawImage) const
	{
		bun::CreateModelRequest req;
		req.trainingImage = preprocessedImage;
		req.rawImage = rawImage;

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
		req.exposure2 = static_cast<double>(cameraCfg_.exposureTime2);
		req.gain2 = static_cast<double>(cameraCfg_.gain2);

		req.imageChannelType = ui->comboBox_ImageType->currentIndex();
		req.useOpening = ui->ckb_opening->isChecked();
		req.openingSize = openingSize_;
		req.useClosing = ui->ckb_closing->isChecked();
		req.closingSize = closingSize_;
		req.useMean = ui->ckb_mean->isChecked();
		req.meanSize = meanSize_;

		req.contrast = contrastAuto_ ? 0 : contrast_;
		req.contrastAuto = contrastAuto_;
		req.minContrast = minContrast_;

		// 关联当前叠加的套版（空 = 不使用）
		req.stampPatternId = stampPatternId_;

		// 偏差补偿（生产匹配找到中心点后叠加）
		req.offsetX = offsetX_;
		req.offsetY = offsetY_;
		req.offsetAngle = offsetAngle_;

		return req;
	}

	// ===== 创建模型 ============================================================

	void ModelEditorDialog::onCreateModel()
	{
		// 仍在绘制中，提示用户先右键完成绘制
		if (isInDrawingTool())
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("提示"),
				QStringLiteral("请先右键完成当前绘制，再创建模板"));
			return;
		}
		if (!requireROI(QStringLiteral("创建模型")))
			return;

		// 关联了套版时，无论当前是否处于套版对齐工具，都把编辑器中的
		// 对齐参数写回套版库（生产帧叠加直接读取库中的对齐参数）
		persistStampPatternAlignment();

		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun || !lastFrame_.IsInitialized())
			return;

		// 防止重复触发
		if (isTraining_)
			return;

		// ===== Phase 1: 构建请求 + 捕获 matchRegion（主线程）=====
		pendingRequest_ = buildRequest(preprocessImage(lastFrame_), lastFrame_);

		bun::MatchRegionCfg matchRegion;
		{
			const auto& inf = biz.infrastructure();
			if (inf.config_module_ && !inf.config_module_->matchRegions.empty())
			{
				matchRegion.valid = true;
				matchRegion.regions = inf.config_module_->matchRegions;
			}
		}

		// ===== Phase 2: 显示进度对话框 =====
		if (!progressDialog_)
		{
			progressDialog_ = new QDialog(this);
			progressDialog_->setWindowTitle(QStringLiteral("请稍候"));
			progressDialog_->setFixedSize(400, 120);
			progressDialog_->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint
				| Qt::WindowTitleHint);
			auto* layout = new QVBoxLayout(progressDialog_);
			auto* label = new QLabel(QStringLiteral("正在创建模型，请稍候..."), progressDialog_);
			label->setAlignment(Qt::AlignCenter);
			QFont font = label->font();
			font.setPointSize(16);
			label->setFont(font);
			layout->addWidget(label);
		}
		isTraining_ = true;
		ui->btn_createShapeModel->setEnabled(false);
		progressDialog_->show();

		// ===== Phase 3: 异步调度训练 =====
		if (!trainingWatcher_)
		{
			trainingWatcher_ = new QFutureWatcher<bun::TrainShapeModelResult>(this);
			connect(trainingWatcher_, &QFutureWatcher<bun::TrainShapeModelResult>::finished,
				this, &ModelEditorDialog::onTrainingFinished);
		}

		// 按值捕获所有参数，确保线程安全
		const auto req = pendingRequest_;
		auto future = QtConcurrent::run(
			[req, matchRegion]() -> bun::TrainShapeModelResult {
				return bun::ShapeModeManagerBun::trainShapeModel(req, matchRegion);
			});
		trainingWatcher_->setFuture(future);
	}

	// ===== 异步训练完成回调 ======================================================

	void ModelEditorDialog::onTrainingFinished()
	{
		// 关闭进度对话框
		if (progressDialog_)
		{
			progressDialog_->close();
			progressDialog_->deleteLater();
			progressDialog_ = nullptr;
		}
		ui->btn_createShapeModel->setEnabled(true);
		isTraining_ = false;

		// 获取训练结果（捕获异常以防线程内未捕获的异常传播）
		bun::TrainShapeModelResult result;
		try
		{
			result = trainingWatcher_->future().result();
		}
		catch (const std::exception& e)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("错误"),
				QStringLiteral("训练线程异常: %1").arg(QString::fromStdString(e.what())));
			return;
		}
		catch (...)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("错误"),
				QStringLiteral("训练线程发生未知异常"));
			return;
		}

		if (!result.success)
		{
			rw::rqwu::MessageBox::warning(this,
				isModifyMode_ ? QStringLiteral("修改模型") : QStringLiteral("创建模型"),
				QString::fromStdString(result.errorMsg));
			return;
		}

		// ===== 主线程持久化 + 信号发射 =====
		auto& biz = app_.business();
		std::string err;
		if (isModifyMode_)
		{
			if (!biz.shape_mode_manager_bun->persistUpdatedModel(result, modelId_, &err))
			{
				rw::rqwu::MessageBox::warning(this,
					QStringLiteral("修改模型"),
					QString::fromStdString(err));
				return;
			}
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("修改模型"),
				QStringLiteral("模型已更新"));
			modelCreated_ = true;
		}
		else
		{
			Config::ShapeModelInfo outInfo;
			if (!biz.shape_mode_manager_bun->persistNewModel(result, pendingRequest_, outInfo, &err))
			{
				rw::rqwu::MessageBox::warning(this,
					QStringLiteral("创建模型"),
					QString::fromStdString(err));
				return;
			}
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("创建模型"),
				QStringLiteral("模型已保存"));
			modelId_ = outInfo.getId();
			modelCreated_ = true;
			isModifyMode_ = true;  // 后续保存走更新逻辑，避免重复创建
		}
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
				case 4: // H（色调）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = h;
					break;
				}
				case 5: // S（饱和度）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = s;
					break;
				}
				case 6: // V（明度）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = v;
					break;
				}
				default:
					break;
				}
			}
			else if (channelType != 0 && channels[0].I() >= channelType)
			{
				AccessChannel(image, &result, channelType);
			}

			// 开运算（灰度形态学，矩形掩膜）
			if (ui->ckb_opening->isChecked() && openingSize_ > 0)
			{
				HImage opened;
				const int openSize = openingSize_ * 2 + 1;
				GrayOpeningRect(result, &opened, openSize, openSize);
				result = opened;
			}

			// 闭运算（灰度形态学，矩形掩膜）
			if (ui->ckb_closing->isChecked() && closingSize_ > 0)
			{
				HImage closed;
				const int closeSize = closingSize_ * 2 + 1;
				GrayClosingRect(result, &closed, closeSize, closeSize);
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
		catch (const HException& e)
		{
			rw::rqwu::MessageBox::warning(nullptr,
				QStringLiteral("预处理异常"),
				QStringLiteral("Halcon 错误 (%1): %2")
					.arg(QString::number(e.ErrorCode()),
					     QString::fromUtf8(e.ErrorMessage().Text())));
			return image;
		}
		catch (...)
		{
			rw::rqwu::MessageBox::warning(nullptr,
				QStringLiteral("预处理异常"),
				QStringLiteral("未知错误"));
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
		// 离开套版对齐工具时，将编辑后的对齐参数写回套版库
		if (lastTool_ == ShapeEditor::Tool::StampPattern && tool != ShapeEditor::Tool::StampPattern)
			persistStampPatternAlignment();
		lastTool_ = tool;
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

	// ===== 修改模式：加载已有模型 ==============================================

	void ModelEditorDialog::loadExistingModel(const std::string& id)
	{
		if (id.empty()) return;

		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun) return;

		const auto& inf = biz.infrastructure();
		if (!inf.shape_model_manager_module_) return;

		auto item = inf.shape_model_manager_module_->getShapeModelItem(id);
		const auto& data = item.data;

		//// ---- 诊断：检查加载的数据 ----
		//{
		//	const int roiCount = static_cast<int>(data._paintCreateRoiList.size());
		//	const int maskCount = static_cast<int>(data._paintShieldRoiList.size());
		//	const bool hasImg = data._originalImage.IsInitialized();
		//	const bool hasCenter = (data.centerX != 0.0 || data.centerY != 0.0);
		//	rw::rqwu::MessageBox::information(this,
		//		QStringLiteral("加载诊断"),
		//		QStringLiteral("原始图=%1 | ROI数=%2 | Mask数=%3 | 中心点=%4")
		//			.arg(hasImg ? QStringLiteral("有") : QStringLiteral("无"))
		//			.arg(roiCount)
		//			.arg(maskCount)
		//			.arg(hasCenter ? QStringLiteral("有") : QStringLiteral("无")));
		//}

		// 保存原始图（双相机拼接原图，始终使用，不接收相机帧）
		if (data._originalImage.IsInitialized())
			lastFrame_ = data._originalImage;

		// 先恢复预处理参数，再显示（保证首次渲染就应用正确的通道/开闭运算等）
		restoreParamsFromModel(data);

		// 显示预处理后的图像
		if (shapeEditor_ && lastFrame_.IsInitialized())
			shapeEditor_->displayImage(preprocessImage(lastFrame_));

		// 恢复 ROI 区域
		if (!data._paintCreateRoiList.empty())
			shapeEditor_->setRoiObjects(data._paintCreateRoiList);

		// 恢复屏蔽区域
		if (!data._paintShieldRoiList.empty())
			shapeEditor_->setMaskObjects(data._paintShieldRoiList);

		// 恢复中心点
		if (data.centerX != 0.0 || data.centerY != 0.0)
			shapeEditor_->setCenterPoint(QPointF(data.centerX, data.centerY));

		// 恢复关联的套版并叠加显示
		if (!data.stampPatternId.empty() && biz.stamp_pattern_bun)
		{
			const Config::StampPatternItem patItem =
				biz.stamp_pattern_bun->getPatternItem(data.stampPatternId);
			if (patItem.data._patternImage.IsInitialized())
			{
				stampPatternId_ = data.stampPatternId;
				// DXF 套版：图素单位为实际尺寸(mm)，需乘上九点标定的像素/mm
				double scale = patItem.data.alignScale;
				if (patItem.data.fromDxf)
					scale *= bun::StampPatternBun::pixelsPerWorldUnit(
						app_.business().infrastructure());
				shapeEditor_->setStampPattern(patItem.data._patternImage,
					patItem.data.alignRow, patItem.data.alignCol,
					patItem.data.alignAngle, scale,
					patItem.data.alpha);
				// 模型存储的偏移量为权威值：按偏移量重建套版位置
				applyOffsetsToStamp();
			}
		}
		updateStampPatternStatus();
	}

	void ModelEditorDialog::restoreParamsFromModel(const Config::ShapeModelData& data)
	{
		// 图像通道类型
		ui->comboBox_ImageType->blockSignals(true);
		ui->comboBox_ImageType->setCurrentIndex(data._createModelPreProcessType);
		ui->comboBox_ImageType->blockSignals(false);

		// 开运算
		ui->ckb_opening->blockSignals(true);
		ui->ckb_opening->setChecked(data._createModelUseOpening);
		ui->ckb_opening->blockSignals(false);
		openingSize_ = data._createModelOpeningRadius;
		ui->btn_opening->setText(QString::number(openingSize_));

		// 闭运算
		ui->ckb_closing->blockSignals(true);
		ui->ckb_closing->setChecked(data._createModelUseClosing);
		ui->ckb_closing->blockSignals(false);
		closingSize_ = data._createModelClosingRadius;
		ui->btn_closing->setText(QString::number(closingSize_));

		// 均值滤波
		ui->ckb_mean->blockSignals(true);
		ui->ckb_mean->setChecked(data._createModelUseMean);
		ui->ckb_mean->blockSignals(false);
		meanSize_ = data._createModelMeanRadius;
		ui->btn_mean->setText(QString::number(meanSize_));

		// 对比度
		contrast_ = data.contrast;
		minContrast_ = data.minContrast;
		contrastAuto_ = (data.contrast == 0);
		ui->rbtn_auto->blockSignals(true);
		ui->rbtn_manual->blockSignals(true);
		if (contrastAuto_)
			ui->rbtn_auto->setChecked(true);
		else
			ui->rbtn_manual->setChecked(true);
		ui->rbtn_auto->blockSignals(false);
		ui->rbtn_manual->blockSignals(false);
		updateContrastVisibility();
		ui->btn_contrast->setText(QString::number(contrast_));
		ui->btn_mincontrast->setText(QString::number(minContrast_));

		// 偏差补偿
		offsetX_ = data.offsetX;
		offsetY_ = data.offsetY;
		offsetAngle_ = data.offsetAngle;
		ui->btn_offsetX->setText(QString::number(offsetX_, 'f', 2));
		ui->btn_offsetY->setText(QString::number(offsetY_, 'f', 2));
		ui->btn_offsetAngle->setText(QString::number(offsetAngle_, 'f', 2));

		// 恢复相机参数（Camera1 + Camera2）
		if (data._createModelExposureTime > 0.0)
		{
			cameraCfg_.exposureTime1 = static_cast<int>(data._createModelExposureTime);
			ui->btn_baoguang1->setText(QString::number(cameraCfg_.exposureTime1));
		}
		if (data._createModelGain > 0.0)
		{
			cameraCfg_.gain1 = static_cast<int>(data._createModelGain);
			ui->btn_zengyi1->setText(QString::number(cameraCfg_.gain1));
		}
		if (data._createModelExposureTime2 > 0.0)
		{
			cameraCfg_.exposureTime2 = static_cast<int>(data._createModelExposureTime2);
			ui->btn_baoguang2->setText(QString::number(cameraCfg_.exposureTime2));
		}
		if (data._createModelGain2 > 0.0)
		{
			cameraCfg_.gain2 = static_cast<int>(data._createModelGain2);
			ui->btn_zengyi2->setText(QString::number(cameraCfg_.gain2));
		}
		// 将恢复的参数写入相机硬件
		applyExposure(global::CameraIndex::Camera1, cameraCfg_.exposureTime1,
			&Config::cameraCfg::exposureTime1);
		applyExposure(global::CameraIndex::Camera2, cameraCfg_.exposureTime2,
			&Config::cameraCfg::exposureTime2);
		applyGain(global::CameraIndex::Camera1, cameraCfg_.gain1,
			&Config::cameraCfg::gain1);
		applyGain(global::CameraIndex::Camera2, cameraCfg_.gain2,
			&Config::cameraCfg::gain2);
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

	void ModelEditorDialog::onExit()
	{
		// 修改模式或已创建过模板 → 弹窗询问是否保存；否则直接退出
		if (isModifyMode_ || modelCreated_)
		{
			if (rw::rqwu::MessageBox::question(this,
				QStringLiteral("退出"),
				QStringLiteral("是否创建模板？\n\n选择\"是\"保存模板后退出，选择\"否\"直接退出。"))
				== rw::rqwu::MessageBox::StandardButton::Yes)
			{
				// 退出保存时同步写回套版对齐参数（仅保存中心点的路径不会经过 onCreateModel）
				persistStampPatternAlignment();
				if (modelCreated_)
				{
					// 已创建过模板，只更新中心点等非训练参数，无需重新训练
					saveCenterPointToModel();
				}
				else
				{
					onCreateModel();
				}
				accept();
			}
			else
			{
				reject();
			}
			return;
		}
		accept();
	}

	void ModelEditorDialog::saveCenterPointToModel()
	{
		if (modelId_.empty() || !shapeEditor_)
			return;

		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun)
			return;

		const auto& inf = biz.infrastructure();
		if (!inf.shape_model_manager_module_)
			return;

		try
		{
			auto item = inf.shape_model_manager_module_->getShapeModelItem(modelId_);
			auto& data = item.data;

			if (shapeEditor_->hasCenterPoint())
			{
				data.centerX = shapeEditor_->centerPoint().x();
				data.centerY = shapeEditor_->centerPoint().y();
			}

			// 同时更新相机参数（用户可能在创建后调整了曝光/增益）
			data._createModelExposureTime = static_cast<double>(cameraCfg_.exposureTime1);
			data._createModelGain = static_cast<double>(cameraCfg_.gain1);
			data._createModelExposureTime2 = static_cast<double>(cameraCfg_.exposureTime2);
			data._createModelGain2 = static_cast<double>(cameraCfg_.gain2);

			// 持久化套版关联（修改模式下可能更换了套版）
			data.stampPatternId = stampPatternId_;

			// 持久化偏差补偿（无需重新训练即可生效的参数）
			data.offsetX = offsetX_;
			data.offsetY = offsetY_;
			data.offsetAngle = offsetAngle_;

			inf.shape_model_manager_module_->changeShapeModelItem(modelId_, data);
		}
		catch (...) {}
	}
bool ModelEditorDialog::requireImage(const QString& action) const
{
	if (lastFrame_.IsInitialized())
		return true;
	rw::rqwu::MessageBox::warning(const_cast<ModelEditorDialog*>(this),
		QStringLiteral("提示"),
		QStringLiteral("请等待图像显示后再进行%1操作").arg(action));
	return false;
}

bool ModelEditorDialog::isInDrawingTool() const
{
	if (!shapeEditor_)
		return false;
	const auto tool = shapeEditor_->tool();
	return tool == ShapeEditor::Tool::RectangleROI || tool == ShapeEditor::Tool::FreehandROI ||
	       tool == ShapeEditor::Tool::RectangleMask || tool == ShapeEditor::Tool::FreehandMask;
}

} // namespace ui
