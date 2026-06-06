#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QPoint>
#include <QSize>
#include <atomic>
#include <mutex>
#include <rqwccore/rqwc_types.hpp>

#include "ProcessParam.hpp"
#include "ImageStitcchingParam.hpp"
#include "ui_Dlg_createshapemodel.h"

namespace HalconCpp
{
	class HTuple;
	class HImage;
	class HObject;
}

// 前向声明
class ImageStitcchingParam;

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_createshapemodelClass; };
QT_END_NAMESPACE

class QObject;
class QEvent;

class Dlg_createshapemodel : public QDialog
{
	Q_OBJECT
private:
	// genImageToWorldPlaneMap 执行期间：丢弃后续 onCameraDisplay
	std::atomic_flag _genMapRunning = ATOMIC_FLAG_INIT;

	// 子线程任务 watcher（finished 信号在 UI 线程触发）
	QFutureWatcher<void> _cameraDisplayWatcher;

	// ========== 双相机拼接相关 ==========
	std::mutex _stitchMutex;                              // 保护图像缓存
	HalconCpp::HImage* _cam1Buffer = nullptr;            // 相机1图像缓存
	HalconCpp::HImage* _cam2Buffer = nullptr;            // 相机2图像缓存
	bool _cam1Ready = false;                             // 相机1图像是否就绪
	bool _cam2Ready = false;                             // 相机2图像是否就绪
	std::atomic_flag _stitchRunning = ATOMIC_FLAG_INIT;  // 拼接处理标志
	
	bool _isDualCameraMode = false;                      // 是否为双相机模式
	ImageStitcchingParam _stitchParam;                   // 图像拼接参数

	// 双相机拼接处理函数
	void handleDualCameraStitch(const HalconCpp::HImage& src, size_t cameraIndex);
	void processStitchedImage(HalconCpp::HImage img1, HalconCpp::HImage img2);
	void handleSingleCamera(const HalconCpp::HImage& src);
	void clearStitchBuffers();
public:
	Dlg_createshapemodel(QWidget* parent = nullptr);
	~Dlg_createshapemodel();

public:
	void build_ui();
	void setRoiEditingUiEnabled(bool enabled);
	void build_connect();
	ProcessModule _ProcessModule;
public slots:
	void onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index);

private slots:
	void btn_exit_clicked();
	void btn_readImage_clicked();
	void btn_paintRegion_clicked();

	void btn_createShapeModel_clicked();
	void btn_shiledRegion_clicked();
	void btn_clearRegion_clicked();
	void btn_paintCenterPoint_clicked();

	void rad_findshapemodel_toggled(bool checked);
	void rad_findshapemodelXLD_toggled(bool checked);

	void btn_MinLength_clicked();
	void btn_min_clicked();
	void btn_max_clicked();
	void btn_createXLD_clicked();

	void rbtn_auto_toggled(bool checked);
	void rbtn_manual_toggled(bool checked);
	void btn_contrast_clicked();
	void btn_mincontrast_clicked();

	void btn_clearRegion2_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang1_clicked();
	void btn_zengyi2_clicked();
	void btn_baoguang2_clicked();
	void btn_angle_clicked();
	void btn_opening_clicked();
	void btn_closing_clicked();
	void btn_mean_clicked();
private:
	bool isShowImg{ false };
	bool _modelSaved{ false };
	bool lastIsCamera1SoftTrigger{ false };
	bool lastIsCamera2SoftTrigger{ false };
public:
	Ui::Dlg_createshapemodelClass* ui;

private:
	// Halcon 显示承载控件 + 窗口句柄 + 最近一次显示的图像
	QWidget* _halconHost = nullptr;
	QSize _labelImgDisplaySize; // 记录替换前 label_imgDisplay 的控件尺寸
	HalconCpp::HTuple* _halconWindowHandle = nullptr;
	HalconCpp::HImage* _halconLastImage = nullptr;
	HalconCpp::HObject* _centerPointXldObj = nullptr;

	struct HalconViewPart
	{
		double r1{ 0.0 };
		double c1{ 0.0 };
		double r2{ 0.0 };
		double c2{ 0.0 };
	};

	bool ensureHalconViewPart();
	void resetHalconViewPartToFullImage();
	void zoomHalconViewAt(const QPoint& hostPos, int steps);
	void panHalconViewFromDrag(const QPoint& dragDelta);

	HalconViewPart _viewPart{};
	bool _viewPartValid{ false };
	int _viewImgW{ 0 };
	int _viewImgH{ 0 };
	bool _isPanning{ false };
	QPoint _panStartPos{};
	HalconViewPart _panStartPart{};

	bool ensureHalconWindow();
	void closeHalconWindow();
	void redrawHalconView(bool clearWindow);

private:
	bool saveModelData();
	// 重新按 UI 参数(通道/膨胀/均值)生成模板图并刷新显示
	void refreshTemplateImage();
protected:
	void showEvent(QShowEvent*) override;
	void closeEvent(QCloseEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	QImage qimg;
	ProcessParam _processParam;
	ProcessModule::RoiPurpose _roiPurpose{ ProcessModule::RoiPurpose::None };

	// 记录“最近一次绘制”的 ROI 类型（用于 btn_clearRegion2 撤销）
	ProcessModule::RoiPurpose _lastPaintPurpose{ ProcessModule::RoiPurpose::None };

private:
	void readImage();
	void paintCreateRegion();
	void paintShieldRegion();
	bool createShapeModelFromRois(ProcessParam& _processParam);
	bool createShapeModel();
};