#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QPoint>
#include <QSize>
#include <atomic>
#include <mutex>
#if 0 // --- 以下项目引用暂时注释 ---
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

class ImageStitcchingParam;
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_createshapemodelClass; };
QT_END_NAMESPACE

class QObject;
class QEvent;

class Dlg_createshapemodel : public QDialog
{
	Q_OBJECT
public:
	Dlg_createshapemodel(QWidget* parent = nullptr);
	~Dlg_createshapemodel();
#if 0 // --- 以下内容暂时注释 ---
private:
	std::atomic_flag _genMapRunning = ATOMIC_FLAG_INIT;
	QFutureWatcher<void> _cameraDisplayWatcher;

	std::mutex _stitchMutex;
	HalconCpp::HImage* _cam1Buffer = nullptr;
	HalconCpp::HImage* _cam2Buffer = nullptr;
	bool _cam1Ready = false;
	bool _cam2Ready = false;
	std::atomic_flag _stitchRunning = ATOMIC_FLAG_INIT;

	bool _isDualCameraMode = false;
	ImageStitcchingParam _stitchParam;

	void handleDualCameraStitch(const HalconCpp::HImage& src, size_t cameraIndex);
	void processStitchedImage(HalconCpp::HImage img1, HalconCpp::HImage img2);
	void handleSingleCamera(const HalconCpp::HImage& src);
	void clearStitchBuffers();

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

	QWidget* _halconHost = nullptr;
	QSize _labelImgDisplaySize;
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
	ProcessModule::RoiPurpose _lastPaintPurpose{ ProcessModule::RoiPurpose::None };

private:
	void readImage();
	void paintCreateRegion();
	void paintShieldRegion();
	bool createShapeModelFromRois(ProcessParam& _processParam);
	bool createShapeModel();
#endif
public:
	Ui::Dlg_createshapemodelClass* ui;
};
