#include "CalibBunTestWindow.hpp"

#include "Business/CameraBun/CameraImgConvert.hpp"
#include "OpenCvCalibrator.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// 将相机回调收到的 cv::Mat 转换为可在 QLabel 中显示的 QImage
static QImage cvMatToQImage(const cv::Mat& mat);

CalibBunTestWindow::CalibBunTestWindow(inf::infrastructure& inf, QWidget* parent)
    : QMainWindow(parent)
    , inf_(inf)
    , calibrator_(std::make_unique<OpenCvCalibrator>())
{
    buildUi();
    buildConnections();
    updateConnectionStatus();
    applyDefaultCameraParams();
}

CalibBunTestWindow::~CalibBunTestWindow()
{
    halconView_.close();
}

void CalibBunTestWindow::buildUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // 控制栏
    auto* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);

    startStopBtn_ = new QPushButton(QStringLiteral("开始采集"), this);
    controlLayout->addWidget(startStopBtn_);

    undistortBtn_ = new QPushButton(QStringLiteral("畸变矫正"), this);
    undistortBtn_->setCheckable(true);
    controlLayout->addWidget(undistortBtn_);

    loadCalibImagesBtn_ = new QPushButton(QStringLiteral("加载标定图"), this);
    controlLayout->addWidget(loadCalibImagesBtn_);

    saveFrameBtn_ = new QPushButton(QStringLiteral("保存当前帧"), this);
    controlLayout->addWidget(saveFrameBtn_);

    calibrateBtn_ = new QPushButton(QStringLiteral("执行标定"), this);
    controlLayout->addWidget(calibrateBtn_);

    saveParamsBtn_ = new QPushButton(QStringLiteral("保存参数"), this);
    controlLayout->addWidget(saveParamsBtn_);

    loadParamsBtn_ = new QPushButton(QStringLiteral("加载参数"), this);
    controlLayout->addWidget(loadParamsBtn_);

    controlLayout->addWidget(new QLabel(QStringLiteral("当前相机:"), this));
    cameraSelect_ = new QComboBox(this);
    cameraSelect_->addItem(QStringLiteral("相机1"), static_cast<int>(global::CameraIndex::Camera1));
    cameraSelect_->addItem(QStringLiteral("相机2"), static_cast<int>(global::CameraIndex::Camera2));
    cameraSelect_->setCurrentIndex(0);
    controlLayout->addWidget(cameraSelect_);

    controlLayout->addStretch();

    controlLayout->addWidget(new QLabel(QStringLiteral("曝光(us):"), this));
    exposureSpin_ = new QSpinBox(this);
    exposureSpin_->setRange(1, 500000);
    exposureSpin_->setValue(10000);
    exposureSpin_->setSingleStep(100);
    controlLayout->addWidget(exposureSpin_);

    auto* setExposureBtn = new QPushButton(QStringLiteral("设置曝光"), this);
    controlLayout->addWidget(setExposureBtn);

    controlLayout->addWidget(new QLabel(QStringLiteral("增益:"), this));
    gainSpin_ = new QSpinBox(this);
    gainSpin_->setRange(0, 255);
    gainSpin_->setValue(1);
    controlLayout->addWidget(gainSpin_);

    auto* setGainBtn = new QPushButton(QStringLiteral("设置增益"), this);
    controlLayout->addWidget(setGainBtn);

    rootLayout->addLayout(controlLayout);

    // 图像显示区：左 Halcon，右原始 Mat
    auto* imageLayout = new QHBoxLayout();
    imageLayout->setSpacing(8);

    auto* halconPanel = new QVBoxLayout();
    halconPanel->setSpacing(4);
    halconPanel->addWidget(new QLabel(QStringLiteral("OpenCV 矫正后 (Halcon 显示)"), this));
    halconHost_ = new QWidget(this);
    halconHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    halconHost_->setMinimumSize(320, 240);
    halconHost_->setStyleSheet(QStringLiteral("background-color: #333;"));
    halconPanel->addWidget(halconHost_, 1);
    imageLayout->addLayout(halconPanel, 1);

    auto* matPanel = new QVBoxLayout();
    matPanel->setSpacing(4);
    matPanel->addWidget(new QLabel(QStringLiteral("原始 Mat 图像"), this));
    matView_ = new QLabel(this);
    matView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    matView_->setMinimumSize(320, 240);
    matView_->setAlignment(Qt::AlignCenter);
    matView_->setStyleSheet(QStringLiteral("background-color: #333;"));
    matPanel->addWidget(matView_, 1);
    imageLayout->addLayout(matPanel, 1);

    rootLayout->addLayout(imageLayout, 1);

    // 状态栏提示
    statusBar()->showMessage(QStringLiteral("就绪"));

    connect(setExposureBtn, &QPushButton::clicked, this, &CalibBunTestWindow::onSetExposure);
    connect(setGainBtn, &QPushButton::clicked, this, &CalibBunTestWindow::onSetGain);
}

void CalibBunTestWindow::buildConnections()
{
    connect(startStopBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onStartStop);
    connect(undistortBtn_, &QPushButton::toggled, this, &CalibBunTestWindow::onToggleUndistort);
    connect(cameraSelect_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &CalibBunTestWindow::onCameraSelected);

    connect(loadCalibImagesBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onLoadCalibrationImages);
    connect(saveFrameBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onSaveCurrentFrame);
    connect(calibrateBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onCalibrate);
    connect(saveParamsBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onSaveParams);
    connect(loadParamsBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onLoadParams);

    // 相机回调使用 DirectConnection，在采集线程中处理图像并排队到 UI 线程显示
    if (inf_.camera_module_)
    {
        connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
            this, &CalibBunTestWindow::onCameraFrame, Qt::DirectConnection);
        connect(inf_.camera_module_.get(), &inf::CameraModule::cameraConnectionStateChanged,
            this, &CalibBunTestWindow::onConnectionChanged, Qt::QueuedConnection);
    }

    // 显示信号必须排队到 UI 线程，避免在非 UI 线程操作 Halcon 窗口 / QLabel
    connect(this, &CalibBunTestWindow::frameReady,
        this, &CalibBunTestWindow::onDisplayFrame, Qt::QueuedConnection);
    connect(this, &CalibBunTestWindow::matFrameReady,
        this, &CalibBunTestWindow::onMatDisplayFrame, Qt::QueuedConnection);
}

void CalibBunTestWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!halconWindowEnsured_)
    {
        halconView_.ensure(halconHost_);
        halconWindowEnsured_ = true;
    }
}

void CalibBunTestWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    halconView_.resizeToHost();
    refreshMatView();
}

void CalibBunTestWindow::closeEvent(QCloseEvent* event)
{
    if (isRunning_.load())
    {
        if (inf_.camera_module_)
            inf_.camera_module_->stopMonitor();
        isRunning_.store(false);
    }
    halconView_.close();
    event->accept();
}

void CalibBunTestWindow::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
{
    if (cameraIndex != selectedCamera_.load(std::memory_order_acquire))
        return;

    try
    {
        // 缓存当前原始帧，用于保存标定图
        lastRawMat_ = matInfo.mat.clone();

        // 左侧：经 OpenCV 畸变矫正后的图像 -> Halcon 显示
        cv::Mat displayMat = matInfo.mat;
        if (undistortEnabled_.load(std::memory_order_acquire))
            displayMat = calibrator_->undistort(displayMat);

        HalconCpp::HImage hImage = bun::CameraImgConvert::cvMatToHImage(displayMat);
        if (hImage.IsInitialized())
            emit frameReady(hImage);

        // 右侧：原始 cv::Mat，便于与左侧矫正结果对比
        emit matFrameReady(cvMatToQImage(matInfo.mat));
    }
    catch (...)
    {
        // 帧处理失败时静默丢弃，保持采集不中断
    }
}

void CalibBunTestWindow::onDisplayFrame(HalconCpp::HImage image)
{
    halconView_.display(image);
}

void CalibBunTestWindow::onMatDisplayFrame(QImage image)
{
    if (image.isNull())
        return;

    lastMatImage_ = image;
    refreshMatView();
}

void CalibBunTestWindow::refreshMatView()
{
    if (lastMatImage_.isNull() || !matView_)
        return;

    const QSize viewSize = matView_->size();
    if (viewSize.width() > 0 && viewSize.height() > 0)
        matView_->setPixmap(QPixmap::fromImage(lastMatImage_).scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        matView_->setPixmap(QPixmap::fromImage(lastMatImage_));
}

void CalibBunTestWindow::onConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
{
    (void)idx;
    (void)connected;
    (void)reason;
    updateConnectionStatus();
}

void CalibBunTestWindow::onSetExposure()
{
    if (!inf_.camera_module_)
        return;

    const int value = exposureSpin_->value();
    const global::CameraIndex idx = cameraIndexFromCombo(cameraSelect_->currentIndex());
    const bool ok = inf_.camera_module_->setExposure(idx, value);

    if (ok)
        statusBar()->showMessage(QStringLiteral("%1 曝光已设置为 %2 us")
            .arg(cameraDisplayName(idx)).arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"),
            QStringLiteral("无法设置 %1 曝光").arg(cameraDisplayName(idx)));
}

void CalibBunTestWindow::onSetGain()
{
    if (!inf_.camera_module_)
        return;

    const int value = gainSpin_->value();
    const global::CameraIndex idx = cameraIndexFromCombo(cameraSelect_->currentIndex());
    const bool ok = inf_.camera_module_->setGain(idx, value);

    if (ok)
        statusBar()->showMessage(QStringLiteral("%1 增益已设置为 %2")
            .arg(cameraDisplayName(idx)).arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"),
            QStringLiteral("无法设置 %1 增益").arg(cameraDisplayName(idx)));
}

void CalibBunTestWindow::onToggleUndistort(bool checked)
{
    undistortEnabled_.store(checked, std::memory_order_release);
    undistortBtn_->setText(checked ? QStringLiteral("关闭畸变矫正") : QStringLiteral("畸变矫正"));
    statusBar()->showMessage(checked ? QStringLiteral("畸变矫正已开启") : QStringLiteral("畸变矫正已关闭"));
}

void CalibBunTestWindow::onStartStop()
{
    if (!inf_.camera_module_)
        return;

    if (isRunning_.load(std::memory_order_acquire))
    {
        inf_.camera_module_->stopMonitor();
        isRunning_.store(false, std::memory_order_release);
        startStopBtn_->setText(QStringLiteral("开始采集"));
        statusBar()->showMessage(QStringLiteral("采集已停止"));
    }
    else
    {
        inf_.camera_module_->startMonitor();
        isRunning_.store(true, std::memory_order_release);
        startStopBtn_->setText(QStringLiteral("停止采集"));
        statusBar()->showMessage(QStringLiteral("采集中..."));
    }
}

void CalibBunTestWindow::onCameraSelected(int index)
{
    const global::CameraIndex idx = cameraIndexFromCombo(index);
    selectedCamera_.store(idx, std::memory_order_release);
    statusBar()->showMessage(QStringLiteral("当前操作/显示已切换到 %1").arg(cameraDisplayName(idx)));
}

void CalibBunTestWindow::onLoadCalibrationImages()
{
    const QString dir = lastCalibDir_.isEmpty()
        ? QDir::currentPath()
        : lastCalibDir_;

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择棋盘格标定图像"),
        dir,
        QStringLiteral("图像文件 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff);;所有文件 (*.*)"));

    if (files.isEmpty())
        return;

    lastCalibDir_ = QFileInfo(files.first()).absolutePath();
    calibrator_->clearCalibrationImages();

    int successCount = 0;
    int failCount = 0;
    for (const QString& file : files)
    {
        cv::Mat img = cv::imread(file.toStdString(), cv::IMREAD_UNCHANGED);
        if (img.empty())
        {
            ++failCount;
            continue;
        }

        if (calibrator_->addCalibrationImage(img))
            ++successCount;
        else
            ++failCount;
    }

    statusBar()->showMessage(
        QStringLiteral("加载完成：成功 %1 张，失败 %2 张，当前共 %3 张")
            .arg(successCount).arg(failCount).arg(calibrator_->calibrationImageCount()));
}

void CalibBunTestWindow::onSaveCurrentFrame()
{
    if (lastRawMat_.empty())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("当前没有可保存的帧"));
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
        statusBar()->showMessage(QStringLiteral("已保存：%1").arg(path));
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入：%1").arg(path));
    }
}

void CalibBunTestWindow::onCalibrate()
{
    if (calibrator_->calibrationImageCount() < 3)
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"),
            QStringLiteral("标定图像不足（至少需要 3 张，当前 %1 张）")
                .arg(calibrator_->calibrationImageCount()));
        return;
    }

    statusBar()->showMessage(QStringLiteral("正在标定..."));
    const double rms = calibrator_->calibrate();

    if (rms < 0.0)
    {
        QMessageBox::warning(this, QStringLiteral("标定失败"), QStringLiteral("calibrateCamera 返回错误"));
        return;
    }

    statusBar()->showMessage(QStringLiteral("标定完成，RMS = %1 px").arg(rms, 0, 'f', 4));
    QMessageBox::information(this, QStringLiteral("标定完成"),
        QStringLiteral("RMS 重投影误差：%1 px\n已可开启“畸变矫正”预览。").arg(rms, 0, 'f', 4));
}

void CalibBunTestWindow::onSaveParams()
{
    if (!calibrator_->isCalibrated())
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("尚未完成标定"));
        return;
    }

    const QString dir = lastCalibDir_.isEmpty()
        ? QDir::currentPath()
        : lastCalibDir_;

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存 OpenCV 标定参数"),
        dir + QStringLiteral("/opencv_camera_params.yml"),
        QStringLiteral("YAML 文件 (*.yml *.yaml)"));

    if (path.isEmpty())
        return;

    if (calibrator_->saveParameters(path.toStdString()))
    {
        lastCalibDir_ = QFileInfo(path).absolutePath();
        statusBar()->showMessage(QStringLiteral("参数已保存：%1").arg(path));
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入：%1").arg(path));
    }
}

void CalibBunTestWindow::onLoadParams()
{
    const QString dir = lastCalibDir_.isEmpty()
        ? QDir::currentPath()
        : lastCalibDir_;

    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("加载 OpenCV 标定参数"),
        dir,
        QStringLiteral("YAML 文件 (*.yml *.yaml);;所有文件 (*.*)"));

    if (path.isEmpty())
        return;

    if (calibrator_->loadParameters(path.toStdString()))
    {
        lastCalibDir_ = QFileInfo(path).absolutePath();
        statusBar()->showMessage(
            QStringLiteral("参数已加载：%1，RMS = %2 px")
                .arg(path).arg(calibrator_->rms(), 0, 'f', 4));
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("无法读取：%1").arg(path));
    }
}

void CalibBunTestWindow::updateConnectionStatus()
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

global::CameraIndex CalibBunTestWindow::cameraIndexFromCombo(int index)
{
    return (index == 1) ? global::CameraIndex::Camera2 : global::CameraIndex::Camera1;
}

QString CalibBunTestWindow::cameraDisplayName(global::CameraIndex idx)
{
    return (idx == global::CameraIndex::Camera1)
        ? QStringLiteral("相机1") : QStringLiteral("相机2");
}

static QImage cvMatToQImage(const cv::Mat& mat)
{
    if (mat.empty())
        return QImage();

    switch (mat.type())
    {
    case CV_8UC1:
    {
        return QImage(mat.data, mat.cols, mat.rows,
            static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    }
    case CV_8UC3:
    {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows,
            static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    case CV_8UC4:
    {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows,
            static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    case CV_16UC1:
    {
        cv::Mat gray8;
        cv::normalize(mat, gray8, 0, 255, cv::NORM_MINMAX, CV_8UC1);
        return QImage(gray8.data, gray8.cols, gray8.rows,
            static_cast<int>(gray8.step), QImage::Format_Grayscale8).copy();
    }
    default:
        return QImage();
    }
}

void CalibBunTestWindow::applyDefaultCameraParams()
{
    if (!inf_.camera_module_)
        return;

    const int exposure = exposureSpin_->value();
    const int gain = gainSpin_->value();

    bool ok = true;
    ok &= inf_.camera_module_->setExposure(global::CameraIndex::Camera1, exposure);
    ok &= inf_.camera_module_->setExposure(global::CameraIndex::Camera2, exposure);
    ok &= inf_.camera_module_->setGain(global::CameraIndex::Camera1, gain);
    ok &= inf_.camera_module_->setGain(global::CameraIndex::Camera2, gain);

    if (ok)
    {
        statusBar()->showMessage(QStringLiteral("已按默认值设置曝光=%1us, 增益=%2")
            .arg(exposure).arg(gain));
    }
    else
    {
        statusBar()->showMessage(QStringLiteral("部分相机默认参数设置失败，请检查连接"));
    }
}
