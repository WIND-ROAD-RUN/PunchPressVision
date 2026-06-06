#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QSize>
#include <QStackedWidget>

#include <atomic>
#include <mutex>

#include <opencv2/core/mat.hpp>

#include <rqwccore/rqwc_types.hpp>

#include "JiudianParam.hpp"
#include "ProcessModule.hpp"
#include "ImageStitcchingParam.hpp"
#include "ui_DlgJiudianbiaoding.h"

#include <QPoint>

namespace HalconCpp
{
	class HImage;
	class HTuple;
	class HObject;
}

QT_BEGIN_NAMESPACE
namespace Ui
{
	class DlgJiudianbiaodingClass;
}
QT_END_NAMESPACE

class DlgJiudianbiaoding : public QDialog
{
	Q_OBJECT

public:
	DlgJiudianbiaoding(QWidget* parent = nullptr);
	~DlgJiudianbiaoding();
private:
	void build_ui();
	void build_connect();
public:
	bool isJiuDianBiaoDing{ false };

public slots:
	void onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index);

protected:
	void showEvent(QShowEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	void closeEvent(QCloseEvent*) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void btn_readImage_clicked();

	void btn_paintRegion_clicked();
	void btn_findRectangle_clicked();
	void clearJiudianResults();
	void btn_jiudianbiaoding_clicked();

	void btn_exit();
	void btn_wanchengjiudianbiaoding_clicked();
	void btn_baoguang1_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang2_clicked();
	void btn_zengyi2_clicked();
	void btn_remove_clicked();
	void btn_removeAll_clicked();

	void btn_MeasureLength1_clicked();
	void btn_MeasureLength2_clicked();
	void btn_Threshold_clicked();
	void btn_num_measures_clicked();

	void btn_startCamera1_clicked();
	void btn_stopCamera1_clicked();
	void btn_startCamera2_clicked();
	void btn_stopCamera2_clicked();

	void onJiuDianImageTableCellClicked(int row, int column);

private:
	enum class FindRectangleDisplay
	{
		SearchRectangleBox, // 只显示"查找矩形框"(测量框)
		FoundRectangle      // 只显示"找到的矩形"(测量结果轮廓)
	};

	// 矩形检测结果结构体
	struct RectangleResult
	{
		double x{ 0.0 };  // 列坐标 (Col)
		double y{ 0.0 };  // 行坐标 (Row)
		HalconCpp::HObject* contour = nullptr;  // 轮廓对象（指针，需外部管理生命周期）
		bool valid{ false };  // 是否有效
	};

private:
	void readImage();

	void load_jiuDianParam();

	struct HalconViewPart
	{
		double r1{ 0.0 };
		double c1{ 0.0 };
		double r2{ 0.0 };
		double c2{ 0.0 };
	};

	bool ensureHalconWindow();
	void closeHalconWindow();
	void redrawHalconView(bool clearWindow);

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

	bool createShapeModelFromRois();
	void createShapeModel();
	void setRoiEditingUiEnabled(bool enabled);
	void paintCreateRegion();

	// 查找矩形（支持多目标，最多3个）
	bool findRectangle(const HalconCpp::HImage& image, QVector<RectangleResult>& results, FindRectangleDisplay display, bool showMessage = true, int maxCount = 3);

	// 往ListView中添加内容（5列：index / pixX / pixY / realX / realY）
	void addNewItemToListView(double pixX, double pixY, double realX, double realY, bool checked);
	void clearJiudianImages();

private:
	Ui::DlgJiudianbiaodingClass* ui = nullptr;

private:
	QWidget* _halconHost = nullptr;
	QSize _labelImgDisplaySize;

	HalconCpp::HTuple* _halconWindowHandle = nullptr;
	HalconCpp::HImage* _halconLastImage = nullptr;

	// genImageToWorldPlaneMap 执行期间：丢弃后续 onCameraDisplay
	std::atomic_flag _genMapRunning = ATOMIC_FLAG_INIT;
	QFutureWatcher<void> _cameraDisplayWatcher;

private:
	JiudianParam _jiudianParam;
	ProcessModule::RoiPurpose _roiPurpose{ ProcessModule::RoiPurpose::None };
	ProcessModule _ProcessModule;

private:
	bool isShowImg{ true };
	int listViewCount{ 0 };
	bool _showPaintCreateRoiObj{ true };
	bool lastIsCamera1SoftTrigger{ false };
	bool lastIsCamera2SoftTrigger{ false };

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
};
