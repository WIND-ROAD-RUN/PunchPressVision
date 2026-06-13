#pragma once

#include <atomic>

#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include "Business/CalibBun/CalibBun.hpp"
#include "UI/HalconView.h"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"
#include "rwul/hoecm/hoec_m.hpp"

#include <QMetaType>

Q_DECLARE_METATYPE(HalconCpp::HImage)

class CalibBunTestWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CalibBunTestWindow(inf::infrastructure& inf, QWidget* parent = nullptr);
    ~CalibBunTestWindow() override;

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
    void onDisplayFrame(HalconCpp::HImage image);
    void onConnectionChanged(global::CameraIndex idx, bool connected, QString reason);

    void onSetExposure();
    void onSetGain();
    void onToggleUndistort(bool checked);
    void onStartStop();

signals:
    void frameReady(HalconCpp::HImage image);

private:
    void buildUi();
    void buildConnections();

private:
    inf::infrastructure& inf_;
    bun::CalibBun calibBun_;
    ui::HalconView halconView_;

    QWidget* imageHost_ = nullptr;
    QPushButton* startStopBtn_ = nullptr;
    QPushButton* undistortBtn_ = nullptr;
    QSpinBox* exposureSpin_ = nullptr;
    QSpinBox* gainSpin_ = nullptr;

    std::atomic_bool undistortEnabled_{ false };
    std::atomic_bool isRunning_{ false };
    bool halconWindowEnsured_ = false;
};
