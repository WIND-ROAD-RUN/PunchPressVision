#pragma once

#include <memory>

#include <QMainWindow>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "rwul/hoecm/hoec_m.hpp"

namespace inf { class infrastructure; }
namespace infTool { class TwoCameraSpliceInfTool; class CalibInfTool; }

QT_BEGIN_NAMESPACE
namespace Ui { class ToolTwoCameraSpliceWindowClass; }
QT_END_NAMESPACE

class ToolTwoCameraSpliceWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit ToolTwoCameraSpliceWindow(inf::infrastructure& inf, QWidget* parent = nullptr);
	~ToolTwoCameraSpliceWindow() override;

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void closeEvent(QCloseEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

signals:
	/// 请求 UI 线程刷新三路 Halcon 显示
	void displayFrameReady();

private slots:
	/// 接收相机原始帧（DirectConnection，采集线程）
	void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
	/// UI 线程刷新显示（QueuedConnection）
	void onDisplayFrame();

	// UI 按钮槽函数
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
	void btn_exit_clicked();

private:
	void buildUi();
	void buildConnections();
	void initCaltabDescrPath();
	void syncConfigToUi();

	// Halcon 窗口管理
	enum class DisplayWindow { Cam1, Cam2, Stitched };
	bool ensureHalconWindow(DisplayWindow win);
	void closeHalconWindows();
	void redrawDisplay(DisplayWindow win);

	// 从 QLabel 占位符创建 native host 承载 Halcon 窗口
	QWidget* replaceLabel(const QString& labelName);

	// cv::Mat → HImage 转换
	static HalconCpp::HImage cvMatToHImage(const cv::Mat& mat);

private:
	inf::infrastructure& inf_;
	std::unique_ptr<infTool::CalibInfTool> calibTool_;
	std::unique_ptr<infTool::TwoCameraSpliceInfTool> spliceTool_;
	Ui::ToolTwoCameraSpliceWindowClass* ui = nullptr;

	// 显示窗口宿主
	QWidget* hostCam1_ = nullptr;
	QWidget* hostCam2_ = nullptr;
	QWidget* hostStitched_ = nullptr;

	// Halcon 窗口句柄 + 末帧缓存
	struct HalconWin {
		HalconCpp::HTuple hwnd;
		HalconCpp::HImage lastImage;
		QSize hostSize;
	};
	HalconWin winCam1_;
	HalconWin winCam2_;
	HalconWin winStitched_;

	// 相机帧缓冲（采集线程写入，UI 线程读取）
	cv::Mat rawMat1_;
	cv::Mat rawMat2_;
	HalconCpp::HImage cam1Image_;
	HalconCpp::HImage cam2Image_;
	bool cam1Ready_ = false;
	bool cam2Ready_ = false;
};
