#include "ToolCalibDistortionWindow.hpp"
#include "ui_ToolCalibDistortionWindow.h"

#include "CornerPreviewDialog.hpp"
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
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// 将相机回调收到的 cv::Mat 转换为可在 QLabel 中显示的 QImage
static QImage cvMatToQImage(const cv::Mat& mat);

ToolCalibDistortionWindow::ToolCalibDistortionWindow(inf::infrastructure& inf, QWidget* parent)
    : QMainWindow(parent)
    , inf_(inf)
    , calibrator_(std::make_unique<OpenCvCalibrator>())
    , ui(new Ui::ToolCalibDistortionWindowClass())
{
    calibrator_->setBoardSize(cv::Size(7, 7), 10, CalibrationPattern::SymmetricCircles);
    ui->setupUi(this);
    buildConnections();
    updateConnectionStatus();
    applyDefaultCameraParams();
}

ToolCalibDistortionWindow::~ToolCalibDistortionWindow()
{
    delete ui;
}

void ToolCalibDistortionWindow::buildConnections()
{
    connect(ui->startStopBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onStartStop);
    connect(ui->undistortBtn, &QPushButton::toggled, this, &ToolCalibDistortionWindow::onToggleUndistort);
    connect(ui->cameraSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ToolCalibDistortionWindow::onCameraSelected);

    connect(ui->loadCalibImagesBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onLoadCalibrationImages);
    connect(ui->saveFrameBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onSaveCurrentFrame);
    connect(ui->calibrateBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onCalibrate);
    connect(ui->saveParamsBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onSaveParams);
    connect(ui->loadParamsBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onLoadParams);
    connect(ui->previewCornersBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onPreviewCorners);

    connect(ui->setExposureBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onSetExposure);
    connect(ui->setGainBtn, &QPushButton::clicked, this, &ToolCalibDistortionWindow::onSetGain);

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

void ToolCalibDistortionWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    refreshOriginalView();
    refreshUndistortedView();
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

void ToolCalibDistortionWindow::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
{
    if (cameraIndex != selectedCamera_.load(std::memory_order_acquire))
        return;

    try
    {
        // 缓存当前原始帧，用于保存标定图
        lastRawMat_ = matInfo.mat.clone();

        // 左侧：OpenCV 畸变矫正后的图像
        cv::Mat undistortedMat = matInfo.mat;
        if (undistortEnabled_.load(std::memory_order_acquire))
            undistortedMat = calibrator_->undistort(undistortedMat);

        emit undistortedFrameReady(cvMatToQImage(undistortedMat));

        // 右侧：原始 cv::Mat，便于与左侧矫正结果对比
        emit originalFrameReady(cvMatToQImage(matInfo.mat));
    }
    catch (...)
    {
        // 帧处理失败时静默丢弃，保持采集不中断
    }
}

void ToolCalibDistortionWindow::onOriginalDisplayFrame(QImage image)
{
    if (image.isNull())
        return;

    lastOriginalImage_ = image;
    refreshOriginalView();
}

void ToolCalibDistortionWindow::onUndistortedDisplayFrame(QImage image)
{
    if (image.isNull())
        return;

    lastUndistortedImage_ = image;
    refreshUndistortedView();
}

void ToolCalibDistortionWindow::refreshOriginalView()
{
    if (lastOriginalImage_.isNull() || !ui->originalView)
        return;

    const QSize viewSize = ui->originalView->size();
    if (viewSize.width() > 0 && viewSize.height() > 0)
        ui->originalView->setPixmap(QPixmap::fromImage(lastOriginalImage_).scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        ui->originalView->setPixmap(QPixmap::fromImage(lastOriginalImage_));
}

void ToolCalibDistortionWindow::refreshUndistortedView()
{
    if (lastUndistortedImage_.isNull() || !ui->undistortedView)
        return;

    const QSize viewSize = ui->undistortedView->size();
    if (viewSize.width() > 0 && viewSize.height() > 0)
        ui->undistortedView->setPixmap(QPixmap::fromImage(lastUndistortedImage_).scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        ui->undistortedView->setPixmap(QPixmap::fromImage(lastUndistortedImage_));
}

void ToolCalibDistortionWindow::onConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
{
    (void)idx;
    (void)connected;
    (void)reason;
    updateConnectionStatus();
}

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

void ToolCalibDistortionWindow::onToggleUndistort(bool checked)
{
    undistortEnabled_.store(checked, std::memory_order_release);
    ui->undistortBtn->setText(checked ? QStringLiteral("关闭畸变矫正") : QStringLiteral("畸变矫正"));
    statusBar()->showMessage(checked ? QStringLiteral("畸变矫正已开启") : QStringLiteral("畸变矫正已关闭"));
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
    statusBar()->showMessage(QStringLiteral("当前操作/显示已切换到 %1").arg(cameraDisplayName(idx)));
}

void ToolCalibDistortionWindow::onLoadCalibrationImages()
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

void ToolCalibDistortionWindow::onSaveCurrentFrame()
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

void ToolCalibDistortionWindow::onCalibrate()
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

void ToolCalibDistortionWindow::onSaveParams()
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

void ToolCalibDistortionWindow::onLoadParams()
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

void ToolCalibDistortionWindow::onPreviewCorners()
{
    if (calibrator_->cornerImageCount() == 0)
    {
        QMessageBox::information(this, QStringLiteral("角点预览"),
            QStringLiteral("当前没有成功检测到角点的图像。\n请先“加载标定图”或采集并保存带棋盘格的图像。"));
        return;
    }

    CornerPreviewDialog dlg(*calibrator_, this);
    dlg.exec();
}

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
        statusBar()->showMessage(QStringLiteral("已按默认值设置曝光=%1us, 增益=%2")
            .arg(exposure).arg(gain));
    }
    else
    {
        statusBar()->showMessage(QStringLiteral("部分相机默认参数设置失败，请检查连接"));
    }
}
