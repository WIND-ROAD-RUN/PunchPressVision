#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <QMainWindow>
#include <QWidget>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "rwul/hoecm/hoec_m.hpp"

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
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
    void onOriginalDisplayFrame(HalconCpp::HImage image);
    void onConnectionChanged(global::CameraIndex idx, bool connected, QString reason);

    void onSetExposure();
    void onSetGain();
    void onSetRotate();
    void onStartStop();
    void onCameraSelected(int index);
    void onExit();

    // Halcon 标定相关槽函数
    void onSaveCurrentFrame();
    void onCalibrate();
    void onPreviewCorners();

signals:
    void originalFrameReady(HalconCpp::HImage image);

private:
    void buildConnections();
    void initBoardDescrPath();
    void cleanCalibImageDirs();
    void updateConnectionStatus();
    void applyDefaultCameraParams();
    void refreshMarkedView();
    static global::CameraIndex cameraIndexFromCombo(int index);
    static QString cameraDisplayName(global::CameraIndex idx);

private:
    inf::infrastructure& inf_;
    std::unique_ptr<infTool::CalibInfTool> calibInfTool_;
    Ui::ToolCalibDistortionWindowClass* ui = nullptr;

    // Halcon 标定图像缓存
    std::vector<HalconCpp::HImage> capturedImages_;

    // drawCalibMarks 结果缓存（用于 resize 重绘）
    HalconCpp::HImage  lastMarkedImage_;
    HalconCpp::HObject lastMarksXld_;

    std::atomic<global::CameraIndex> selectedCamera_{ global::CameraIndex::Camera1 };
    std::atomic_bool isRunning_{ false };

    cv::Mat lastRawMat_;
    QString lastCalibDir_;
};
