#pragma once

#include <QCheckBox>
#include <QMainWindow>
#include <QPoint>
#include <atomic>
#if 0 // --- 以下项目引用暂时注释 ---
#include "PictureViewerThumbnails.h"
#include "ImageEnlargedDisplay.h"
#include "rqw_LabelClickable.h"
#include "PanZoomLabel.h"
#include "rqwu/rqwu_DlgModelManager.h"
#include "WarnUtilty.hpp"
#include "func/ProcessModule.hpp"
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class PunchPressClass; };
QT_END_NAMESPACE

class PunchPress : public QMainWindow
{
	Q_OBJECT
public:
	PunchPress(QWidget *parent = nullptr);
	~PunchPress();
#if 0 // --- 以下内容暂时注释 ---
public:
	void build_ui();
	void build_connect();
	void build_PunchPressData();
	void ini_clickableTitle();

public:
	void initializeComponents();
	void destroyComponents();
public:
	void getCameraStateAndUpdateUi();

	void build_DlgModelManager();
	void destroy_DlgModelManager();
public slots:
	// 回调函数
	void onImageProcessingFrameReady();
	// 更新tabwidget结果
	void onUpdateTabWidgetResult(const float centerX, const float centerY, const float angle);

	void updateCameraLabelState(int cameraIndex, bool state);

	void onUpdateStatisticalInfoUI();

	void onUpdateDlgModelManager();

	void onDeleteModelModelManager(rw::rqwu::ModelInfo info);
	void onLoadModelModelManager(rw::rqwu::ModelInfo info);
	void onModelSelectChangeModelManager(rw::rqwu::ModelInfo info, QMap<rw::rqwu::ClassID, QLabel*> classIdToLabel);
	void onReNameModel(rw::rqwu::ModelInfo info, QString newName);

private slots:
	void lb_title_clicked();
private slots:
	void pbtn_exit_clicked();
	void rbtn_debug_checked(bool checked);
	void rbtn_removeFunc_checked(bool checked);

	void btn_saveProcessParam_clicked();
	void btn_setOffSet_clicked();

	void btn_save_clicked();

	void btn_baoguang1_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang2_clicked();
	void btn_zengyi2_clicked();

	void btn_createModel_clicked();
	void btn_changeModel_clicked();
	void btn_loadModel_clicked();

	void btn_jiudianbiaoding_clicked();
	void btn_jibianjiaozheng_clicked();
	void btn_imageStitching_clicked();

	void ckb_jioucaiqie_checked(bool checked);
public slots:
	void rbtn_upLight_clicked();
	void rbtn_downLight_clicked();

signals:
	void shibiekuangChanged();
private:
	void loadCompanyTXT();
	void build_HalconDisplay();

	QWidget* _halconHost = nullptr;

	// Halcon 显示（主界面）
	QSize _imgDis1Size;
	HalconCpp::HTuple* _halconWindowHandle = nullptr;
	HalconCpp::HImage* _halconLastImage = nullptr;
	HalconCpp::HObject* _halconLastXld = nullptr;
	std::atomic<bool> _isDrawing{ false };
	ProcessModule _ProcessModule;

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

	bool _panCandidate{ false };
	bool _isPanning{ false };
	QPoint _panStartPos{};
	HalconViewPart _panStartPart{};

	bool ensureHalconWindow();
	void closeHalconWindow();
	void redrawHalconView(bool clearWindow);

protected:
	void showEvent(QShowEvent* e) override;
	void resizeEvent(QResizeEvent* e) override;
	bool eventFilter(QObject* watched, QEvent* event) override;
private slots:

private:
	int minimizeCount{ 3 };
	bool _warnedNoCalibParam{ false };
private:
	rw::rqw::ClickableLabel* clickableTitle = nullptr;
	PanZoomLabel* panZoomLabel = nullptr;
	rw::rqwu::DlgModelManager* _dlgModelManager = nullptr;
#endif
private:
	Ui::PunchPressClass* ui;
};
