#include "CalibBunTestWindow.hpp"

#include "Business/CameraBun/CameraImgConvert.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

CalibBunTestWindow::CalibBunTestWindow(inf::infrastructure& inf, QWidget* parent)
    : QMainWindow(parent)
    , inf_(inf)
    , calibBun_(inf_)
{
    buildUi();
    buildConnections();
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

    // 图像显示区
    imageHost_ = new QWidget(this);
    imageHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageHost_->setMinimumSize(640, 480);
    imageHost_->setStyleSheet(QStringLiteral("background-color: #333;"));
    rootLayout->addWidget(imageHost_, 1);

    // 状态栏提示
    statusBar()->showMessage(QStringLiteral("就绪"));

    connect(setExposureBtn, &QPushButton::clicked, this, &CalibBunTestWindow::onSetExposure);
    connect(setGainBtn, &QPushButton::clicked, this, &CalibBunTestWindow::onSetGain);
}

void CalibBunTestWindow::buildConnections()
{
    connect(startStopBtn_, &QPushButton::clicked, this, &CalibBunTestWindow::onStartStop);
    connect(undistortBtn_, &QPushButton::toggled, this, &CalibBunTestWindow::onToggleUndistort);

    // 相机回调使用 DirectConnection，在采集线程中处理图像并排队到 UI 线程显示
    if (inf_.camera_module_)
    {
        connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
            this, &CalibBunTestWindow::onCameraFrame, Qt::DirectConnection);
        connect(inf_.camera_module_.get(), &inf::CameraModule::cameraConnectionStateChanged,
            this, &CalibBunTestWindow::onConnectionChanged, Qt::QueuedConnection);
    }

    // 显示信号必须排队到 UI 线程，避免在非 UI 线程操作 Halcon 窗口
    connect(this, &CalibBunTestWindow::frameReady,
        this, &CalibBunTestWindow::onDisplayFrame, Qt::QueuedConnection);
}

void CalibBunTestWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!halconWindowEnsured_)
    {
        halconView_.ensure(imageHost_);
        halconWindowEnsured_ = true;
    }
}

void CalibBunTestWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    halconView_.resizeToHost();
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
    (void)cameraIndex;

    try
    {
        HalconCpp::HImage hImage = bun::CameraImgConvert::cvMatToHImage(matInfo.mat);
        if (!hImage.IsInitialized())
            return;

        if (undistortEnabled_.load(std::memory_order_acquire))
            hImage = calibBun_.undistortImage(hImage);

        emit frameReady(hImage);
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

void CalibBunTestWindow::onConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
{
    const QString name = (idx == global::CameraIndex::Camera1)
        ? QStringLiteral("相机1") : QStringLiteral("相机2");
    if (connected)
        statusBar()->showMessage(name + QStringLiteral(" 已连接"));
    else
        statusBar()->showMessage(name + QStringLiteral(" 未连接: ") + reason);
}

void CalibBunTestWindow::onSetExposure()
{
    if (!inf_.camera_module_)
        return;

    const int value = exposureSpin_->value();
    const bool ok1 = inf_.camera_module_->setExposure(global::CameraIndex::Camera1, value);
    const bool ok2 = inf_.camera_module_->setExposure(global::CameraIndex::Camera2, value);

    if (ok1 || ok2)
        statusBar()->showMessage(QStringLiteral("曝光已设置为 %1 us").arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"), QStringLiteral("无法设置相机曝光"));
}

void CalibBunTestWindow::onSetGain()
{
    if (!inf_.camera_module_)
        return;

    const int value = gainSpin_->value();
    const bool ok1 = inf_.camera_module_->setGain(global::CameraIndex::Camera1, value);
    const bool ok2 = inf_.camera_module_->setGain(global::CameraIndex::Camera2, value);

    if (ok1 || ok2)
        statusBar()->showMessage(QStringLiteral("增益已设置为 %1").arg(value));
    else
        QMessageBox::warning(this, QStringLiteral("设置失败"), QStringLiteral("无法设置相机增益"));
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
