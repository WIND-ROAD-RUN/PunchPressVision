#pragma once

#include <QWidget>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "halconcpp/HalconCpp.h"

namespace ui
{
	class HalconInteractiveLabel;

	/// 模型/形状编辑器（L3 复合控件）。
	/// 组合 HalconInteractiveLabel + 鼠标捕获叠加层，保留 L2 的缩放平移能力，
	/// 同时提供 ROI、Mask、中心点等绘制工具。
	///
	/// 支持多个 ROI 矩形，回撤移除最近添加的一个。
	class ShapeEditor : public QWidget
	{
		Q_OBJECT
	public:
		enum class Tool
		{
			View,          ///< 视图操纵：缩放/平移/复位
			RectangleROI,  ///< 矩形 ROI
			CenterPoint,   ///< 定义模板中心点
		};

		explicit ShapeEditor(QWidget* parent = nullptr);
		~ShapeEditor() override;

		/// 显示 Halcon 图像
		void displayImage(const HalconCpp::HImage& image);

		/// 切换当前工具
		void setTool(Tool tool);
		Tool tool() const { return tool_; }

		/// 将所有 ROI 合并为单个 Halcon 区域对象，未绘制时返回未初始化对象
		HalconCpp::HObject roi() const;

		/// 所有 ROI 矩形（图像坐标）
		QVector<QRectF> roiRects() const { return roiRects_; }
		int roiCount() const { return roiRects_.size(); }
		bool hasROI() const { return !roiRects_.empty(); }

		/// 当前中心点（图像坐标）
		QPointF centerPoint() const { return centerPoint_; }
		bool hasCenterPoint() const { return hasCenterPoint_; }

		/// 清除全部 ROI
		void clearROI();

		/// 回撤最近一个 ROI（LIFO）
		void undoROI();

		/// 清除中心点
		void clearCenterPoint();

		/// 全部清空（ROI + 中心点）
		void clearAll();

		/// 获取内部 L2 控件，用于连接 zoomChanged 等信号
		HalconInteractiveLabel* imageLabel() const { return imageLabel_; }

	signals:
		void roiChanged();
		void centerPointChanged();
		void toolChanged(Tool tool);

	protected:
		void resizeEvent(QResizeEvent* e) override;
		bool eventFilter(QObject* obj, QEvent* e) override;

	private:
		void refreshOverlay();        ///< 重新绘制全部 ROI / 中心点
		void drawAllROIs();           ///< 在 Halcon 窗口上绘制所有 ROI
		void drawCenterPoint();       ///< 在 Halcon 窗口上绘制中心点

		QPointF widgetToImage(const QPoint& widgetPos) const;

		HalconInteractiveLabel* imageLabel_{ nullptr };

		Tool tool_{ Tool::View };

		// ROI 绘制状态
		bool roiDrawing_{ false };
		QPoint roiStartWidget_;       // 拖拽起点（控件坐标）
		QPoint roiEndWidget_;         // 拖拽终点（控件坐标）
		QVector<QRectF> roiRects_;    // 已确认的全部 ROI（图像坐标）

		// 中心点
		QPointF centerPoint_;         // 图像坐标
		bool hasCenterPoint_{ false };

		// displayImage 重入哨兵：阻止 viewChanged → refreshOverlay 同步嵌套
		bool displaying_{ false };
	};
} // namespace ui
