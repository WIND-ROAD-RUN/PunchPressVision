#pragma once

#include <QDialog>
#include <rqwcm/rqwc_m.hpp>

#include "ImageStitcchingParam.hpp"
#include "ui_Dlg_ImageStitching.h"
#include "ProcessModule.hpp"
#include "CalibParam.hpp"
// Halcon forward declarations
namespace HalconCpp
{
	class HTuple;
	class HObject;
	class HImage;
}


QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_ImageStitchingClass; };
QT_END_NAMESPACE

class Dlg_ImageStitching : public QDialog
{
	Q_OBJECT

public:
	Dlg_ImageStitching(QWidget *parent = nullptr);
	~Dlg_ImageStitching();
private:
	void build_ui();
	void build_connect();

	// Halcon 窗口相关（三个显示区域）
	enum class DisplayWindow { Window1, Window2, WindowMain };
	
	bool ensureHalconWindow(DisplayWindow win);
	void closeHalconWindow(DisplayWindow win);
	void closeAllHalconWindows();
	void redrawHalconView(DisplayWindow win, bool clearWindow = true);
	bool ensureHalconViewPart(DisplayWindow win);
	void resetHalconViewPartToFullImage(DisplayWindow win);
	void zoomHalconViewAt(DisplayWindow win, const QPoint& hostPos, int steps);
	void panHalconViewFromDrag(DisplayWindow win, const QPoint& dragDelta);
	
	// 设置要显示的图像
	void setDisplayImage(DisplayWindow win, const HalconCpp::HImage& image);
	
	ProcessModule _ProcessModule;

protected:
	void showEvent(QShowEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	void closeEvent(QCloseEvent*) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void btn_exit_clicked();

	void btn_takePicture_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang1_clicked();
	void btn_zengyi2_clicked();
	void btn_baoguang2_clicked();

	void btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked();

	void btn_caiqiebili_clicked();

	void btn_buchang1_clicked();
	void btn_buchang2_clicked();

	void btn_yongshi_clicked();
	void btn_pinjieguocheng_clicked();

	void btn_xinjianpinjieliaohao_clicked();
	void btn_liaohaomingcheng_clicked();
	void btn_xiugaimingcheng_clicked();
	void btn_delete_clicked();
	void btn_deleteAll_clicked();
	void btn_dangqianxuanzedepinjieliaohao_clicked();

	void btn_test_clicked();
public slots:
	void onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index);

private:
	Ui::Dlg_ImageStitchingClass *ui;

	bool lastIsCamera1SoftTrigger{ false };
	bool lastIsCamera2SoftTrigger{ false };

	// Halcon 窗口相关成员（三个窗口）
	struct HalconWindowData
	{
		QWidget* host = nullptr;
		HalconCpp::HTuple* windowHandle = nullptr;
		HalconCpp::HImage* lastImage = nullptr;
		QSize originalSize;
		
		// 视图控制
		struct ViewPart
		{
			double r1 = 0.0, c1 = 0.0, r2 = 0.0, c2 = 0.0;
		};
		ViewPart viewPart;
		ViewPart panStartPart;
		bool viewPartValid = false;
		int viewImgW = 0;
		int viewImgH = 0;
		
		// 鼠标交互
		bool isPanning = false;
		QPoint panStartPos;
	};
	HalconWindowData _win1;  // label_imgDisplay1 - 相机1
	HalconWindowData _win2;  // label_imgDisplay2 - 相机2  
	HalconWindowData _winMain;  // label_imgDisplay - 拼接结果
	
	HalconWindowData* getWindowData(DisplayWindow win);
	DisplayWindow _currentActiveWindow = DisplayWindow::WindowMain;

	// 相机图像缓存
	HalconCpp::HImage* _camera1Image = nullptr;
	HalconCpp::HImage* _camera2Image = nullptr;

	// 标定和拼接参数
	ImageStitcchingParam _stitchParam{};
	CalibParam _cam1Calib{};
	CalibParam _cam2Calib{};
};
