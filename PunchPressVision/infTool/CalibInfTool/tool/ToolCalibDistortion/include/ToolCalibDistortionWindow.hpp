#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <QMainWindow>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "rwul/hoecm/hoec_m.hpp"

#include "DisplayView.hpp"

namespace infTool { class CalibInfTool; }

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
    void onOriginalDisplayFrame(HalconCpp::HImage image);
    void onUndistortedDisplayFrame(HalconCpp::HImage image);
    void onConnectionChanged(global::CameraIndex idx, bool connected, QString reason);

    void onSetExposure();
    void onSetGain();
    void onToggleUndistort(bool checked);
    void onStartStop();
    void onCameraSelected(int index);

    // Halcon 标定相关槽函数
    void onLoadCalibrationImages();
    void onSaveCurrentFrame();
    void onCalibrate();
    void onSaveParams();
    void onLoadParams();
    void onPreviewCorners();

signals:
    void originalFrameReady(HalconCpp::HImage image);
    void undistortedFrameReady(HalconCpp::HImage image);

private:
    void buildConnections();
    void initBoardDescrPath();
    void updateConnectionStatus();
    void applyDefaultCameraParams();
    static global::CameraIndex cameraIndexFromCombo(int index);
    static QString cameraDisplayName(global::CameraIndex idx);

private:
    inf::infrastructure& inf_;
    std::unique_ptr<infTool::CalibInfTool> calibInfTool_;
    Ui::ToolCalibDistortionWindowClass* ui = nullptr;

    DisplayView originalView_;
    DisplayView undistortedView_;

    // Halcon 标定图像缓存
    std::vector<HalconCpp::HImage> capturedImages_;

    std::atomic<global::CameraIndex> selectedCamera_{ global::CameraIndex::Camera1 };
    std::atomic_bool undistortEnabled_{ false };
    std::atomic_bool isRunning_{ false };

    cv::Mat lastRawMat_;
    QString lastCalibDir_;
};
