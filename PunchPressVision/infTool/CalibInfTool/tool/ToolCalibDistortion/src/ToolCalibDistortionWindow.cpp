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

    // 打开 Halcon 显示窗口，嵌入到 QWidget 占位控件上
    originalView_.ensure(ui->originalView);
    undistortedView_.ensure(ui->undistortedView);

    buildConnections();
    initBoardDescrPath();
    updateConnectionStatus();
    applyDefaultCameraParams();
}

ToolCalibDistortionWindow::~ToolCalibDistortionWindow()
{
    calibInfTool_->destroy();
    originalView_.close();
    undistortedView_.close();
    delete ui;
}

// ===================================================================
// UI 连接
// ===================================================================
void ToolCalibDistortionWindow::buildConnections()
{
    connect(ui->startStopBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onStartStop);
    connect(ui->undistortBtn, &QPushButton::toggled,
        this, &ToolCalibDistortionWindow::onToggleUndistort);
    connect(ui->cameraSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ToolCalibDistortionWindow::onCameraSelected);

    // 标定流程
    connect(ui->loadCalibImagesBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onLoadCalibrationImages);
    connect(ui->saveFrameBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSaveCurrentFrame);
    connect(ui->calibrateBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onCalibrate);
    connect(ui->saveParamsBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSaveParams);
    connect(ui->loadParamsBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onLoadParams);
    connect(ui->previewCornersBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onPreviewCorners);

    // 曝光 / 增益
    connect(ui->setExposureBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSetExposure);
    connect(ui->setGainBtn, &QPushButton::clicked,
        this, &ToolCalibDistortionWindow::onSetGain);

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
    connect(this, &ToolCalibDistortionWindow::undistortedFrameReady,
        this, &ToolCalibDistortionWindow::onUndistortedDisplayFrame, Qt::QueuedConnection);
}

// ===================================================================
// 窗口事件
// ===================================================================
void ToolCalibDistortionWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    originalView_.resizeToHost();
    undistortedView_.resizeToHost();
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

        // 转 HImage
        HalconCpp::HImage hSrc = cvMatToHImage(matInfo.mat);

        // 左侧：畸变矫正后的图像
        HalconCpp::HImage hUndistorted = hSrc;
        if (undistortEnabled_.load(std::memory_order_acquire))
            hUndistorted = calibInfTool_->undistortImage(hSrc);

        emit undistortedFrameReady(hUndistorted);
        // 右侧：原始图像
        emit originalFrameReady(hSrc);
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
    originalView_.display(image);
}

void ToolCalibDistortionWindow::onUndistortedDisplayFrame(HalconCpp::HImage image)
{
    undistortedView_.display(image);
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
// 采集控制
// ===================================================================
void ToolCalibDistortionWindow::onToggleUndistort(bool checked)
{
    // 检查是否已标定
    auto& cfg = inf_.calib_config_module_->calibConfig;
    if (checked && cfg.cameraParameters.Length() == 0)
    {
        QMessageBox::information(this, QStringLiteral("未标定"),
            QStringLiteral("尚未执行标定或未加载标定参数，无法开启畸变矫正。"));
        ui->undistortBtn->setChecked(false);
        return;
    }

    undistortEnabled_.store(checked, std::memory_order_release);
    ui->undistortBtn->setText(checked
        ? QStringLiteral("关闭畸变矫正") : QStringLiteral("畸变矫正"));
    statusBar()->showMessage(checked
        ? QStringLiteral("Halcon 畸变矫正已开启") : QStringLiteral("畸变矫正已关闭"));
}

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
        cfg.calibBoardDescrPath = foundPath;
        ui->calibBoardPathEdit->setText(QString::fromStdString(foundPath));
        statusBar()->showMessage(
            QStringLiteral("标定板描述文件: %1").arg(QString::fromStdString(foundPath)));
    }
    else if (!cfg.calibBoardDescrPath.empty())
    {
        // config 中未找到 .descr 但已有旧路径则沿用
        ui->calibBoardPathEdit->setText(
            QString::fromStdString(cfg.calibBoardDescrPath));
    }
    else
    {
        ui->calibBoardPathEdit->setText(QString());
        statusBar()->showMessage(
            QStringLiteral("config 目录下未找到 .descr 描述文件"));
    }
}

// ===================================================================
// Halcon 标定流程
// ===================================================================
void ToolCalibDistortionWindow::onLoadCalibrationImages()
{
    const QString dir = lastCalibDir_.isEmpty()
        ? QDir::currentPath()
        : lastCalibDir_;

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择标定图像"),
        dir,
        QStringLiteral("图像文件 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff);;所有文件 (*.*)"));

    if (files.isEmpty())
        return;

    lastCalibDir_ = QFileInfo(files.first()).absolutePath();
    capturedImages_.clear();

    int successCount = 0;
    int failCount = 0;
    for (const QString& file : files)
    {
        try
        {
            HalconCpp::HImage img(file.toStdString().c_str());
            if (img.IsInitialized())
            {
                capturedImages_.push_back(img);
                ++successCount;
            }
            else
            {
                ++failCount;
            }
        }
        catch (...)
        {
            ++failCount;
        }
    }

    statusBar()->showMessage(
        QStringLiteral("Halcon 加载完成：成功 %1 张，失败 %2 张，当前共 %3 张")
            .arg(successCount).arg(failCount).arg(capturedImages_.size()));
}

void ToolCalibDistortionWindow::onSaveCurrentFrame()
{
    if (lastRawMat_.empty())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("当前没有可保存的帧"));
        return;
    }

    const QString dir = lastCalibDir_.isEmpty()
        ? QDir::currentPath()
        : lastCalibDir_;

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存当前帧为标定图"),
        dir + QStringLiteral("/calib_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"))),
        QStringLiteral("PNG 图像 (*.png);;BMP 图像 (*.bmp);;JPEG 图像 (*.jpg)"));

    if (path.isEmpty())
        return;

    if (cv::imwrite(path.toStdString(), lastRawMat_))
    {
        lastCalibDir_ = QFileInfo(path).absolutePath();
        // 同时加入标定图集（转为 HImage）
        capturedImages_.push_back(cvMatToHImage(lastRawMat_));
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
    auto& cfg = inf_.calib_config_module_->calibConfig;

    if (cfg.calibBoardDescrPath.empty())
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
        capturedImages_, focalLength, plateThickness, /*referenceIndex*/ 0, &err);

    if (!ok)
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"),
            QString::fromStdString(err));
        return;
    }

    // 读取标定误差
    QString rmsStr;
    if (cfg.calibrationErrors.Length() > 0)
        rmsStr = QString::number(cfg.calibrationErrors[0].D(), 'f', 4);

    statusBar()->showMessage(
        QStringLiteral("Halcon 标定完成，RMS = %1 px").arg(rmsStr));
    QMessageBox::information(this, QStringLiteral("标定完成"),
        QStringLiteral("Halcon 标定完成。\n"
            "RMS 重投影误差：%1 px\n"
            "已可开启“畸变矫正”预览。").arg(rmsStr));
}

void ToolCalibDistortionWindow::onSaveParams()
{
    auto& cfg = inf_.calib_config_module_->calibConfig;
    if (cfg.cameraParameters.Length() == 0)
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("尚未完成标定，无参数可保存"));
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择参数保存目录"),
        lastCalibDir_.isEmpty() ? QDir::currentPath() : lastCalibDir_);

    if (dir.isEmpty())
        return;

    try
    {
        cfg.saveInDir(dir.toStdString());
        lastCalibDir_ = dir;
        statusBar()->showMessage(
            QStringLiteral("Halcon 标定参数已保存至：%1").arg(dir));
    }
    catch (...)
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("写入标定参数时发生异常：%1").arg(dir));
    }
}

void ToolCalibDistortionWindow::onLoadParams()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择参数加载目录"),
        lastCalibDir_.isEmpty() ? QDir::currentPath() : lastCalibDir_);

    if (dir.isEmpty())
        return;

    auto& cfg = inf_.calib_config_module_->calibConfig;
    try
    {
        cfg.loadInDir(dir.toStdString());
        lastCalibDir_ = dir;

        const int paramLen = cfg.cameraParameters.Length();
        QString rmsStr;
        if (cfg.calibrationErrors.Length() > 0)
            rmsStr = QString::number(cfg.calibrationErrors[0].D(), 'f', 4);

        statusBar()->showMessage(
            QStringLiteral("Halcon 参数已加载：%1，相机参数项 %2，RMS = %3 px")
                .arg(dir).arg(paramLen).arg(rmsStr));
    }
    catch (...)
    {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
            QStringLiteral("读取标定参数时发生异常：%1").arg(dir));
    }
}

void ToolCalibDistortionWindow::onPreviewCorners()
{
    if (capturedImages_.empty())
    {
        QMessageBox::information(this, QStringLiteral("角点预览"),
            QStringLiteral("当前没有标定图像。\n请先“加载标定图”或保存当前帧。"));
        return;
    }

    auto& cfg = inf_.calib_config_module_->calibConfig;
    if (cfg.calibBoardDescrPath.empty())
    {
        QMessageBox::warning(this, QStringLiteral("角点预览"),
            QStringLiteral("请先选择标定板描述文件（.descr）"));
        return;
    }

    CornerPreviewDialog dlg(*calibInfTool_, cfg, capturedImages_, this);
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
