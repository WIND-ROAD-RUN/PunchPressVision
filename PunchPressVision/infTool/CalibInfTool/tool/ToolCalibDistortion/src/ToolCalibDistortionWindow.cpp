#include "ToolCalibDistortionWindow.hpp"
#include "ui_ToolCalibDistortionWindow.h"

#include "CornerPreviewDialog.hpp"
#include "infTool/CalibInfTool/CalibInfTool.hpp"
#include "infrastructure/CalibConfigModule/CalibConfigModulePath.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QResizeEvent>
#include <QSpinBox>
#include <QStatusBar>

#include <filesystem>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>

// ---------------------------------------------------------------------------
// cv::Mat → Halcon HImage 转换（零拷贝，调用方须保证 mat 使用期间有效）
// ---------------------------------------------------------------------------
static HalconCpp::HImage cvMatToHImage(const cv::Mat& mat)
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
ToolCalibDistortionWindow::ToolCalibDistortionWindow(inf::infrastructure& inf, QWidget* parent)
    : QMainWindow(parent)
    , inf_(inf)
    , calibInfTool_(std::make_unique<infTool::CalibInfTool>(inf_))
    , ui(new Ui::ToolCalibDistortionWindowClass())
{
    ui->setupUi(this);

    // 初始化 Halcon 标定引擎
    calibInfTool_->build();

    buildConnections();
    initBoardDescrPath();
    cleanCalibImageDirs();
    updateConnectionStatus();
    applyDefaultCameraParams();
}

ToolCalibDistortionWindow::~ToolCalibDistortionWindow()
{
    calibInfTool_->destroy();
    delete ui;
}

// ===================================================================
// UI 连接
// ===================================================================
void ToolCalibDistortionWindow::buildConnections()
{
    connect(ui->startStopBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onStartStop);
    connect(ui->cameraSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ToolCalibDistortionWindow::onCameraSelected);

    // 标定流程
    connect(ui->saveFrameBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSaveCurrentFrame);
    connect(ui->calibrateBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onCalibrate);
    connect(ui->previewCornersBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onPreviewCorners);

    // 隐藏已废弃的按钮
    ui->saveParamsBtn->hide();
    ui->loadParamsBtn->hide();
    ui->loadCalibImagesBtn->hide();

    // 曝光 / 增益
    connect(ui->setExposureBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSetExposure);
    connect(ui->setGainBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSetGain);
    // 旋转次数（持久化到 cameraCfg，在 CameraModule 回调中实际执行旋转）
    connect(ui->setRotateBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSetRotate);

    // 相机回调使用 DirectConnection，在采集线程中处理图像并排队到 UI 线程显示
    if (inf_.camera_module_)
    {
        connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
            this, &ToolCalibDistortionWindow::onCameraFrame, Qt::DirectConnection);
        connect(inf_.camera_module_.get(), &inf::CameraModule::cameraConnectionStateChanged,
            this, &ToolCalibDistortionWindow::onConnectionChanged, Qt::QueuedConnection);
    }

    // 显示信号排队到 UI 线程
    connect(this, &ToolCalibDistortionWindow::originalFrameReady,
        this, &ToolCalibDistortionWindow::onOriginalDisplayFrame, Qt::QueuedConnection);
}

// ===================================================================
// 窗口事件
// ===================================================================
void ToolCalibDistortionWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // HalconInteractiveLabel 在 showEvent 中懒创建 Halcon 窗口，
    // 无需手动 ensure / resizeToHost
    refreshMarkedView();
}

void ToolCalibDistortionWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // HalconInteractiveLabel 内部处理图像重绘；
    // 右侧 XLD 叠加需手动恢复（Halcon ClearWindow 会擦除 XLD）
    if (lastMarksXld_.IsInitialized() && ui->undistortedView->isReady())
    {
        HalconCpp::SetColor(ui->undistortedView->halconHandle(), "green");
        HalconCpp::DispObj(lastMarksXld_, ui->undistortedView->halconHandle());
    }
}

void ToolCalibDistortionWindow::closeEvent(QCloseEvent* event)
{
    if (isRunning_.load())
    {
        if (inf_.camera_module_)
            inf_.camera_module_->stopMonitor();
        isRunning_.store(false);
    }
    event->accept();
}

// ===================================================================
// 相机帧回调（相机线程）
// ===================================================================
void ToolCalibDistortionWindow::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
{
    if (cameraIndex != selectedCamera_.load(std::memory_order_acquire))
        return;

    try
    {
        // 缓存当前原始帧（cv::Mat 用于保存文件）
        lastRawMat_ = matInfo.mat.clone();

        // 左侧：原始图像
        emit originalFrameReady(cvMatToHImage(matInfo.mat));
    }
    catch (...)
    {
        // 帧处理失败时静默丢弃，保持采集不中断
    }
}

// ===================================================================
// 显示槽（UI 线程）
// ===================================================================
void ToolCalibDistortionWindow::onOriginalDisplayFrame(HalconCpp::HImage image)
{
    ui->originalView->displayImage(image);
}

// ===================================================================
// 右侧 XLD 标记视图 — 仅在 saveFrame 后刷新，resize 时重绘
// ===================================================================
void ToolCalibDistortionWindow::refreshMarkedView()
{
    if (!lastMarkedImage_.IsInitialized())
        return;

    ui->undistortedView->displayImage(lastMarkedImage_);
    if (lastMarksXld_.IsInitialized())
    {
        HalconCpp::SetColor(ui->undistortedView->halconHandle(), "green");
        HalconCpp::DispObj(lastMarksXld_, ui->undistortedView->halconHandle());
    }
}

// ===================================================================
// 连接状态
// ===================================================================
void ToolCalibDistortionWindow::onConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
{
    (void)idx;
    (void)connected;
    (void)reason;
    updateConnectionStatus();
}

// ===================================================================
// 曝光 / 增益
// ===================================================================
void ToolCalibDistortionWindow::onSetExposure()
{
    if (!inf_.camera_module_)
        return;

    const int value = ui->exposureSpin->value();
    const global::CameraIndex idx = cameraIndexFromCombo(ui->cameraSelect->currentIndex());
    const bool ok = inf_.camera_module_->setExposure(idx, value);

    if (ok)
        statusBar()->showMessage(QStringLiteral("%1 曝光已设置为 %2 us")
            .arg(cameraDisplayName(idx)).arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"),
            QStringLiteral("无法设置 %1 曝光").arg(cameraDisplayName(idx)));
}

void ToolCalibDistortionWindow::onSetGain()
{
    if (!inf_.camera_module_)
        return;

    const int value = ui->gainSpin->value();
    const global::CameraIndex idx = cameraIndexFromCombo(ui->cameraSelect->currentIndex());
    const bool ok = inf_.camera_module_->setGain(idx, value);

    if (ok)
        statusBar()->showMessage(QStringLiteral("%1 增益已设置为 %2")
            .arg(cameraDisplayName(idx)).arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"),
            QStringLiteral("无法设置 %1 增益").arg(cameraDisplayName(idx)));
}

// ===================================================================
// 旋转次数（持久化到 cameraCfg，实际旋转在 CameraModule 回调中执行）
// ===================================================================
void ToolCalibDistortionWindow::onSetRotate()
{
    if (!inf_.config_module_)
        return;

    const int value = ui->rotateSpin->value();
    const global::CameraIndex idx = cameraIndexFromCombo(ui->cameraSelect->currentIndex());

    auto& cfg = inf_.config_module_->cameraCfg;
    if (idx == global::CameraIndex::Camera1)
        cfg.rotateCount1 = value;
    else
        cfg.rotateCount2 = value;

    // 同步到 CameraModule 的原子量（采集线程回调读取，线程安全）
    if (inf_.camera_module_)
    {
        if (idx == global::CameraIndex::Camera1)
            inf_.camera_module_->rotateCount1_.store(value, std::memory_order_relaxed);
        else
            inf_.camera_module_->rotateCount2_.store(value, std::memory_order_relaxed);
    }

    inf_.config_module_->save();

    statusBar()->showMessage(QStringLiteral("%1 旋转次数已设置为 %2 (顺时针%3°)")
        .arg(cameraDisplayName(idx)).arg(value).arg(value * 90));
}

// ===================================================================
// 采集控制
// ===================================================================
void ToolCalibDistortionWindow::onStartStop()
{
    if (!inf_.camera_module_)
        return;

    if (isRunning_.load(std::memory_order_acquire))
    {
       
        inf_.camera_module_->stopMonitor();
        isRunning_.store(false, std::memory_order_release);
        ui->startStopBtn->setText(QStringLiteral("开始采集"));
        statusBar()->showMessage(QStringLiteral("采集已停止"));
    }
    else
    {

        inf_.camera_module_->startMonitor();
        isRunning_.store(true, std::memory_order_release);
        ui->startStopBtn->setText(QStringLiteral("停止采集"));
        statusBar()->showMessage(QStringLiteral("采集中..."));
    }
}

void ToolCalibDistortionWindow::onCameraSelected(int index)
{
    const global::CameraIndex idx = cameraIndexFromCombo(index);
    selectedCamera_.store(idx, std::memory_order_release);

    // 切换相机时同步旋转次数 spinner
    if (inf_.config_module_)
    {
        const auto& cfg = inf_.config_module_->cameraCfg;
        const int rotateCount = (idx == global::CameraIndex::Camera1)
            ? cfg.rotateCount1 : cfg.rotateCount2;
        ui->rotateSpin->setValue(rotateCount);
    }

    statusBar()->showMessage(
        QStringLiteral("当前操作/显示已切换到 %1").arg(cameraDisplayName(idx)));
}

// ===================================================================
// 标定板描述文件 — 从 config 目录自动检测
// ===================================================================
void ToolCalibDistortionWindow::initBoardDescrPath()
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

    auto& cfg = inf_.calib_config_module_->calibConfig;

    if (!foundPath.empty())
    {
        // 为两个相机都设置标定板描述文件路径
        cfg.item(global::CameraIndex::Camera1).calibBoardDescrPath = foundPath;
        cfg.item(global::CameraIndex::Camera2).calibBoardDescrPath = foundPath;
        ui->calibBoardPathEdit->setText(QString::fromStdString(foundPath));
        statusBar()->showMessage(
            QStringLiteral("标定板描述文件: %1").arg(QString::fromStdString(foundPath)));
    }
    else if (!cfg.item(global::CameraIndex::Camera1).calibBoardDescrPath.empty())
    {
        // config 中未找到 .descr 但已有旧路径则沿用
        ui->calibBoardPathEdit->setText(
            QString::fromStdString(cfg.item(global::CameraIndex::Camera1).calibBoardDescrPath));
    }
    else
    {
        ui->calibBoardPathEdit->setText(QString());
        statusBar()->showMessage(
            QStringLiteral("config 目录下未找到 .descr 描述文件"));
    }
}

// ===================================================================
// 启动时自动清理 Camera1 / Camera2 目录下的旧标定图
// ===================================================================
void ToolCalibDistortionWindow::cleanCalibImageDirs()
{
    namespace fs = std::filesystem;
    const std::string baseDir = inf::CalibConfigModulePath.RootPath;

    auto cleanDir = [&](const std::string& subDir)
    {
        const std::string dirPath = global::joinPath(baseDir, subDir);
        std::error_code ec;
        if (fs::exists(dirPath, ec) && fs::is_directory(dirPath, ec))
        {
            for (const auto& entry : fs::directory_iterator(dirPath, ec))
            {
                if (entry.is_regular_file(ec))
                    fs::remove(entry.path(), ec);
            }
        }
        else
        {
            fs::create_directories(dirPath, ec);
        }
    };

    cleanDir("Camera1");
    cleanDir("Camera2");
}

// ===================================================================
// Halcon 标定流程
// ===================================================================
void ToolCalibDistortionWindow::onSaveCurrentFrame()
{
    if (lastRawMat_.empty())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("当前没有可保存的帧"));
        return;
    }

    // 根据当前选中相机确定保存子目录
    const global::CameraIndex camIdx = selectedCamera_.load(std::memory_order_acquire);
    const QString camName = (camIdx == global::CameraIndex::Camera1)
        ? QStringLiteral("Camera1") : QStringLiteral("Camera2");

    const std::string saveDir = global::joinPath(
        inf::CalibConfigModulePath.RootPath, camName.toStdString());

    std::error_code ec;
    std::filesystem::create_directories(saveDir, ec);

    // 先转为 HImage 并通过 drawCalibMarks 校验标定板是否可识别
    HalconCpp::HImage hImg = cvMatToHImage(lastRawMat_);
    if (!hImg.IsInitialized())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("图像转换失败"));
        return;
    }

    auto& item = inf_.calib_config_module_->calibConfig.item(camIdx);
    if (item.calibBoardDescrPath.empty())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("未设置标定板描述文件（.descr），无法校验图像"));
        return;
    }

    bool isOk = false;
    HalconCpp::HObject marksXld, marksRegion;
    std::string err;
    calibInfTool_->drawCalibMarks(hImg, item, isOk, marksXld, marksRegion, &err);

    if (!isOk)
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("当前帧未检测到完整标定板标记，不符合标定图要求，不予保存。\n\n详细信息：%1")
                .arg(QString::fromStdString(err)));
        return;
    }

    // 校验通过，保存到相机专属目录
    const QString filename = QStringLiteral("calib_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    const QString path = QDir(QString::fromStdString(saveDir)).filePath(filename);

    if (cv::imwrite(path.toStdString(), lastRawMat_))
    {
        capturedImages_.push_back(hImg);

        // 右侧：显示 XLD 标记
        lastMarkedImage_ = hImg;
        lastMarksXld_   = marksXld;

        refreshMarkedView();

        statusBar()->showMessage(
            QStringLiteral("已保存：%1（已加入标定图集，共 %2 张）")
                .arg(path).arg(capturedImages_.size()));
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("无法写入：%1").arg(path));
    }
}

void ToolCalibDistortionWindow::onCalibrate()
{
    const global::CameraIndex camIdx = selectedCamera_.load(std::memory_order_acquire);
    auto& item = inf_.calib_config_module_->calibConfig.item(camIdx);

    if (item.calibBoardDescrPath.empty())
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"),
            QStringLiteral("请先选择标定板描述文件（.descr）"));
        return;
    }

    if (capturedImages_.size() < 3)
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"),
            QStringLiteral("标定图像不足（至少需要 3 张，当前 %1 张）")
                .arg(capturedImages_.size()));
        return;
    }

    const double focalLength = ui->focalLengthSpin->value();
    const double plateThickness = ui->plateThicknessSpin->value();

    statusBar()->showMessage(QStringLiteral("正在 Halcon 标定..."));
    QApplication::processEvents();

    std::string err;
    const bool ok = calibInfTool_->calibrateFromImages(
        capturedImages_, focalLength, plateThickness, item, /*referenceIndex*/ 0, &err);

    if (!ok)
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"),
            QString::fromStdString(err));
        return;
    }

    // 标定成功 → 自动保存参数到 CalibConfigModulePath.RootPath
    const std::string configDir = inf::CalibConfigModulePath.RootPath;
    try
    {
        inf_.calib_config_module_->calibConfig.saveInDir(configDir);
        statusBar()->showMessage(
            QStringLiteral("标定参数已自动保存至：%1")
                .arg(QString::fromStdString(configDir)));
    }
    catch (...)
    {
        statusBar()->showMessage(
            QStringLiteral("标定完成但参数保存失败：%1")
                .arg(QString::fromStdString(configDir)));
    }

    // 构建详细标定信息
    const auto& cp = item.cameraParameters;  // area_scan_polynomial: 13 elements
    const auto& pose = item.cameraPose;      // [tx, ty, tz, rx, ry, rz, type]

    // RMS
    QString rmsStr = QStringLiteral("N/A");
    if (item.calibrationErrors.Length() > 0)
        rmsStr = QString::number(item.calibrationErrors[0].D(), 'f', 4);

    // 焦距
    QString focusStr = QStringLiteral("N/A");
    if (cp.Length() > 1)
        focusStr = QString::number(cp[1].D() * 1000.0, 'f', 3) + QStringLiteral(" mm");

    // 像素尺寸（传感器物理属性，固定值）
    constexpr double kPixelSizeM = 2.4e-06;  // 2.4 μm
    const QString sxStr = QStringLiteral("2.400 μm");
    const QString syStr = QStringLiteral("2.400 μm");

    // 主点
    QString cxStr = QStringLiteral("N/A"), cyStr = QStringLiteral("N/A");
    if (cp.Length() > 10)
    {
        cxStr = QString::number(cp[9].D(), 'f', 1) + QStringLiteral(" px");
        cyStr = QString::number(cp[10].D(), 'f', 1) + QStringLiteral(" px");
    }

    // 畸变系数 K1/K2/K3/P1/P2
    QString k1Str = QStringLiteral("N/A"), k2Str = QStringLiteral("N/A"), k3Str = QStringLiteral("N/A");
    QString p1Str = QStringLiteral("N/A"), p2Str = QStringLiteral("N/A");
    if (cp.Length() > 6)
    {
        k1Str = QString::number(cp[2].D(), 'e', 4);
        k2Str = QString::number(cp[3].D(), 'e', 4);
        k3Str = QString::number(cp[4].D(), 'e', 4);
        p1Str = QString::number(cp[5].D(), 'e', 4);
        p2Str = QString::number(cp[6].D(), 'e', 4);
    }

    // 工作距离（标定板到相机的 Z 方向距离）
    QString wdStr = QStringLiteral("N/A");
    if (pose.Length() >= 3)
        wdStr = QString::number(std::abs(pose[2].D()) * 1000.0, 'f', 1) + QStringLiteral(" mm");

    // 像素当量（在标定板工作平面处）: kPixelSizeM * tz / Focus
    QString pixelEquivStr = QStringLiteral("N/A");
    if (pose.Length() >= 3 && cp.Length() > 1 && cp[1].D() != 0.0)
    {
        const double pe = kPixelSizeM * std::abs(pose[2].D()) / cp[1].D() * 1000.0;  // mm/px
        pixelEquivStr = QString::number(pe, 'f', 5) + QStringLiteral(" mm/px");
    }

    const QString camDisplayName = cameraDisplayName(camIdx);
    const QString info = QStringLiteral(
        "%15 Halcon 标定完成\n\n"
        "━━━━ 相机内参 ━━━━\n"
        "  类型: area_scan_polynomial\n"
        "  焦距: %1\n"
        "  像素尺寸: Sx = %2, Sy = %3\n"
        "  主点: Cx = %4, Cy = %5\n"
        "  径向畸变 K1: %6\n"
        "  径向畸变 K2: %7\n"
        "  径向畸变 K3: %8\n"
        "  切向畸变 P1: %9\n"
        "  切向畸变 P2: %10\n\n"
        "━━━━ 相机外参（参考帧）━━━━\n"
        "  工作距离: %11\n"
        "  像素当量: %12\n\n"
        "━━━━ 标定精度 ━━━━\n"
        "  重投影误差 RMS: %13 px\n\n"
        "参数已自动保存至:\n%14")
        .arg(focusStr, sxStr, syStr, cxStr, cyStr,
             k1Str, k2Str, k3Str, p1Str, p2Str,
             wdStr, pixelEquivStr, rmsStr,
             QString::fromStdString(configDir),
             camDisplayName);

    QMessageBox::information(this, QStringLiteral("标定完成 — %1").arg(camDisplayName), info);
}

void ToolCalibDistortionWindow::onPreviewCorners()
{
    if (capturedImages_.empty())
    {
        QMessageBox::information(this, QStringLiteral("角点预览"),
            QStringLiteral("当前没有标定图像。\n请先“加载标定图”或保存当前帧。"));
        return;
    }

    const global::CameraIndex camIdx = selectedCamera_.load(std::memory_order_acquire);
    auto& item = inf_.calib_config_module_->calibConfig.item(camIdx);
    if (item.calibBoardDescrPath.empty())
    {
        QMessageBox::warning(this, QStringLiteral("角点预览"),
            QStringLiteral("请先选择标定板描述文件（.descr）"));
        return;
    }

    CornerPreviewDialog dlg(*calibInfTool_, item, capturedImages_, this);
    dlg.exec();
}

// ===================================================================
// 辅助方法
// ===================================================================
void ToolCalibDistortionWindow::updateConnectionStatus()
{
    if (!inf_.camera_module_)
    {
        statusBar()->showMessage(QStringLiteral("相机模块未初始化"));
        return;
    }

    const bool cam1 = inf_.camera_module_->isConnected(global::CameraIndex::Camera1);
    const bool cam2 = inf_.camera_module_->isConnected(global::CameraIndex::Camera2);

    const QString msg = QStringLiteral("相机1: %1 | 相机2: %2 | 当前: %3")
        .arg(cam1 ? QStringLiteral("已连接") : QStringLiteral("未连接"))
        .arg(cam2 ? QStringLiteral("已连接") : QStringLiteral("未连接"))
        .arg(cameraDisplayName(selectedCamera_.load(std::memory_order_acquire)));

    statusBar()->showMessage(msg);
}

global::CameraIndex ToolCalibDistortionWindow::cameraIndexFromCombo(int index)
{
    return (index == 1) ? global::CameraIndex::Camera2 : global::CameraIndex::Camera1;
}

QString ToolCalibDistortionWindow::cameraDisplayName(global::CameraIndex idx)
{
    return (idx == global::CameraIndex::Camera1)
        ? QStringLiteral("相机1") : QStringLiteral("相机2");
}

void ToolCalibDistortionWindow::applyDefaultCameraParams()
{
    if (!inf_.camera_module_)
        return;

    const int exposure = ui->exposureSpin->value();
    const int gain = ui->gainSpin->value();

    bool ok = true;
    ok &= inf_.camera_module_->setExposure(global::CameraIndex::Camera1, exposure);
    ok &= inf_.camera_module_->setExposure(global::CameraIndex::Camera2, exposure);
    ok &= inf_.camera_module_->setGain(global::CameraIndex::Camera1, gain);
    ok &= inf_.camera_module_->setGain(global::CameraIndex::Camera2, gain);

    // 从持久化配置恢复旋转次数到 UI spinner（当前选中相机）
    if (inf_.config_module_)
    {
        const auto& cfg = inf_.config_module_->cameraCfg;
        const global::CameraIndex idx = cameraIndexFromCombo(ui->cameraSelect->currentIndex());
        const int rotateCount = (idx == global::CameraIndex::Camera1)
            ? cfg.rotateCount1 : cfg.rotateCount2;
        ui->rotateSpin->setValue(rotateCount);
    }

    if (ok)
    {
        statusBar()->showMessage(
            QStringLiteral("已按默认值设置曝光=%1us, 增益=%2").arg(exposure).arg(gain));
    }
    else
    {
        statusBar()->showMessage(
            QStringLiteral("部分相机默认参数设置失败，请检查连接"));
    }
}
