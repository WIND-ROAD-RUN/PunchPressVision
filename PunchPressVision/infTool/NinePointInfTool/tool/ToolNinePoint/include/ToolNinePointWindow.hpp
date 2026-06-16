#pragma once

#include <memory>
#include <vector>

#include <QMainWindow>
#include <QFutureWatcher>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "rwul/hoecm/hoec_m.hpp"
#include "infTool/NinePointInfTool/NinePointInfTool.hpp"

namespace inf { class infrastructure; }
namespace infTool { class NinePointInfTool; }

QT_BEGIN_NAMESPACE
namespace Ui { class ToolNinePointWindowClass; }
QT_END_NAMESPACE

class ToolNinePointWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit ToolNinePointWindow(inf::infrastructure& inf, QWidget* parent = nullptr);
	~ToolNinePointWindow() override;

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void closeEvent(QCloseEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

signals:
	/// 请求 UI 线程刷新 Halcon 显示
	void displayFrameReady();

private slots:
	/// 接收相机原始帧（DirectConnection，采集线程）
	void onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex);
	/// UI 线程刷新显示（QueuedConnection）
	void onDisplayFrame();

	// UI 按钮槽函数
	void btn_readImage_clicked();
	void btn_paintRegion_clicked();
	void btn_findRectangle_clicked();
	void btn_ninePointCalibration_clicked();
	void btn_completeCalibration_clicked();
	void btn_exit_clicked();

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

	void onNinePointTableClicked(int row, int column);

private:
	void buildUi();
	void buildConnections();
	void syncConfigToUi();
	void applyCalibParams();
	void readImage();

	// Halcon 窗口管理
	bool ensureHalconWindow();
	void closeHalconWindow();
	void redrawHalconView(bool clearWindow);

	// 从 QLabel 占位符创建 native host 承载 Halcon 窗口
	QWidget* replaceLabel(const QString& labelName);

	// Halcon: 绘制 ROI + 创建模板 + 查找矩形
	void paintCreateRegion();
	void createShapeModel();
	bool createShapeModelFromRois();
	void setRoiEditingUiEnabled(bool enabled);

	// 查找矩形（支持多目标，最多3个）
	struct RectangleResult
	{
		double x{ 0.0 };
		double y{ 0.0 };
		HalconCpp::HObject* contour = nullptr;
		bool valid{ false };
	};
	bool findRectangle(const HalconCpp::HImage& image,
		QVector<RectangleResult>& results,
		bool showMessage = true, int maxCount = 3);

	// 表格操作
	void addNewItemToListView(double pixX, double pixY, double realX, double realY);
	void clearNinePointImages();
	void clearNinePointResults();

	// 缩放/拖拽
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

private:
	inf::infrastructure& inf_;
	std::unique_ptr<infTool::NinePointInfTool> ninePointTool_;

	Ui::ToolNinePointWindowClass* ui = nullptr;

	// Halcon 显示宿主
	QWidget* hostDisplay_ = nullptr;
	QSize _labelImgDisplaySize;
	HalconCpp::HTuple halconWindowHandle_;
	HalconCpp::HImage halconLastImage_;

	// 模板/ROI 相关
	HalconCpp::HObject* paintCreateRoiObj_ = nullptr;
	HalconCpp::HObject* findCreateXldObj_ = nullptr;
	HalconCpp::HTuple* hv_ModelID_ = nullptr;
	HalconCpp::HTuple* hv_MetrologyHandle_ = nullptr;

	HalconViewPart viewPart_{};
	bool viewPartValid_{ false };
	int viewImgW_{ 0 };
	int viewImgH_{ 0 };
	bool isPanning_{ false };
	QPoint panStartPos_{};
	HalconViewPart panStartPart_{};

	bool showPaintCreateRoiObj_{ true };
	bool isShowImg_{ true };
	int listViewCount_{ 0 };
	bool isJiuDianBiaoDing_{ false };

	// 九点采集数据缓存
	std::vector<infTool::Point2D> pixPoints_;
	std::vector<infTool::Point2D> worldPoints_;

	// 相机帧缓冲（采集线程写入，UI 线程读取）
	cv::Mat rawMat1_;
	cv::Mat rawMat2_;
	HalconCpp::HImage cam1Image_;
	HalconCpp::HImage cam2Image_;
	bool cam1Ready_{ false };
	bool cam2Ready_{ false };

	// 显示模式
	enum class FindRectangleDisplay
	{
		SearchRectangleBox,
		FoundRectangle
	};

	QFutureWatcher<void> cameraDisplayWatcher_;
	bool lastCamera1IsSoftTrigger_{ false };
	bool lastCamera2IsSoftTrigger_{ false };

	static HalconCpp::HImage cvMatToHImage(const cv::Mat& mat);
};
