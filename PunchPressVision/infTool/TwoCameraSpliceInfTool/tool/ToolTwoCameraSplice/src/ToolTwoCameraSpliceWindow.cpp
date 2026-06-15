#include "ToolTwoCameraSpliceWindow.hpp"
#include "ui_ToolTwoCameraSpliceWindow.h"

#include "infTool/TwoCameraSpliceInfTool/TwoCameraSpliceInfTool.hpp"
#include "infTool/CalibInfTool/CalibInfTool.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"
#include "infrastructure/CalibConfigModule/CalibConfigModulePath.hpp"
#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"
#include "rwul/hoecm/hoec_m.hpp"

#include <QCloseEvent>
#include <QEvent>
#include <QInputDialog>
#include <QStatusBar>

#include <filesystem>
#include <QLabel>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>

#include <opencv2/imgcodecs.hpp>

// ===================================================================
// cv::Mat → Halcon HImage 转换
// ===================================================================
HalconCpp::HImage ToolTwoCameraSpliceWindow::cvMatToHImage(const cv::Mat& mat)
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
ToolTwoCameraSpliceWindow::ToolTwoCameraSpliceWindow(inf::infrastructure& inf, QWidget* parent)
	: QMainWindow(parent)
	, inf_(inf)
	, calibTool_(std::make_unique<infTool::CalibInfTool>(inf_))
	, spliceTool_(std::make_unique<infTool::TwoCameraSpliceInfTool>(inf_, *calibTool_))
	, ui(new Ui::ToolTwoCameraSpliceWindowClass())
{
	ui->setupUi(this);

	calibTool_->build();
	spliceTool_->build();

	buildUi();
	buildConnections();
	initCaltabDescrPath();
	syncConfigToUi();
}

ToolTwoCameraSpliceWindow::~ToolTwoCameraSpliceWindow()
{
	spliceTool_->destroy();
	calibTool_->destroy();
	delete ui;
}

// ===================================================================
// 将 .ui 中的 QLabel 占位符替换为 native QWidget（承载 Halcon 显示）
// ===================================================================
QWidget* ToolTwoCameraSpliceWindow::replaceLabel(const QString& labelName)
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

	// 关键：确保 Halcon OpenWindow 能拿到有效的 native 句柄
	host->setAttribute(Qt::WA_NativeWindow, true);

	// 宿主 resize 时立即同步 Halcon 窗体
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

void ToolTwoCameraSpliceWindow::buildUi()
{
	hostCam1_     = replaceLabel(QStringLiteral("label_imgDisplay1"));
	hostCam2_     = replaceLabel(QStringLiteral("label_imgDisplay2"));
	hostStitched_ = replaceLabel(QStringLiteral("label_imgDisplay"));
}

// ===================================================================
void ToolTwoCameraSpliceWindow::buildConnections()
{
	// 相机帧信号 → DirectConnection 在采集线程中缓存
	if (inf_.camera_module_)
	{
		connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
			this, &ToolTwoCameraSpliceWindow::onCameraFrame, Qt::DirectConnection);
	}

	// 显示信号排队到 UI 线程
	connect(this, &ToolTwoCameraSpliceWindow::displayFrameReady,
		this, &ToolTwoCameraSpliceWindow::onDisplayFrame, Qt::QueuedConnection);

	// UI 按钮
	connect(ui->btn_takePicture, &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_takePicture_clicked);
	connect(ui->btn_zengyi1,    &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_zengyi1_clicked);
	connect(ui->btn_baoguang1,  &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_baoguang1_clicked);
	connect(ui->btn_zengyi2,    &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_zengyi2_clicked);
	connect(ui->btn_baoguang2,  &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_baoguang2_clicked);

	connect(ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi, &QPushButton::clicked,
		this, &ToolTwoCameraSpliceWindow::btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked);
	connect(ui->btn_caiqiebili, &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_caiqiebili_clicked);
	connect(ui->btn_buchang1,   &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_buchang1_clicked);
	connect(ui->btn_buchang2,   &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_buchang2_clicked);

	connect(ui->btn_test, &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_test_clicked);
	connect(ui->btn_exit, &QPushButton::clicked, this, &ToolTwoCameraSpliceWindow::btn_exit_clicked);
}

// ===================================================================
// 自动发现标定板描述文件（仿照 ToolCalibDistortionWindow::initBoardDescrPath）
// ===================================================================
void ToolTwoCameraSpliceWindow::initCaltabDescrPath()
{
	namespace fs = std::filesystem;

	const std::string configDir = inf::CalibConfigModulePath.RootPath;
	std::string foundPath;
	std::error_code ec;

	if (fs::exists(configDir, ec) && fs::is_directory(configDir, ec))
	{
		for (const auto& entry : fs::directory_iterator(configDir, ec))
		{
			if (entry.is_regular_file(ec)
				&& entry.path().extension() == ".descr")
			{
				foundPath = entry.path().string();
				break;
			}
		}
	}

	if (inf_.two_camera_splice_module_)
	{
		auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

		if (!foundPath.empty())
		{
			spliceCfg.caltabDescrPath = foundPath;
			statusBar()->showMessage(
				QStringLiteral("标定板描述文件: %1").arg(QString::fromStdString(foundPath)));
		}
		else if (!spliceCfg.caltabDescrPath.empty())
		{
			statusBar()->showMessage(
				QStringLiteral("标定板描述文件(已缓存): %1")
					.arg(QString::fromStdString(spliceCfg.caltabDescrPath)));
		}
		else
		{
			statusBar()->showMessage(
				QStringLiteral("config 目录下未找到 .descr 描述文件"));
		}
	}
}

// ===================================================================
// 从 TwoCameraSpliceCfg 同步到 UI
// ===================================================================
void ToolTwoCameraSpliceWindow::syncConfigToUi()
{
	if (!inf_.two_camera_splice_module_)
		return;

	const auto& cfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

	ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->setText(
		QString::number(cfg.DiffHeight * 1000.0, 'f', 2));
	ui->btn_caiqiebili->setText(
		QString::number(cfg.OverlapInPercent, 'f', 1));
}

// ===================================================================
// Halcon 窗口管理（UI 线程调用）
// ===================================================================
bool ToolTwoCameraSpliceWindow::ensureHalconWindow(DisplayWindow win)
{
	using namespace HalconCpp;

	QWidget* host = nullptr;
	HalconWin* hw = nullptr;

	switch (win)
	{
	case DisplayWindow::Cam1:     host = hostCam1_;     hw = &winCam1_;     break;
	case DisplayWindow::Cam2:     host = hostCam2_;     hw = &winCam2_;     break;
	case DisplayWindow::Stitched: host = hostStitched_; hw = &winStitched_; break;
	}

	if (!host)
		return false;

	const int w = host->width();
	const int h = host->height();
	if (w <= 0 || h <= 0)
		return false;

	if (hw->hwnd.Length() == 0)
	{
		const Hlong winId = static_cast<Hlong>(host->winId());
		OpenWindow(0, 0, w, h, winId, "visible", "", &hw->hwnd);
		SetWindowAttr("background_color", "gray");
		hw->hostSize = QSize(w, h);
	}
	else if (hw->hostSize.width() != w || hw->hostSize.height() != h)
	{
		SetWindowExtents(hw->hwnd, 0, 0, w, h);
		hw->hostSize = QSize(w, h);
		// resize 后重绘
		if (hw->lastImage.IsInitialized())
		{
			HTuple iw, ih;
			hw->lastImage.GetImageSize(&iw, &ih);
			SetPart(hw->hwnd, 0, 0, ih[0].I() - 1, iw[0].I() - 1);
			ClearWindow(hw->hwnd);
			DispObj(hw->lastImage, hw->hwnd);
		}
	}

	return true;
}

void ToolTwoCameraSpliceWindow::closeHalconWindows()
{
	using namespace HalconCpp;
	if (winCam1_.hwnd.Length() > 0)     { CloseWindow(winCam1_.hwnd);     winCam1_.hwnd.Clear(); }
	if (winCam2_.hwnd.Length() > 0)     { CloseWindow(winCam2_.hwnd);     winCam2_.hwnd.Clear(); }
	if (winStitched_.hwnd.Length() > 0) { CloseWindow(winStitched_.hwnd); winStitched_.hwnd.Clear(); }
}

void ToolTwoCameraSpliceWindow::redrawDisplay(DisplayWindow win)
{
	using namespace HalconCpp;

	HalconWin* hw = nullptr;
	switch (win)
	{
	case DisplayWindow::Cam1:     hw = &winCam1_;     break;
	case DisplayWindow::Cam2:     hw = &winCam2_;     break;
	case DisplayWindow::Stitched: hw = &winStitched_; break;
	}

	if (!hw || hw->hwnd.Length() == 0)
		return;

	try
	{
		if (hw->lastImage.IsInitialized())
		{
			HTuple iw, ih;
			hw->lastImage.GetImageSize(&iw, &ih);
			SetPart(hw->hwnd, 0, 0, ih[0].I() - 1, iw[0].I() - 1);
			ClearWindow(hw->hwnd);
			DispObj(hw->lastImage, hw->hwnd);
		}
	}
	catch (const HException&) {}
}

// ===================================================================
// 事件
// ===================================================================
bool ToolTwoCameraSpliceWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::Resize)
	{
		DisplayWindow win;
		if (watched == hostCam1_)          win = DisplayWindow::Cam1;
		else if (watched == hostCam2_)     win = DisplayWindow::Cam2;
		else if (watched == hostStitched_) win = DisplayWindow::Stitched;
		else return QMainWindow::eventFilter(watched, event);

		if (ensureHalconWindow(win))
			redrawDisplay(win);
	}
	return QMainWindow::eventFilter(watched, event);
}

void ToolTwoCameraSpliceWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
	ensureHalconWindow(DisplayWindow::Cam1);
	ensureHalconWindow(DisplayWindow::Cam2);
	ensureHalconWindow(DisplayWindow::Stitched);

	if (inf_.camera_module_)
		inf_.camera_module_->startMonitor();
}

void ToolTwoCameraSpliceWindow::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);
	// host resize 由 eventFilter 驱动，此处无需额外处理
}

void ToolTwoCameraSpliceWindow::closeEvent(QCloseEvent* event)
{
	if (inf_.camera_module_)
		inf_.camera_module_->stopMonitor();
	closeHalconWindows();
	QMainWindow::closeEvent(event);
}

// ===================================================================
// 相机帧回调（DirectConnection，采集线程）
// ===================================================================
void ToolTwoCameraSpliceWindow::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
{
	// 仅在采集线程中缓存图像，Halcon 操作留给 UI 线程
	switch (cameraIndex)
	{
	case global::CameraIndex::Camera1:
		rawMat1_   = matInfo.mat.clone();
		cam1Image_ = cvMatToHImage(matInfo.mat);
		cam1Ready_ = true;
		winCam1_.lastImage = cam1Image_;
		break;

	case global::CameraIndex::Camera2:
		rawMat2_   = matInfo.mat.clone();
		cam2Image_ = cvMatToHImage(matInfo.mat);
		cam2Ready_ = true;
		winCam2_.lastImage = cam2Image_;
		break;
	}

	// 排队到 UI 线程执行 Halcon 绘制
	emit displayFrameReady();
}

// ===================================================================
// UI 线程刷新显示（QueuedConnection）
// ===================================================================
void ToolTwoCameraSpliceWindow::onDisplayFrame()
{
	if (cam1Ready_)
	{
		if (ensureHalconWindow(DisplayWindow::Cam1))
			redrawDisplay(DisplayWindow::Cam1);
	}
	if (cam2Ready_)
	{
		if (ensureHalconWindow(DisplayWindow::Cam2))
			redrawDisplay(DisplayWindow::Cam2);
	}
}

// ===================================================================
// 拍照
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_takePicture_clicked()
{
	if (!inf_.camera_module_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("相机模块未初始化"));
		return;
	}

	rw::hoec::MatInfo frame1, frame2;
	const bool ok1 = inf_.camera_module_->captureSingleFrame(global::CameraIndex::Camera1, frame1);
	const bool ok2 = inf_.camera_module_->captureSingleFrame(global::CameraIndex::Camera2, frame2);

	if (ok1)
	{
		rawMat1_   = frame1.mat.clone();
		cam1Image_ = cvMatToHImage(frame1.mat);
		cam1Ready_ = true;
		winCam1_.lastImage = cam1Image_;
	}

	if (ok2)
	{
		rawMat2_   = frame2.mat.clone();
		cam2Image_ = cvMatToHImage(frame2.mat);
		cam2Ready_ = true;
		winCam2_.lastImage = cam2Image_;
	}

	

}

// ===================================================================
// 相机1/2 增益 & 曝光
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_zengyi1_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机1增益"),
		QStringLiteral("增益值:"), ui->btn_zengyi1->text().toDouble(&ok), 0, 100, 2, &ok);
	if (!ok) return;

	ui->btn_zengyi1->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setGain(global::CameraIndex::Camera1, v);
}

void ToolTwoCameraSpliceWindow::btn_baoguang1_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机1曝光"),
		QStringLiteral("曝光值 (us):"), ui->btn_baoguang1->text().toDouble(&ok), 0, 10000000, 0, &ok);
	if (!ok) return;

	ui->btn_baoguang1->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setExposure(global::CameraIndex::Camera1, v);
}

void ToolTwoCameraSpliceWindow::btn_zengyi2_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机2增益"),
		QStringLiteral("增益值:"), ui->btn_zengyi2->text().toDouble(&ok), 0, 100, 2, &ok);
	if (!ok) return;

	ui->btn_zengyi2->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setGain(global::CameraIndex::Camera2, v);
}

void ToolTwoCameraSpliceWindow::btn_baoguang2_clicked()
{
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("相机2曝光"),
		QStringLiteral("曝光值 (us):"), ui->btn_baoguang2->text().toDouble(&ok), 0, 10000000, 0, &ok);
	if (!ok) return;

	ui->btn_baoguang2->setText(QString::number(v));
	if (inf_.camera_module_)
		inf_.camera_module_->setExposure(global::CameraIndex::Camera2, v);
}

// ===================================================================
// 高度差值
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked()
{
	if (!inf_.two_camera_splice_module_)
		return;

	auto& cfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("高度差值"),
		QStringLiteral("相机物料与标定板高度差 (mm):"),
		cfg.DiffHeight * 1000.0, -1000, 1000, 3, &ok);

	if (ok)
	{
		cfg.DiffHeight = v / 1000.0;
		ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->setText(QString::number(v, 'f', 3));
		inf_.two_camera_splice_module_->save();
	}
}

// ===================================================================
// 裁切比例
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_caiqiebili_clicked()
{
	if (!inf_.two_camera_splice_module_)
		return;

	auto& cfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;
	bool ok = false;
	const double v = QInputDialog::getDouble(this, QStringLiteral("裁切比例"),
		QStringLiteral("裁切比例 (%):"),
		cfg.OverlapInPercent, 0, 100, 1, &ok);

	if (ok)
	{
		cfg.OverlapInPercent = v;
		ui->btn_caiqiebili->setText(QString::number(v, 'f', 1));
		inf_.two_camera_splice_module_->save();
	}
}

// ===================================================================
// 补偿 / 用时 / 拼接过程
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_buchang1_clicked()
{
	QMessageBox::information(this, QStringLiteral("补偿1"), QStringLiteral("补偿功能暂不开放，请重新标定"));
}

void ToolTwoCameraSpliceWindow::btn_buchang2_clicked()
{
	QMessageBox::information(this, QStringLiteral("补偿2"), QStringLiteral("补偿功能暂不开放，请重新标定"));
}

void ToolTwoCameraSpliceWindow::btn_yongshi_clicked()     { /* 只读 */ }
void ToolTwoCameraSpliceWindow::btn_pinjieguocheng_clicked() { /* 只读 */ }

// ===================================================================
// 料号管理
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_xinjianpinjieliaohao_clicked()
{
	QMessageBox::information(this, QStringLiteral("新建拼接料号"), QStringLiteral("料号管理功能待实现"));
}

void ToolTwoCameraSpliceWindow::btn_liaohaomingcheng_clicked()             { /* 只读显示 */ }
void ToolTwoCameraSpliceWindow::btn_xiugaimingcheng_clicked()              { /* TODO */ }
void ToolTwoCameraSpliceWindow::btn_delete_clicked()                       { /* TODO */ }
void ToolTwoCameraSpliceWindow::btn_deleteAll_clicked()                    { /* TODO */ }
void ToolTwoCameraSpliceWindow::btn_dangqianxuanzedepinjieliaohao_clicked(){ /* 只读显示 */ }

// ===================================================================
// 测试：执行拼接流程
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_test_clicked()
{
	if (!inf_.two_camera_splice_module_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("拼接模块未初始化"));
		return;
	}
	if (!inf_.calib_config_module_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("标定配置模块未初始化"));
		return;
	}

	auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;
	auto& calibCfg  = inf_.calib_config_module_->calibConfig;

	// DiffHeight（UI mm → 内部 m）
	spliceCfg.DiffHeight = ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->text().toDouble() / 1000.0;
	spliceCfg.OverlapInPercent = ui->btn_caiqiebili->text().toDouble();

	const auto& item1 = calibCfg.item(global::CameraIndex::Camera1);
	const auto& item2 = calibCfg.item(global::CameraIndex::Camera2);

	if (spliceCfg.caltabDescrPath.empty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("标定板描述文件路径为空，请在 config 目录下放置 .descr 文件"));
		return;
	}

	if (!cam1Ready_ || !cam2Ready_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("相机图像未就绪，请先拍照"));
		return;
	}

	// 执行标定拼接
	std::string errorMsg;
	HalconCpp::HObject ho1 = static_cast<HalconCpp::HObject>(cam1Image_);
	HalconCpp::HObject ho2 = static_cast<HalconCpp::HObject>(cam2Image_);

	const bool ok = spliceTool_->calibImage(ho1, ho2, item1, item2, spliceCfg, &errorMsg);
	if (!ok)
	{
		QMessageBox::warning(this, QStringLiteral("标定失败"), QString::fromStdString(errorMsg));
		return;
	}

	HalconCpp::HObject stitched;
	if (!spliceTool_->pinjieImage(ho1, ho2, spliceCfg, stitched))
	{
		QMessageBox::warning(this, QStringLiteral("拼接失败"), QStringLiteral("图像拼接失败"));
		return;
	}

	// 显示
	HalconCpp::HImage stitchedHImage(stitched);
	winStitched_.lastImage = stitchedHImage;
	if (ensureHalconWindow(DisplayWindow::Stitched))
		redrawDisplay(DisplayWindow::Stitched);

	inf_.two_camera_splice_module_->save();

	ui->lb_realPosition->setText(
		QStringLiteral("拼接完成 | 矫正尺寸: %1 × %2 px")
			.arg(spliceCfg.rectifiedWidth)
			.arg(spliceCfg.rectifiedHeight));
}

// ===================================================================
void ToolTwoCameraSpliceWindow::btn_exit_clicked()
{
	close();
}
