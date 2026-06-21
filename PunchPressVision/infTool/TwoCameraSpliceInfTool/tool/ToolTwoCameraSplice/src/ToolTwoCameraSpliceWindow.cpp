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
#include <QDateTime>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QShowEvent>
#include <QStatusBar>

#include <filesystem>

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

	// HalconInteractiveLabel 由 .ui 提升后自动创建，无需 buildUi 替换
	buildConnections();
	initCaltabDescrPath();
	syncConfigToUi();
	applyCalibParams();
}

ToolTwoCameraSpliceWindow::~ToolTwoCameraSpliceWindow()
{
	spliceTool_->destroy();
	calibTool_->destroy();
	delete ui;
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
// 从 TwoCameraSpliceCfg 加载曝光/增益到 UI 并生效到相机
// ===================================================================
void ToolTwoCameraSpliceWindow::applyCalibParams()
{
	if (!inf_.two_camera_splice_module_)
		return;

	const auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

	// 同步到 UI
	ui->btn_zengyi1->setText(QString::number(spliceCfg.camera1Gain, 'f', 0));
	ui->btn_baoguang1->setText(QString::number(spliceCfg.camera1Exposure, 'f', 0));
	ui->btn_zengyi2->setText(QString::number(spliceCfg.camera2Gain, 'f', 0));
	ui->btn_baoguang2->setText(QString::number(spliceCfg.camera2Exposure, 'f', 0));

	// 生效到相机
	if (inf_.camera_module_)
	{
		inf_.camera_module_->setGain(global::CameraIndex::Camera1, spliceCfg.camera1Gain);
		inf_.camera_module_->setExposure(global::CameraIndex::Camera1, spliceCfg.camera1Exposure);
		inf_.camera_module_->setGain(global::CameraIndex::Camera2, spliceCfg.camera2Gain);
		inf_.camera_module_->setExposure(global::CameraIndex::Camera2, spliceCfg.camera2Exposure);
	}
}

// ===================================================================
// 事件 — HalconInteractiveLabel 自管理窗口生命周期与 resize
// ===================================================================
void ToolTwoCameraSpliceWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
	// HalconInteractiveLabel 在 showEvent 中懒创建 Halcon 窗口

	if (inf_.camera_module_)
		inf_.camera_module_->startMonitor();
}

void ToolTwoCameraSpliceWindow::closeEvent(QCloseEvent* event)
{
	if (inf_.camera_module_)
		inf_.camera_module_->stopMonitor();
	// HalconInteractiveLabel 在析构时自动关闭 Halcon 窗口
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
		break;

	case global::CameraIndex::Camera2:
		rawMat2_   = matInfo.mat.clone();
		cam2Image_ = cvMatToHImage(matInfo.mat);
		cam2Ready_ = true;
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
		ui->label_imgDisplay1->displayImage(cam1Image_);
	if (cam2Ready_)
		ui->label_imgDisplay2->displayImage(cam2Image_);
}

// ===================================================================
// 拍照
// ===================================================================
void ToolTwoCameraSpliceWindow::btn_takePicture_clicked()
{
	// 直接从相机回调缓存中取当前帧，存入 TwoCameraSpliceCfg 供后续标定使用
	if (!cam1Ready_ && !cam2Ready_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("相机图像未就绪，请确认相机正在采集"));
		return;
	}

	if (!inf_.two_camera_splice_module_)
		return;

	auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

	if (cam1Ready_)
		spliceCfg.camera1Piccture = static_cast<HalconCpp::HObject>(cam1Image_);
	if (cam2Ready_)
		spliceCfg.camera2Piccture = static_cast<HalconCpp::HObject>(cam2Image_);

	statusBar()->showMessage(
		cam1Ready_ && cam2Ready_ ? QStringLiteral("拍照完成")
								 : QStringLiteral("拍照完成（部分相机未就绪）"));
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
	if (inf_.two_camera_splice_module_)
	{
		inf_.two_camera_splice_module_->twoCameraSpliceConfig.camera1Gain = v;
		inf_.two_camera_splice_module_->save();
	}
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
	if (inf_.two_camera_splice_module_)
	{
		inf_.two_camera_splice_module_->twoCameraSpliceConfig.camera1Exposure = v;
		inf_.two_camera_splice_module_->save();
	}
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
	if (inf_.two_camera_splice_module_)
	{
		inf_.two_camera_splice_module_->twoCameraSpliceConfig.camera2Gain = v;
		inf_.two_camera_splice_module_->save();
	}
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
	if (inf_.two_camera_splice_module_)
	{
		inf_.two_camera_splice_module_->twoCameraSpliceConfig.camera2Exposure = v;
		inf_.two_camera_splice_module_->save();
	}
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

	// DiffHeight / OverlapInPercent 在 btn_*_clicked 中已同步到 config，
	// 保存最新值确保拼接使用
	inf_.two_camera_splice_module_->save();

	const auto& item1 = calibCfg.item(global::CameraIndex::Camera1);
	const auto& item2 = calibCfg.item(global::CameraIndex::Camera2);

	if (spliceCfg.caltabDescrPath.empty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("标定板描述文件路径为空，请在 config 目录下放置 .descr 文件"));
		return;
	}

	// 使用 TwoCameraSpliceCfg 中缓存的拍照图像
	if (!spliceCfg.camera1Piccture.IsInitialized() || !spliceCfg.camera2Piccture.IsInitialized())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("相机图像未就绪，请先拍照"));
		return;
	}

	// 从相机1标定参数计算像素当量（若 config 中为 0）
	// pixTowWorld = PixelSize × |tz| / Focus，单位 m/px
	if (spliceCfg.pixTowWorld == 0.0)
	{
		const auto& cp   = item1.cameraParameters;
		const auto& pose = item1.cameraPose;
		if (cp.TupleLength() >= 13 && pose.TupleLength() >= 7)
		{
			constexpr double kPixelSizeM = 2.4e-06;  // 2.4 μm → m
			const double focus = cp[1].D();            // 焦距 (m)
			const double tz    = std::abs(pose[2].D());// 工作距离 (m)
			if (focus > 0.0)
			{
				spliceCfg.pixTowWorld = kPixelSizeM * tz / focus;
				inf_.two_camera_splice_module_->save();
			}
		}
	}

	if (spliceCfg.pixTowWorld == 0.0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("像素当量(pixTowWorld)为 0，请先完成相机标定"));
		return;
	}

	// 标定：使用 TwoCameraSpliceCfg 中存储的拍照图像
	std::string errorMsg;
	HalconCpp::HObject ho1 = spliceCfg.camera1Piccture;
	HalconCpp::HObject ho2 = spliceCfg.camera2Piccture;

	const bool ok = spliceTool_->calibImage(ho1, ho2, item1, item2, spliceCfg, &errorMsg);
	if (!ok)
	{
		QMessageBox::warning(this, QStringLiteral("标定失败"), QString::fromStdString(errorMsg));
		return;
	}

	// 拼接：使用当前实时显示的图像
	if (!cam1Ready_ || !cam2Ready_)
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("实时图像未就绪，请确认相机正在采集"));
		return;
	}
	HalconCpp::HObject live1 = static_cast<HalconCpp::HObject>(cam1Image_);
	HalconCpp::HObject live2 = static_cast<HalconCpp::HObject>(cam2Image_);

	HalconCpp::HObject stitched;
	if (!spliceTool_->pinjieImage(live1, live2, spliceCfg, stitched))
	{
		QMessageBox::warning(this, QStringLiteral("拼接失败"), QStringLiteral("图像拼接失败"));
		return;
	}

	// 显示拼接结果（HalconInteractiveLabel 自动处理缩放/平移）
	HalconCpp::HImage stitchedHImage(stitched);
	ui->label_imgDisplay->displayImage(stitchedHImage);

	ui->lb_realPosition->setText(
		QStringLiteral("拼接完成 | 矫正尺寸: %1 × %2 px")
			.arg(spliceCfg.rectifiedWidth)
			.arg(spliceCfg.rectifiedHeight));
}

// ===================================================================
void ToolTwoCameraSpliceWindow::btn_exit_clicked()
{
	// 若标定已成功（MapSingle1/MapSingle2 已生成），提示是否保存
	if (inf_.two_camera_splice_module_)
	{
		auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;
		if (spliceCfg.MapSingle1.IsInitialized() && spliceCfg.MapSingle2.IsInitialized())
		{
			const auto btn = QMessageBox::question(
				this,
				QStringLiteral("保存标定结果"),
				QStringLiteral("标定已成功，是否保存拼接配置？\n保存路径: TwoCameraSpliceModule"),
				QMessageBox::Yes | QMessageBox::No,
				QMessageBox::Yes);
			if (btn == QMessageBox::Yes)
			{
				inf_.two_camera_splice_module_->save();
				statusBar()->showMessage(QStringLiteral("拼接配置已保存"), 3000);
			}
		}
	}
	close();
}
