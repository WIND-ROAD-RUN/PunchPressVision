#pragma once

#include <atomic>
#include <memory>

#include <QMainWindow>

#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "rwul/hoecm/hoec_m.hpp"

class OpenCvCalibrator;

QT_BEGIN_NAMESPACE
namespace Ui { class ToolCalibDistortionWindowClass; }
QT_END_NAMESPACE

class ToolCalibDistortionWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ToolCalibDistortionWindow(inf::infrastructure& inf, QWidget* parent = nullptr);
    ~ToolCalibDistortionWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
    void onOriginalDisplayFrame(QImage image);
    void onUndistortedDisplayFrame(QImage image);
    void onConnectionChanged(global::CameraIndex idx, bool connected, QString reason);

    void onSetExposure();
    void onSetGain();
    void onToggleUndistort(bool checked);
    void onStartStop();
    void onCameraSelected(int index);

    // OpenCV 标定相关槽函数
    void onLoadCalibrationImages();
    void onSaveCurrentFrame();
    void onCalibrate();
    void onSaveParams();
    void onLoadParams();
    void onPreviewCorners();

signals:
    void originalFrameReady(QImage image);
    void undistortedFrameReady(QImage image);

private:
    void buildConnections();
    void updateConnectionStatus();
    void refreshOriginalView();
    void refreshUndistortedView();
    void applyDefaultCameraParams();
    static global::CameraIndex cameraIndexFromCombo(int index);
    static QString cameraDisplayName(global::CameraIndex idx);

private:
    inf::infrastructure& inf_;
    std::unique_ptr<OpenCvCalibrator> calibrator_;
    Ui::ToolCalibDistortionWindowClass* ui = nullptr;

    std::atomic<global::CameraIndex> selectedCamera_{ global::CameraIndex::Camera1 };
    std::atomic_bool undistortEnabled_{ false };
    std::atomic_bool isRunning_{ false };

    QImage lastOriginalImage_;
    QImage lastUndistortedImage_;
    cv::Mat lastRawMat_;
    QString lastCalibDir_;
};
