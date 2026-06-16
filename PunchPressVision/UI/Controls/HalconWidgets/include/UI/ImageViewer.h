#pragma once

#include <QWidget>

#include "halconcpp/HalconCpp.h"

class QLabel;

namespace ui
{
	class HalconInteractiveLabel;
	class OverlayWidget;

	/// 全功能图像查看器，组合 HalconInteractiveLabel + 叠加绘制层 + 信息标签。
	///
	/// 模式（互斥）：
	///   Normal     – 纯视图操纵（缩放/平移/复位）
	///   Crosshair  – 十字准星 + 坐标/灰度实时显示
	///   ROI        – 左键拖拽框选矩形，释放时发射 roiSelected
	///   Measurement– Shift+左键依次取两点，发射 measurementReady
	///
	/// 快捷键：
	///   Ctrl+0/Ctrl+1  → 适应窗口 / 1:1（委托给 HalconInteractiveLabel）
	///   Ctrl+S         → 保存截图
	class ImageViewer : public QWidget
	{
		Q_OBJECT
	public:
		enum class Mode
		{
			Normal,
			Crosshair,
			ROI,
			Measurement,
		};

		explicit ImageViewer(QWidget* parent = nullptr);

		/// 显示 Halcon 图像（委托给 HalconInteractiveLabel）
		void displayImage(const HalconCpp::HImage& image);

		/// 设置工作模式
		void setMode(Mode mode);
		Mode mode() const;

		/// 快捷开关：十字线模式
		void setCrosshairEnabled(bool on);

		/// 快捷开关：ROI 框选模式
		void setROIMode(bool on);

		/// 获取当前 ROI（图像坐标），无 ROI 时返回未初始化对象
		HalconCpp::HObject currentROI() const;

		/// 设置像素→世界坐标转换系数（mm/pixel），设 0 时不转换
		void setPixelToWorldRatio(double mmPerPixel);

		// ---- 叠加层管理 ----
		void addOverlay(const HalconCpp::HObject& obj, const QString& label);
		void clearOverlays();
		void setOverlayVisible(bool visible);

		/// 获取内部 HalconInteractiveLabel（用于外部连接 zoomChanged 等信号）
		HalconInteractiveLabel* imageLabel() const;

	signals:
		/// ROI 框选完成，矩形为图像坐标 (col, row)
		void roiSelected(QRectF imageROI);

		/// 双点测距完成
		void measurementReady(double pixelDist, double worldDist_mm);

		/// 鼠标在图像上移动时发射（仅 Crosshair / ROI / Measurement 模式）
		void mousePosChanged(int row, int col, int grayValue);

	protected:
		void resizeEvent(QResizeEvent*) override;
		bool eventFilter(QObject* obj, QEvent* e) override;

	private:
		void updateZoomLabel();
		void updateCoordLabel(QPoint widgetPos);
		int  grayValueAt(int row, int col) const;
		QPointF widgetToImage(const QPointF& widgetPos) const;

		void startROI(QPoint widgetPos);
		void updateROI(QPoint widgetPos);
		void finishROI();

		void placeMeasurePoint(QPoint widgetPos);
		void updateMeasurePreview(QPoint widgetPos);
		void finishMeasurement();

		void saveScreenshot();

		HalconInteractiveLabel* imageLabel_;
		OverlayWidget* drawingOverlay_;
		QLabel* zoomLabel_;
		QLabel* coordLabel_;

		Mode mode_{ Mode::Normal };
		double mmPerPixel_{ 0.0 };

		// ROI 拖拽状态
		bool roiDragging_{ false };
		QPoint roiStart_;
		QRect roiRect_;

		// 测距状态
		bool measureActive_{ false };
		QPoint measureP1_;
		QPoint measureP2_;
		bool measureHasP1_{ false };
	};
} // namespace ui
