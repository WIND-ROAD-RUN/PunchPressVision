#include "ToolNinePointWindow.hpp"
#include "ui_ToolNinePointWindow.h"

#include "global/GlobalPath.hpp"
#include "infTool/NinePointInfTool/NinePointInfTool.hpp"
#include "infTool/TwoCameraSpliceInfTool/TwoCameraSpliceInfTool.hpp"
#include "infTool/CalibInfTool/CalibInfTool.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infrastructure/NinePointModule/Config/NinePointConfig.hpp"
#include "infrastructure/NinePointModule/NinePointModule.hpp"
#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModulePath.hpp"
#include "rwul/hoecm/hoec_m.hpp"

#include <filesystem>

#include <QCloseEvent>
#include <QEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include <algorithm>

#include <opencv2/imgcodecs.hpp>

// ===================================================================
// 辅助函数
// ===================================================================
static int halconCountObj(const HalconCpp::HObject* obj)
{
	if (!obj)
		return 0;
	try
	{
		HalconCpp::HTuple n;
		HalconCpp::CountObj(*obj, &n);
		return n.I();
	}
	catch (...)
	{
		return 0;
	}
}

// ===================================================================
// cv::Mat → Halcon HImage 转换
// ===================================================================
HalconCpp::HImage ToolNinePointWindow::cvMatToHImage(const cv::Mat& mat)
{
	if (mat.empty())
		return HalconCpp::HImage();

	const int width  = mat.cols;
	const int height = mat.rows;

	HalconCpp::HImage hImage;
	switch (mat.type())
	{
	case CV_8UC1:
		HalconCpp::GenImage1(&hImage, "byte", width, height,
			reinterpret_cast<Hlong>(mat.data));
		return hImage;
	case CV_16UC1:
		HalconCpp::GenImage1(&hImage, "uint2", width, height,
			reinterpret_cast<Hlong>(mat.data));
		return hImage;
	case CV_8UC3:
		HalconCpp::GenImageInterleaved(&hImage, reinterpret_cast<Hlong>(mat.data),
			"bgr", width, height, 0, "byte", width, height, 0, 0, -1, 0);
		return hImage;
	default:
		return HalconCpp::HImage();
	}
}

// ===================================================================
ToolNinePointWindow::ToolNinePointWindow(inf::infrastructure& inf, QWidget* parent)
	: QMainWindow(parent)
	, inf_(inf)
	, ninePointTool_(std::make_unique<infTool::NinePointInfTool>(inf_))
	, calibTool_(std::make_unique<infTool::CalibInfTool>(inf_))
	, spliceTool_(std::make_unique<infTool::TwoCameraSpliceInfTool>(inf_, *calibTool_))
	, ui(new Ui::ToolNinePointWindowClass())
{
	ui->setupUi(this);

	ninePointTool_->build();
	calibTool_->build();
	spliceTool_->build();

	buildUi();
	buildConnections();
	syncConfigToUi();
	applyCalibParams();
}

ToolNinePointWindow::~ToolNinePointWindow()
{
	spliceTool_->destroy();
	calibTool_->destroy();
	ninePointTool_->destroy();
	closeHalconWindow();

	delete paintCreateRoiObj_;
	delete findCreateXldObj_;
	delete hv_ModelID_;
	delete hv_MetrologyHandle_;

	delete ui;
}

// ===================================================================
// 将 .ui 中的 QLabel 占位符替换为 native QWidget（承载 Halcon 显示）
// ===================================================================
QWidget* ToolNinePointWindow::replaceLabel(const QString& labelName)
{
	auto* old = findChild<QLabel*>(labelName);
	if (!old)
		return nullptr;

	auto* host = new QWidget(old->parentWidget());
	host->setObjectName(old->objectName() + "_host");
	host->setStyleSheet(QStringLiteral("background-color: #333;"));
	host->setSizePolicy(old->sizePolicy());
	host->setMinimumSize(old->minimumSize());
	host->setMaximumSize(old->maximumSize());

	host->setAttribute(Qt::WA_NativeWindow, true);
	host->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

	host->setFocusPolicy(Qt::StrongFocus);
	host->installEventFilter(this);

	if (auto* lay = old->parentWidget() ? old->parentWidget()->layout() : nullptr)
		lay->replaceWidget(old, host);
	else
		host->setGeometry(old->geometry());

	host->show();
	old->hide();
	old->deleteLater();
	return host;
}

void ToolNinePointWindow::buildUi()
{
	hostDisplay_ = replaceLabel(QStringLiteral("label_imgDisplay"));

	// 配置九点数据表格（5列）
	if (ui && ui->tabWidget_ninePointData)
	{
		auto& t = ui->tabWidget_ninePointData;
		t->setColumnCount(5);
		t->setHorizontalHeaderLabels({ "Index", "像素X", "像素Y", "实际X", "实际Y" });
		t->setSelectionBehavior(QAbstractItemView::SelectRows);
		t->setSelectionMode(QAbstractItemView::SingleSelection);
		t->setEditTriggers(QAbstractItemView::NoEditTriggers);
		t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	}
}

void ToolNinePointWindow::buildConnections()
{
	// 相机帧信号 → DirectConnection 在采集线程中缓存
	if (inf_.camera_module_)
	{
		connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
			this, &ToolNinePointWindow::onCameraFrame, Qt::DirectConnection);
	}

	// 显示信号排队到 UI 线程
	connect(this, &ToolNinePointWindow::displayFrameReady,
		this, &ToolNinePointWindow::onDisplayFrame, Qt::QueuedConnection);

	// UI 按钮
	connect(ui->btn_readImage, &QPushButton::clicked, this, &ToolNinePointWindow::btn_readImage_clicked);
	connect(ui->btn_paintRegion, &QPushButton::clicked, this, &ToolNinePointWindow::btn_paintRegion_clicked);
	connect(ui->btn_findRectangle, &QPushButton::clicked, this, &ToolNinePointWindow::btn_findRectangle_clicked);
	connect(ui->btn_ninePointCalibration, &QPushButton::clicked, this, &ToolNinePointWindow::btn_ninePointCalibration_clicked);
	connect(ui->btn_completeCalibration, &QPushButton::clicked, this, &ToolNinePointWindow::btn_completeCalibration_clicked);
	connect(ui->btn_exit, &QPushButton::clicked, this, &ToolNinePointWindow::btn_exit_clicked);

	connect(ui->btn_baoguang1, &QPushButton::clicked, this, &ToolNinePointWindow::btn_baoguang1_clicked);
	connect(ui->btn_zengyi1, &QPushButton::clicked, this, &ToolNinePointWindow::btn_zengyi1_clicked);
	connect(ui->btn_baoguang2, &QPushButton::clicked, this, &ToolNinePointWindow::btn_baoguang2_clicked);
	connect(ui->btn_zengyi2, &QPushButton::clicked, this, &ToolNinePointWindow::btn_zengyi2_clicked);

	connect(ui->btn_MeasureLength1, &QPushButton::clicked, this, &ToolNinePointWindow::btn_MeasureLength1_clicked);
	connect(ui->btn_MeasureLength2, &QPushButton::clicked, this, &ToolNinePointWindow::btn_MeasureLength2_clicked);
	connect(ui->btn_Threshold, &QPushButton::clicked, this, &ToolNinePointWindow::btn_Threshold_clicked);
	connect(ui->btn_num_measures, &QPushButton::clicked, this, &ToolNinePointWindow::btn_num_measures_clicked);

	connect(ui->btn_startCamera1, &QPushButton::clicked, this, &ToolNinePointWindow::btn_startCamera1_clicked);
	connect(ui->btn_stopCamera1, &QPushButton::clicked, this, &ToolNinePointWindow::btn_stopCamera1_clicked);
	connect(ui->btn_startCamera2, &QPushButton::clicked, this, &ToolNinePointWindow::btn_startCamera2_clicked);
	connect(ui->btn_stopCamera2, &QPushButton::clicked, this, &ToolNinePointWindow::btn_stopCamera2_clicked);

	connect(ui->btn_remove, &QPushButton::clicked, this, &ToolNinePointWindow::btn_remove_clicked);
	connect(ui->btn_removeAll, &QPushButton::clicked, this, &ToolNinePointWindow::btn_removeAll_clicked);

	connect(ui->tabWidget_ninePointData, &QTableWidget::cellClicked,
		this, &ToolNinePointWindow::onNinePointTableClicked);

	connect(&cameraDisplayWatcher_, &QFutureWatcher<void>::finished, this,
		[this]() { /* 处理完成 */ });
}

// ===================================================================
// 配置同步
// ===================================================================
void ToolNinePointWindow::syncConfigToUi()
{
	if (!inf_.nine_point_module_)
		return;

	const auto& cfg = inf_.nine_point_module_->ninePointConfig;

	// 测量参数
	ui->btn_MeasureLength1->setText(QString::number(cfg.MeasureLength1));
	ui->btn_MeasureLength2->setText(QString::number(cfg.MeasureLength2));
	ui->btn_Threshold->setText(QString::number(cfg.MeasureThreshold));
	ui->btn_num_measures->setText(QString::number(cfg.num_Measure));

	// 相机曝光/增益
	ui->btn_baoguang1->setText(QString::number(cfg.camera1Exposure));
	ui->btn_zengyi1->setText(QString::number(cfg.camera1Gain));
	ui->btn_baoguang2->setText(QString::number(cfg.camera2Exposure));
	ui->btn_zengyi2->setText(QString::number(cfg.camera2Gain));
}

void ToolNinePointWindow::applyCalibParams()
{
	if (!inf_.nine_point_module_)
		return;

	const auto& cfg = inf_.nine_point_module_->ninePointConfig;

	// 应用相机曝光/增益到硬件
	if (inf_.camera_module_)
	{
		inf_.camera_module_->setExposure(global::CameraIndex::Camera1, cfg.camera1Exposure);
		inf_.camera_module_->setGain(global::CameraIndex::Camera1, cfg.camera1Gain);
		inf_.camera_module_->setExposure(global::CameraIndex::Camera2, cfg.camera2Exposure);
		inf_.camera_module_->setGain(global::CameraIndex::Camera2, cfg.camera2Gain);
	}

	// 九点参数同步到九点工具
	std::vector<infTool::Point2D> mechCoords;
	// 构建3x3网格机械坐标
	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 3; ++col)
		{
			infTool::Point2D pt;
			pt.x = cfg.xoffset + cfg.xdiantance * col;
			pt.y = cfg.ydistance * row;
			mechCoords.push_back(pt);
		}
	}
	ninePointTool_->setMechanicalCoords(mechCoords);
}

// ===================================================================
// Halcon 窗口管理
// ===================================================================
bool ToolNinePointWindow::ensureHalconWindow()
{
	if (!hostDisplay_)
		return false;

	if (halconWindowHandle_.Length() > 0)
		return true;

	const Hlong winId = static_cast<Hlong>(hostDisplay_->winId());
	const int w = std::max(1, hostDisplay_->width());
	const int h = std::max(1, hostDisplay_->height());

	try
	{
		HalconCpp::OpenWindow(0, 0, w, h, winId, "visible", "", &halconWindowHandle_);
		HalconCpp::SetWindowAttr("background_color", "gray");
	}
	catch (...)
	{
		qWarning() << "OpenHalconWindow failed";
		return false;
	}

	return true;
}

void ToolNinePointWindow::closeHalconWindow()
{
	try
	{
		if (halconWindowHandle_.Length() > 0)
			HalconCpp::CloseWindow(halconWindowHandle_);
	}
	catch (...) {}

	halconWindowHandle_ = HalconCpp::HTuple();

	viewPartValid_ = false;
	viewImgW_ = 0;
	viewImgH_ = 0;
	isPanning_ = false;
}

void ToolNinePointWindow::redrawHalconView(bool clearWindow)
{
	if (!ensureHalconWindow())
		return;
	if (!halconLastImage_.IsInitialized())
		return;
	if (!ensureHalconViewPart())
		return;

	const qreal dpr = hostDisplay_ ? hostDisplay_->devicePixelRatioF() : 1.0;
	const int winW = std::max(1, static_cast<int>(std::lround(hostDisplay_->width() * dpr)));
	const int winH = std::max(1, static_cast<int>(std::lround(hostDisplay_->height() * dpr)));

	try
	{
		using namespace HalconCpp;
		SetWindowExtents(halconWindowHandle_, 0, 0, winW, winH);

		if (clearWindow)
			ClearWindow(halconWindowHandle_);

		HalconViewPart partToShow = viewPart_;
		const double partW = partToShow.c2 - partToShow.c1;
		const double partH = partToShow.r2 - partToShow.r1;
		const double eps = 1e-9;
		if (partW > eps && partH > eps)
		{
			const double winAspect = (winH > 0) ? static_cast<double>(winW) / winH : 1.0;
			const double partAspect = partW / partH;
			if (winAspect > partAspect)
			{
				const double newW = partH * winAspect;
				const double pad = (newW - partW) * 0.5;
				partToShow.c1 -= pad;
				partToShow.c2 += pad;
			}
			else if (winAspect < partAspect)
			{
				const double newH = partW / winAspect;
				const double pad = (newH - partH) * 0.5;
				partToShow.r1 -= pad;
				partToShow.r2 += pad;
			}
		}

		SetPart(halconWindowHandle_, partToShow.r1, partToShow.c1, partToShow.r2, partToShow.c2);
		DispObj(halconLastImage_, halconWindowHandle_);
	}
	catch (...) { return; }

	// 叠加 ROI / 轮廓
	try
	{
		using namespace HalconCpp;
		SetDraw(halconWindowHandle_, "margin");
		SetLineWidth(halconWindowHandle_, 2);

		auto dispObj = [&](const char* color, const HObject* obj)
			{
				if (!obj) return;
				HTuple n;
				CountObj(*obj, &n);
				if (n.I() <= 0) return;
				SetColor(halconWindowHandle_, color);
				for (int i = 1; i <= n.I(); ++i)
				{
					HObject one;
					SelectObj(*obj, &one, i);
					DispObj(one, halconWindowHandle_);
				}
			};

		if (showPaintCreateRoiObj_)
			dispObj("green", paintCreateRoiObj_);

		if (findCreateXldObj_)
		{
			HTuple n;
			CountObj(*findCreateXldObj_, &n);
			if (n.I() > 0)
			{
				SetColor(halconWindowHandle_, "cyan");
				DispObj(*findCreateXldObj_, halconWindowHandle_);
			}
		}
	}
	catch (...) {}
}

// ===================================================================
// 事件
// ===================================================================
bool ToolNinePointWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == hostDisplay_)
	{
		switch (event->type())
		{
		case QEvent::Wheel:
		{
			auto* e = static_cast<QWheelEvent*>(event);
			const int delta = e->angleDelta().y();
			if (delta == 0) return true;
			int steps = delta / 120;
			if (steps == 0) steps = (delta > 0) ? 1 : -1;
			zoomHalconViewAt(e->position().toPoint(), steps);
			redrawHalconView(true);
			return true;
		}
		case QEvent::MouseButtonPress:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (e->button() == Qt::LeftButton)
			{
				if (!ensureHalconViewPart()) return true;
				isPanning_ = true;
				panStartPos_ = e->pos();
				panStartPart_ = viewPart_;
				return true;
			}
			break;
		}
		case QEvent::MouseMove:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (isPanning_ && (e->buttons() & Qt::LeftButton))
			{
				const QPoint delta = e->pos() - panStartPos_;
				panHalconViewFromDrag(delta);
				redrawHalconView(true);
				return true;
			}
			break;
		}
		case QEvent::MouseButtonRelease:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (e->button() == Qt::LeftButton)
			{
				isPanning_ = false;
				return true;
			}
			break;
		}
		default: break;
		}
	}
	return QMainWindow::eventFilter(watched, event);
}

void ToolNinePointWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
	ensureHalconWindow();

	// 软件打开时加载双相机拼接配置（MapSingle1/MapSingle2 等）
	if (inf_.two_camera_splice_module_)
		inf_.two_camera_splice_module_->twoCameraSpliceConfig.loadInDir(inf::TwoCameraSpliceModulePath.RootPath);

	if (inf_.camera_module_)
		inf_.camera_module_->startMonitor();
}

void ToolNinePointWindow::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);
	if (!ensureHalconWindow()) return;
	try
	{
		HalconCpp::SetWindowExtents(halconWindowHandle_, 0, 0,
			std::max(1, hostDisplay_ ? hostDisplay_->width() : width()),
			std::max(1, hostDisplay_ ? hostDisplay_->height() : height()));
		redrawHalconView(false);
	}
	catch (...) {}
}

void ToolNinePointWindow::closeEvent(QCloseEvent* event)
{
	if (inf_.nine_point_module_)
	{
		const auto btn = QMessageBox::question(this,
			QStringLiteral("保存"),
			QStringLiteral("是否保存九点标定配置？"),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
		if (btn == QMessageBox::Yes)
			inf_.nine_point_module_->save();
		else
			inf_.nine_point_module_->skipSaveOnDestroy = true;
	}
	if (inf_.camera_module_)
		inf_.camera_module_->stopMonitor();
	closeHalconWindow();
	QMainWindow::closeEvent(event);
}

// ===================================================================
// 相机帧回调（DirectConnection，采集线程）
// ===================================================================
void ToolNinePointWindow::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
{
	if (!isShowImg_)
		return;

	switch (cameraIndex)
	{
	case global::CameraIndex::Camera1:
		rawMat1_   = matInfo.mat.clone();
		cam1Image_ = cvMatToHImage(matInfo.mat);
		cam1Ready_ = true;
		break;
	case global::CameraIndex::Camera2:
		rawMat2_   = matInfo.mat.clone();
		cam2Image_ = cvMatToHImage(matInfo.mat);
		cam2Ready_ = true;
		break;
	}

	emit displayFrameReady();
}

void ToolNinePointWindow::onDisplayFrame()
{
	// 等待双相机帧均就绪后才拼接显示
	if (!cam1Ready_ || !cam2Ready_)
		return;

	HalconCpp::HObject displayImage;
	bool haveImage = false;

	// 使用 TwoCameraSpliceInfTool 的 pinjieImage 进行拼接
	auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

	{
		HalconCpp::HObject ho1 = static_cast<HalconCpp::HObject>(cam1Image_);
		HalconCpp::HObject ho2 = static_cast<HalconCpp::HObject>(cam2Image_);
		HalconCpp::HObject stitched;
		if (spliceTool_->pinjieImage(ho1, ho2, spliceCfg, stitched))
		{
			displayImage = stitched;
			haveImage = true;
		}
	}

	// 降级：无 Map 则原图左右硬拼接
	if (!haveImage)
	{
		try
		{
			HalconCpp::HObject concat;
			HalconCpp::ConcatObj(cam1Image_, cam2Image_, &concat);
			HalconCpp::TileImages(concat, &displayImage, 2, "horizontal");
			haveImage = displayImage.IsInitialized();
		}
		catch (const HalconCpp::HException&) { haveImage = false; }
	}

	cam1Ready_ = false;
	cam2Ready_ = false;

	if (!haveImage)
		return;

	halconLastImage_ = HalconCpp::HImage(displayImage);

	if (ensureHalconWindow())
	{
		redrawHalconView(true);

		// 九点标定模式下自动检测矩形（基于拼接图像，与 DlgJiudianbiaoding 一致）
		if (isJiuDianBiaoDing_ && halconLastImage_.IsInitialized())
		{
			QVector<RectangleResult> results;
			const bool found = findRectangle(halconLastImage_, results, true, 3);

			if (found && !results.isEmpty())
			{
				// 按 x(Col) 坐标从小到大排序，确保与实际位置一一对应（与 DlgJiudianbiaoding 一致）
				std::sort(results.begin(), results.end(),
					[](const RectangleResult& a, const RectangleResult& b) { return a.x < b.x; });
				for (const auto& rect : results)
				{
					// 计算实际坐标（3x3网格）
					const int idx = static_cast<int>(pixPoints_.size());
					double realX = 0.0, realY = 0.0;

					if (inf_.nine_point_module_)
					{
						const auto& cfg = inf_.nine_point_module_->ninePointConfig;
						realX = cfg.xoffset + static_cast<double>(cfg.xdiantance * (idx % 3));
						realY = static_cast<double>(cfg.ydistance * (idx / 3));
					}

					infTool::Point2D pix{ rect.x, rect.y };
					infTool::Point2D world{ realX, realY };

					pixPoints_.push_back(pix);
					worldPoints_.push_back(world);

					addNewItemToListView(rect.x, rect.y, realX, realY);
				}

				if (pixPoints_.size() >= 9)
				{
					ui->btn_ninePointCalibration->setEnabled(true);
					isJiuDianBiaoDing_ = false;
					QMessageBox::information(this,
						QStringLiteral("提示"),
						QStringLiteral("已完成9个标定点采集，可以进行九点标定了！"));
				}

				for (auto& r : results)
				{
					delete r.contour;
					r.contour = nullptr;
				}
			}
		}
	}
}

// ===================================================================
// 读取图片
// ===================================================================
void ToolNinePointWindow::btn_readImage_clicked()
{
	const std::string configDir = global::path::configDir();
	static const char* kExtensions[] = { ".bmp", ".png", ".jpg", ".jpeg" };

	std::string filePath;
	for (const auto* ext : kExtensions)
	{
		const auto candidate = configDir + "/model" + ext;
		if (std::filesystem::exists(candidate))
		{
			filePath = candidate;
			break;
		}
	}

	if (filePath.empty())
	{
		// 目标路径下没有 model 图片 → 保存当前图像
		if (!halconLastImage_.IsInitialized())
		{
			QMessageBox::warning(this, QStringLiteral("提示"),
				QStringLiteral("config 目录下未找到 model 图片，且当前无图像可保存。"));
			return;
		}

		const std::string savePath = configDir + "/model.bmp";
		std::filesystem::create_directories(configDir);
		halconLastImage_.WriteImage("bmp", 0, savePath.c_str());
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("已将当前图像保存为 model 图片。"));
		return;
	}

	cv::Mat bgr = cv::imread(filePath, cv::IMREAD_COLOR);
	if (bgr.empty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("图片解码失败！"));
		return;
	}

	halconLastImage_ = cvMatToHImage(bgr);
	redrawHalconView(true);
}

// ===================================================================
// ROI 绘制
// ===================================================================
void ToolNinePointWindow::setRoiEditingUiEnabled(bool enabled)
{
	if (!ui) return;
	ui->btn_readImage->setEnabled(enabled);
	ui->btn_paintRegion->setEnabled(enabled);
	ui->btn_findRectangle->setEnabled(enabled);
	ui->btn_ninePointCalibration->setEnabled(enabled);
	ui->btn_exit->setEnabled(enabled);
}

void ToolNinePointWindow::paintCreateRegion()
{
	isShowImg_ = false;

	if (!halconLastImage_.IsInitialized())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载图片。"));
		isShowImg_ = true;
		return;
	}
	if (!ensureHalconWindow())
	{
		isShowImg_ = true;
		return;
	}

	setRoiEditingUiEnabled(false);

	try
	{
		using namespace HalconCpp;

		HTuple w, h;
		GetImageSize(halconLastImage_, &w, &h);

		HTuple row1, col1, row2, col2;
		DrawRectangle1(halconWindowHandle_, &row1, &col1, &row2, &col2);

		HObject roiObj;
		GenRectangle1(&roiObj, row1.D(), col1.D(), row2.D(), col2.D());

		delete paintCreateRoiObj_;
		paintCreateRoiObj_ = new HObject();
		*paintCreateRoiObj_ = roiObj;

		delete findCreateXldObj_;
		findCreateXldObj_ = new HObject();
		GenEmptyObj(findCreateXldObj_);

		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		QMessageBox::warning(this, QStringLiteral("Halcon异常"),
			QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...) {}

	setRoiEditingUiEnabled(true);
	isShowImg_ = true;
}

void ToolNinePointWindow::btn_paintRegion_clicked()
{
	showPaintCreateRoiObj_ = true;

	// 清空上次的 ROI / 轮廓
	try
	{
		delete paintCreateRoiObj_;
		paintCreateRoiObj_ = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(paintCreateRoiObj_);

		delete findCreateXldObj_;
		findCreateXldObj_ = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(findCreateXldObj_);
	}
	catch (...) {}

	redrawHalconView(true);

	ui->btn_readImage->setEnabled(false);
	ui->btn_findRectangle->setEnabled(false);
	ui->btn_exit->setEnabled(false);

	paintCreateRegion();
}

// ===================================================================
// 模板创建
// ===================================================================
bool ToolNinePointWindow::createShapeModelFromRois()
{
	try
	{
		if (!halconLastImage_.IsInitialized()) return false;
		if (!paintCreateRoiObj_ || halconCountObj(paintCreateRoiObj_) <= 0) return false;

		HalconCpp::HObject hoCreate = *paintCreateRoiObj_;
		HalconCpp::HObject hoCreateUnion;
		HalconCpp::Union1(hoCreate, &hoCreateUnion);

		HalconCpp::HImage imgReduced;
		HalconCpp::ReduceDomain(halconLastImage_, hoCreateUnion, &imgReduced);

		HalconCpp::HTuple modelIdLocal;
		HalconCpp::CreateShapeModel(imgReduced,
			"auto", 0, HalconCpp::HTuple(360).TupleRad(),
			"auto", "auto", "use_polarity", "auto", "auto",
			&modelIdLocal);

		delete hv_ModelID_;
		hv_ModelID_ = new HalconCpp::HTuple(modelIdLocal);

		HalconCpp::HTuple hv_Row, hv_Column, hv_Angle, hv_Score;
		HalconCpp::FindShapeModel(halconLastImage_,
			*hv_ModelID_, 0, HalconCpp::HTuple(360).TupleRad(),
			0.5, 1, 0.5, "least_squares", 0, 0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		if (hv_Row.TupleLength() < 1) return false;

		HalconCpp::HObject ho_ModelContours, ho_ContoursAffineTrans;
		HalconCpp::HTuple hv_HomMat2D;
		HalconCpp::GetShapeModelContours(&ho_ModelContours, *hv_ModelID_, 1);
		HalconCpp::VectorAngleToRigid(0, 0, 0, hv_Row[0], hv_Column[0], hv_Angle[0], &hv_HomMat2D);
		HalconCpp::AffineTransContourXld(ho_ModelContours, &ho_ContoursAffineTrans, hv_HomMat2D);

		delete findCreateXldObj_;
		findCreateXldObj_ = new HalconCpp::HObject();
		*findCreateXldObj_ = ho_ContoursAffineTrans;

		redrawHalconView(true);

		const double score = (hv_Score.TupleLength() > 0) ? hv_Score[0].D() : 0.0;
		QMessageBox::information(this, QStringLiteral("提示"),
			QString("建模成功，初次匹配score=%1").arg(score));
		return true;
	}
	catch (const HalconCpp::HException& e)
	{
		QMessageBox::critical(this, QStringLiteral("Halcon异常"),
			QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	return false;
}

void ToolNinePointWindow::createShapeModel()
{
	if (!paintCreateRoiObj_ || halconCountObj(paintCreateRoiObj_) <= 0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先绘制ROI。"));
		return;
	}
	if (!halconLastImage_.IsInitialized())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载图片。"));
		return;
	}
	createShapeModelFromRois();
}

// ===================================================================
// 查找矩形
// ===================================================================
bool ToolNinePointWindow::findRectangle(const HalconCpp::HImage& hImg,
	QVector<RectangleResult>& results, bool showMessage, int maxCount)
{
	results.clear();

	try
	{
		HalconCpp::HTuple w, h;
		HalconCpp::GetImageSize(hImg, &w, &h);
		if (w.I() <= 0 || h.I() <= 0)
		{
			if (showMessage)
				QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("传入图像为空，无法识别。"));
			return false;
		}
	}
	catch (...)
	{
		if (showMessage)
			QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("传入图像为空，无法识别。"));
		return false;
	}

	if (!hv_ModelID_)
	{
		if (showMessage)
			QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先创建模板（CreateShapeModel）。"));
		return false;
	}

	if (!paintCreateRoiObj_ || halconCountObj(paintCreateRoiObj_) <= 0)
	{
		if (showMessage)
			QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("没有可用的绘制矩形（Create ROI）。"));
		return false;
	}

	try
	{
		using namespace HalconCpp;

		HTuple hv_Row, hv_Column, hv_Angle, hv_Score;
		FindShapeModel(hImg, *hv_ModelID_,
			0, HTuple(360).TupleRad(), 0.2, maxCount, 0.5,
			"least_squares", 0, 0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		const int matchCount = hv_Row.TupleLength();
		if (matchCount < 1)
		{
			if (showMessage)
				QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("没有找到模板匹配结果。"));
			return false;
		}

		HTuple hv_Width, hv_Height;
		GetImageSize(hImg, &hv_Width, &hv_Height);

		auto readBtnDouble = [](const QPushButton* btn, double fallback) -> double
		{
			if (!btn) return fallback;
			bool ok = false;
			const double v = btn->text().trimmed().toDouble(&ok);
			return ok ? v : fallback;
		};

		double measureLength1 = readBtnDouble(ui ? ui->btn_MeasureLength1 : nullptr, 100.0);
		double measureLength2 = readBtnDouble(ui ? ui->btn_MeasureLength2 : nullptr, 50.0);
		double measureSigma   = readBtnDouble(ui ? ui->btn_num_measures : nullptr, 1.0);
		double measureThreshold = readBtnDouble(ui ? ui->btn_Threshold : nullptr, 5.0);

		measureLength1 = std::max(1.0, measureLength1);
		measureLength2 = std::max(1.0, measureLength2);
		measureSigma = std::max(0.1, measureSigma);
		measureThreshold = std::max(0.0, measureThreshold);

		double roiW = 0.0, roiH = 0.0;
		{
			HObject one;
			SelectObj(*paintCreateRoiObj_, &one, 1);
			HTuple r1, c1, r2, c2;
			SmallestRectangle1(one, &r1, &c1, &r2, &c2);
			roiW = std::abs(c2.D() - c1.D());
			roiH = std::abs(r2.D() - r1.D());
		}

		if (roiW <= 0.0 || roiH <= 0.0)
		{
			if (showMessage)
				QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("ROI尺寸无效，无法进行测量。"));
			return false;
		}

		HObject allContours;
		GenEmptyObj(&allContours);

		for (int i = 0; i < matchCount; ++i)
		{
			const double foundRow = hv_Row[i].D();
			const double foundCol = hv_Column[i].D();
			const double foundAngle = hv_Angle[i].D();

			HTuple hv_MetrologyLocal;
			CreateMetrologyModel(&hv_MetrologyLocal);
			SetMetrologyModelImageSize(hv_MetrologyLocal, hv_Width, hv_Height);

			HTuple hv_Index;
			AddMetrologyObjectRectangle2Measure(hv_MetrologyLocal,
				foundRow, foundCol, foundAngle,
				roiW / 2.0, roiH / 2.0,
				measureLength1, measureLength2, measureSigma, measureThreshold,
				HTuple(), HTuple(), &hv_Index);

			bool applied = true;
			try { ApplyMetrologyModel(hImg, hv_MetrologyLocal); }
			catch (...) { applied = false; }

			RectangleResult result;

			if (applied)
			{
				try
				{
					HObject* hoResultContour = new HObject();
					GetMetrologyObjectResultContour(hoResultContour, hv_MetrologyLocal, 0, "all", 1.5);
					result.contour = hoResultContour;
					result.valid = (halconCountObj(result.contour) > 0);

					if (result.valid)
					{
						HTuple row, col, phi, l1, l2;
						SmallestRectangle2Xld(*result.contour, &row, &col, &phi, &l1, &l2);
						if (row.TupleLength() > 0) { result.x = col[0].D(); result.y = row[0].D(); }
						else { result.x = foundCol; result.y = foundRow; }
					}

					if (result.valid)
					{
						HObject temp;
						ConcatObj(allContours, *result.contour, &temp);
						allContours = temp;
					}
				}
				catch (...)
				{
					result.valid = false;
					result.x = foundCol;
					result.y = foundRow;
					delete result.contour;
					result.contour = nullptr;
				}
			}
			else
			{
				result.x = foundCol;
				result.y = foundRow;
			}

			results.append(result);

			if (i == 0)
			{
				delete hv_MetrologyHandle_;
				hv_MetrologyHandle_ = new HTuple(hv_MetrologyLocal);
			}
		}

		delete findCreateXldObj_;
		findCreateXldObj_ = new HObject();
		*findCreateXldObj_ = allContours;

		redrawHalconView(true);
		return !results.isEmpty();
	}
	catch (const HalconCpp::HException& e)
	{
		if (showMessage)
			QMessageBox::critical(this, QStringLiteral("Halcon异常"),
				QString::fromLocal8Bit(e.ErrorMessage().Text()));
		return false;
	}
}

void ToolNinePointWindow::btn_findRectangle_clicked()
{
	showPaintCreateRoiObj_ = true;
	createShapeModel();

	QVector<RectangleResult> results;
	if (halconLastImage_.IsInitialized())
	{
		const bool found = findRectangle(halconLastImage_, results, true, 3);
		if (found && !results.isEmpty())
		{
			QString msg = QString("找到 %1 个矩形轮廓。\n").arg(results.size());
			for (int i = 0; i < results.size(); ++i)
			{
				msg += QString("矩形%1: (Col=X)=%2, (Row=Y)=%3\n")
					.arg(i + 1).arg(results[i].x, 0, 'f', 2).arg(results[i].y, 0, 'f', 2);
			}
			QMessageBox::information(this, QStringLiteral("提示"), msg);

			// 在图上绘制找到的所有点（与 DlgJiudianbiaoding 一致）
			try
			{
				HalconCpp::HObject allCrosses;
				HalconCpp::GenEmptyObj(&allCrosses);

				for (const auto& result : results)
				{
					HalconCpp::HObject hoCross;
					HalconCpp::GenCrossContourXld(&hoCross, result.y, result.x, 30.0, 0.0);

					HalconCpp::HObject temp;
					HalconCpp::ConcatObj(allCrosses, hoCross, &temp);
					allCrosses = temp;
				}

				if (findCreateXldObj_ && halconCountObj(findCreateXldObj_) > 0)
				{
					HalconCpp::HObject out;
					HalconCpp::ConcatObj(*findCreateXldObj_, allCrosses, &out);
					*findCreateXldObj_ = out;
				}
				else
				{
					*findCreateXldObj_ = allCrosses;
				}

				redrawHalconView(false);
			}
			catch (...)
			{
				// 绘制失败不影响主流程
			}
		}
		else
		{
			QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未找到矩形轮廓。"));
		}

		for (auto& r : results) { delete r.contour; r.contour = nullptr; }
	}
	else
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载图片。"));
	}
}

// ===================================================================
// 九点标定按钮
// ===================================================================
void ToolNinePointWindow::clearNinePointResults()
{
	pixPoints_.clear();
	worldPoints_.clear();
	clearNinePointImages();

	isJiuDianBiaoDing_ = false;
	if (ui) ui->btn_ninePointCalibration->setEnabled(false);
}

void ToolNinePointWindow::btn_ninePointCalibration_clicked()
{
	// 清空双相机缓存（防止回调中残留数据污染新一轮采集）
	cam1Image_ = HalconCpp::HImage();
	cam2Image_ = HalconCpp::HImage();
	cam1Ready_ = false;
	cam2Ready_ = false;

	// 开始新一轮采集前：如果已有历史点，先确认是否清空
	if (!pixPoints_.empty())
	{
		const auto ret = QMessageBox::question(this,
			QStringLiteral("确认"),
			QStringLiteral("检测到已有历史九点标定数据，是否清空后重新采集？"));
		if (ret != QMessageBox::Yes) return;
		clearNinePointResults();
	}
	else
	{
		clearNinePointImages();
		if (ui) ui->btn_ninePointCalibration->setEnabled(false);
	}

	applyCalibParams();

	// 切换到硬触发模式（Line0），等待外部触发出图
	if (inf_.camera_module_)
	{
		inf_.camera_module_->setTriggerMode(global::CameraIndex::Camera1, global::TriggerSource::Line0, 2.0);
		inf_.camera_module_->setTriggerMode(global::CameraIndex::Camera2, global::TriggerSource::Line0, 2.0);
	}

	// 2s 后再进入九点标定状态（异步，不阻塞UI；等待相机回调切换完成）
	QTimer::singleShot(2000, this, [this]()
	{
		isJiuDianBiaoDing_ = true;
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("准备完成"));
	});
}

void ToolNinePointWindow::btn_completeCalibration_clicked()
{
	if (pixPoints_.size() < 3)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("九点标定点位不足（至少需要3个点）。"));
		return;
	}

	if (!inf_.nine_point_module_)
		return;

	auto& cfg = inf_.nine_point_module_->ninePointConfig;

	const bool ok = ninePointTool_->calcPixToWorldHomMat2D(pixPoints_, worldPoints_, cfg);
	if (!ok)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("计算像素->世界变换矩阵失败，请检查点位数据。"));
		return;
	}

	inf_.nine_point_module_->save();

	QMessageBox::information(this, QStringLiteral("提示"),
		QStringLiteral("九点标定完成：已计算并保存像素->世界 HomMat2D。"));
}

void ToolNinePointWindow::btn_exit_clicked()
{
	close();  // closeEvent 中统一处理保存询问
}

// ===================================================================
// 相机曝光/增益
// ===================================================================
void ToolNinePointWindow::btn_baoguang1_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机1曝光"),
		QStringLiteral("曝光值 (us):"), ui->btn_baoguang1->text().toDouble(&ok),
		0, 10000000, 0, &ok);
	if (!ok) return;
	ui->btn_baoguang1->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setExposure(global::CameraIndex::Camera1, v);
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.camera1Exposure = v;
}

void ToolNinePointWindow::btn_zengyi1_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机1增益"),
		QStringLiteral("增益值:"), ui->btn_zengyi1->text().toDouble(&ok), 0, 100, 2, &ok);
	if (!ok) return;
	ui->btn_zengyi1->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setGain(global::CameraIndex::Camera1, v);
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.camera1Gain = v;
}

void ToolNinePointWindow::btn_baoguang2_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机2曝光"),
		QStringLiteral("曝光值 (us):"), ui->btn_baoguang2->text().toDouble(&ok),
		0, 10000000, 0, &ok);
	if (!ok) return;
	ui->btn_baoguang2->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setExposure(global::CameraIndex::Camera2, v);
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.camera2Exposure = v;
}

void ToolNinePointWindow::btn_zengyi2_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机2增益"),
		QStringLiteral("增益值:"), ui->btn_zengyi2->text().toDouble(&ok), 0, 100, 2, &ok);
	if (!ok) return;
	ui->btn_zengyi2->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setGain(global::CameraIndex::Camera2, v);
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.camera2Gain = v;
}

// ===================================================================
// 测量参数按钮
// ===================================================================
void ToolNinePointWindow::btn_MeasureLength1_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("测量矩形高"),
		QStringLiteral("高度:"), ui->btn_MeasureLength1->text().toDouble(&ok),
		1, 9999, 0, &ok);
	if (!ok) return;
	ui->btn_MeasureLength1->setText(QString::number(v));
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.MeasureLength1 = v;
}

void ToolNinePointWindow::btn_MeasureLength2_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("测量矩形宽"),
		QStringLiteral("宽度:"), ui->btn_MeasureLength2->text().toDouble(&ok),
		1, 9999, 0, &ok);
	if (!ok) return;
	ui->btn_MeasureLength2->setText(QString::number(v));
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.MeasureLength2 = v;
}

void ToolNinePointWindow::btn_Threshold_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("测量矩形对比度"),
		QStringLiteral("对比度:"), ui->btn_Threshold->text().toDouble(&ok),
		0, 255, 0, &ok);
	if (!ok) return;
	ui->btn_Threshold->setText(QString::number(v));
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.MeasureThreshold = v;
}

void ToolNinePointWindow::btn_num_measures_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("测量矩形个数"),
		QStringLiteral("个数:"), ui->btn_num_measures->text().toDouble(&ok),
		1, 100, 0, &ok);
	if (!ok) return;
	ui->btn_num_measures->setText(QString::number(v));
	if (inf_.nine_point_module_)
		inf_.nine_point_module_->ninePointConfig.num_Measure = v;
}

// ===================================================================
// 开始/停止拍照
// ===================================================================
void ToolNinePointWindow::btn_startCamera1_clicked()
{
	isJiuDianBiaoDing_ = false;
	if (inf_.camera_module_)
		inf_.camera_module_->setFreeRunMode(global::CameraIndex::Camera1, 2.0);
}

void ToolNinePointWindow::btn_stopCamera1_clicked()
{
	if (inf_.camera_module_)
		inf_.camera_module_->setTriggerMode(global::CameraIndex::Camera1, global::TriggerSource::Line0, 2.0);
}

void ToolNinePointWindow::btn_startCamera2_clicked()
{
	isJiuDianBiaoDing_ = false;
	if (inf_.camera_module_)
		inf_.camera_module_->setFreeRunMode(global::CameraIndex::Camera2, 2.0);
}

void ToolNinePointWindow::btn_stopCamera2_clicked()
{
	if (inf_.camera_module_)
		inf_.camera_module_->setTriggerMode(global::CameraIndex::Camera2, global::TriggerSource::Line0, 2.0);
}

// ===================================================================
// 移除/移除所有
// ===================================================================
void ToolNinePointWindow::btn_remove_clicked()
{
	// 移除最后一项
	if (!pixPoints_.empty())
	{
		pixPoints_.pop_back();
		worldPoints_.pop_back();
	}
	if (ui->tabWidget_ninePointData && ui->tabWidget_ninePointData->rowCount() > 0)
		ui->tabWidget_ninePointData->removeRow(ui->tabWidget_ninePointData->rowCount() - 1);
	listViewCount_ = ui->tabWidget_ninePointData ? ui->tabWidget_ninePointData->rowCount() : 0;
}

void ToolNinePointWindow::btn_removeAll_clicked()
{
	const auto ret = QMessageBox::question(this,
		QStringLiteral("警告"),
		QStringLiteral("是否确认要清空所有九点标定数据？"));
	if (ret != QMessageBox::Yes) return;
	clearNinePointResults();
}

// ===================================================================
// 表格操作
// ===================================================================
void ToolNinePointWindow::onNinePointTableClicked(int row, int column)
{
	(void)column;

	if (row < 0 || row >= static_cast<int>(pixPoints_.size()))
		return;

	// 在 Halcon 窗口上高亮对应点
	if (ensureHalconWindow() && halconLastImage_.IsInitialized())
	{
		try
		{
			HalconCpp::HObject hoCross;
			HalconCpp::GenCrossContourXld(&hoCross,
				pixPoints_[row].y, pixPoints_[row].x, 60.0, 0.0);

			delete findCreateXldObj_;
			findCreateXldObj_ = new HalconCpp::HObject();
			*findCreateXldObj_ = hoCross;

			redrawHalconView(false);
		}
		catch (...) {}
	}
}

void ToolNinePointWindow::addNewItemToListView(double pixX, double pixY, double realX, double realY)
{
	if (!ui || !ui->tabWidget_ninePointData) return;

	auto& t = ui->tabWidget_ninePointData;
	const int row = t->rowCount();
	t->insertRow(row);

	auto mkNumItem = [](double v) -> QTableWidgetItem*
	{
		auto* it = new QTableWidgetItem(QString::number(v, 'f', 2));
		it->setFlags((it->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
		it->setTextAlignment(Qt::AlignCenter);
		return it;
	};

	auto* idxItem = new QTableWidgetItem(QString::number(listViewCount_));
	idxItem->setFlags((idxItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
	idxItem->setCheckState(Qt::Checked);
	idxItem->setTextAlignment(Qt::AlignCenter);
	t->setItem(row, 0, idxItem);

	t->setItem(row, 1, mkNumItem(pixX));
	t->setItem(row, 2, mkNumItem(pixY));
	t->setItem(row, 3, mkNumItem(realX));
	t->setItem(row, 4, mkNumItem(realY));

	listViewCount_ = t->rowCount();
}

void ToolNinePointWindow::clearNinePointImages()
{
	listViewCount_ = 0;
	if (ui->tabWidget_ninePointData)
		ui->tabWidget_ninePointData->setRowCount(0);
}

// ===================================================================
// 缩放/拖拽
// ===================================================================
bool ToolNinePointWindow::ensureHalconViewPart()
{
	if (!halconLastImage_.IsInitialized()) return false;

	try
	{
		HalconCpp::HTuple w, h;
		HalconCpp::GetImageSize(halconLastImage_, &w, &h);
		const int imgW = w.I();
		const int imgH = h.I();
		if (imgW <= 0 || imgH <= 0) return false;

		if (!viewPartValid_ || imgW != viewImgW_ || imgH != viewImgH_)
		{
			viewImgW_ = imgW;
			viewImgH_ = imgH;
			resetHalconViewPartToFullImage();
		}
		return true;
	}
	catch (...) { return false; }
}

void ToolNinePointWindow::resetHalconViewPartToFullImage()
{
	viewPart_.r1 = 0.0;
	viewPart_.c1 = 0.0;
	viewPart_.r2 = std::max(0, viewImgH_ - 1);
	viewPart_.c2 = std::max(0, viewImgW_ - 1);
	viewPartValid_ = true;
}

void ToolNinePointWindow::zoomHalconViewAt(const QPoint& hostPos, int steps)
{
	if (steps == 0 || !ensureHalconViewPart()) return;

	const int hostW = std::max(1, hostDisplay_ ? hostDisplay_->width() : 1);
	const int hostH = std::max(1, hostDisplay_ ? hostDisplay_->height() : 1);

	const double spanC = viewPart_.c2 - viewPart_.c1;
	const double spanR = viewPart_.r2 - viewPart_.r1;

	const double rx = (hostW > 1) ? static_cast<double>(hostPos.x()) / (hostW - 1) : 0.5;
	const double ry = (hostH > 1) ? static_cast<double>(hostPos.y()) / (hostH - 1) : 0.5;

	const double col = viewPart_.c1 + rx * spanC;
	const double row = viewPart_.r1 + ry * spanR;

	const double base = 1.2;
	const double scale = std::pow(base, -steps);

	double newSpanC = spanC * scale;
	double newSpanR = spanR * scale;

	const double eps = 1e-6;
	if (std::abs(newSpanC) < eps) newSpanC = (newSpanC >= 0.0) ? eps : -eps;
	if (std::abs(newSpanR) < eps) newSpanR = (newSpanR >= 0.0) ? eps : -eps;

	const double fullSpanC = std::max(0, viewImgW_ - 1);
	const double fullSpanR = std::max(0, viewImgH_ - 1);
	if (newSpanC >= fullSpanC || newSpanR >= fullSpanR)
	{
		resetHalconViewPartToFullImage();
		return;
	}

	viewPart_.c1 = col - rx * newSpanC;
	viewPart_.r1 = row - ry * newSpanR;
	const double maxC1 = fullSpanC - newSpanC;
	const double maxR1 = fullSpanR - newSpanR;
	viewPart_.c1 = std::clamp(viewPart_.c1, 0.0, std::max(0.0, maxC1));
	viewPart_.r1 = std::clamp(viewPart_.r1, 0.0, std::max(0.0, maxR1));
	viewPart_.c2 = viewPart_.c1 + newSpanC;
	viewPart_.r2 = viewPart_.r1 + newSpanR;
	viewPartValid_ = true;
}

void ToolNinePointWindow::panHalconViewFromDrag(const QPoint& dragDelta)
{
	if (!ensureHalconViewPart()) return;

	const int hostW = std::max(1, hostDisplay_ ? hostDisplay_->width() : 1);
	const int hostH = std::max(1, hostDisplay_ ? hostDisplay_->height() : 1);

	const double spanC = panStartPart_.c2 - panStartPart_.c1;
	const double spanR = panStartPart_.r2 - panStartPart_.r1;

	const double dx = static_cast<double>(dragDelta.x());
	const double dy = static_cast<double>(dragDelta.y());

	const double dCol = (hostW > 1) ? (-(dx / (hostW - 1)) * spanC) : 0.0;
	const double dRow = (hostH > 1) ? (-(dy / (hostH - 1)) * spanR) : 0.0;

	viewPart_ = panStartPart_;
	viewPart_.c1 += dCol; viewPart_.c2 += dCol;
	viewPart_.r1 += dRow; viewPart_.r2 += dRow;

	const double fullSpanC = std::max(0, viewImgW_ - 1);
	const double fullSpanR = std::max(0, viewImgH_ - 1);
	const double curSpanC = viewPart_.c2 - viewPart_.c1;
	const double curSpanR = viewPart_.r2 - viewPart_.r1;
	if (curSpanC > 1e-9 && curSpanR > 1e-9)
	{
		const double maxC1 = fullSpanC - curSpanC;
		const double maxR1 = fullSpanR - curSpanR;
		viewPart_.c1 = std::clamp(viewPart_.c1, 0.0, std::max(0.0, maxC1));
		viewPart_.r1 = std::clamp(viewPart_.r1, 0.0, std::max(0.0, maxR1));
		viewPart_.c2 = viewPart_.c1 + curSpanC;
		viewPart_.r2 = viewPart_.r1 + curSpanR;
	}
	viewPartValid_ = true;
}
