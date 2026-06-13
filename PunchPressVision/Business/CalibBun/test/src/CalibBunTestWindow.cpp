#include "CalibBunTestWindow.hpp"

#include "Business/CameraBun/CameraImgConvert.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
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
#include <opencv2/imgproc.hpp>

// 将相机回调收到的 cv::Mat 转换为可在 QLabel 中显示的 QImage
static QImage cvMatToQImage(const cv::Mat& mat);

CalibBunTestWindow::CalibBunTestWindow(inf::infrastructure& inf, QWidget* parent)
    : QMainWindow(parent)
    , inf_(inf)
    , calibBun_(inf_)
{
    buildUi();
    buildConnections();
    updateConnectionStatus();
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
    halconPanel->addWidget(new QLabel(QStringLiteral("Halcon 图像"), this));
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
        // 左侧：经 cvMatToHImage + 可选畸变矫正后的 Halcon 图像
        HalconCpp::HImage hImage = bun::CameraImgConvert::cvMatToHImage(matInfo.mat);
        if (hImage.IsInitialized())
        {
            if (undistortEnabled_.load(std::memory_order_acquire))
                hImage = calibBun_.undistortImage(hImage);

            emit frameReady(hImage);
        }

        // 右侧：原始 cv::Mat，用于排查转换或 Halcon 内部问题
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
