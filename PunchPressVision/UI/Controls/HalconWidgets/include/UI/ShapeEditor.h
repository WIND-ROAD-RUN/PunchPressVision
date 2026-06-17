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
	/// 二期：支持多 ROI / 多 Mask 绘制，统一回撤栈。
	class ShapeEditor : public QWidget
	{
		Q_OBJECT
	public:
		enum class Tool
		{
			View,           ///< 视图操纵：缩放/平移/复位
			RectangleROI,   ///< 感兴趣区域（绿色矩形）
			FreehandROI,    ///< 感兴趣区域（Halcon 自由绘制）
			RectangleMask,  ///< 屏蔽区域（品红色矩形）
			FreehandMask,   ///< 屏蔽区域（Halcon 自由绘制）
			CenterPoint,    ///< 定义模板中心点
		};

		/// 操作类型，用于统一回撤栈
		enum class ActionType : uint8_t
		{
			ROI,   ///< 感兴趣区域
			Mask,  ///< 屏蔽区域
		};

		explicit ShapeEditor(QWidget* parent = nullptr);
		~ShapeEditor() override;

		/// 显示 Halcon 图像
		void displayImage(const HalconCpp::HImage& image);

		/// 切换当前工具
		void setTool(Tool tool);
		Tool tool() const { return tool_; }

		// === ROI ===

		/// 将所有 ROI 合并为单个 Halcon 区域对象，未绘制时返回未初始化对象
		HalconCpp::HObject roi() const;
		/// ROI 区域对象列表（逐个保存，支持回撤）
		std::vector<HalconCpp::HObject> roiObjects() const { return roiObjects_; }
		int roiCount() const { return static_cast<int>(roiObjects_.size()); }
		bool hasROI() const { return !roiObjects_.empty(); }

		// === Mask ===

		/// 将所有 Mask 合并为单个 Halcon 区域对象，未绘制时返回未初始化对象
		HalconCpp::HObject mask() const;
		/// Mask 区域对象列表（逐个保存，支持回撤）
		std::vector<HalconCpp::HObject> maskObjects() const { return maskObjects_; }
		int maskCount() const { return static_cast<int>(maskObjects_.size()); }
		bool hasMask() const { return !maskObjects_.empty(); }

		// === 中心点 ===

		QPointF centerPoint() const { return centerPoint_; }
		bool hasCenterPoint() const { return hasCenterPoint_; }

		// === 编辑操作 ===

		/// 统一回撤：移除最近一次操作（ROI 或 Mask）
		void undo();

		/// 清除全部 ROI
		void clearROI();

		/// 清除全部 Mask
		void clearMask();

		/// 清除中心点
		void clearCenterPoint();

		/// 全部清空（ROI + Mask + 中心点 + 历史）
		void clearAll();

		/// 绘制识别匹配结果标记（绿色十字 + 分数），row/col 为图像坐标
		void drawRecognitionMarker(double row, double col, double angle, double score);

		/// 清除识别标记
		void clearMarker();

		/// 绘制模型轮廓（创建模型成功后显示）
		void drawModelContours(const HalconCpp::HObject& contours);

		/// 清除模型轮廓
		void clearModelContours();

		/// 获取内部 L2 控件，用于连接 zoomChanged 等信号
		HalconInteractiveLabel* imageLabel() const { return imageLabel_; }

	signals:
		void roiChanged();
		void maskChanged();
		void centerPointChanged();
		void toolChanged(Tool tool);

	protected:
		void resizeEvent(QResizeEvent* e) override;
		bool eventFilter(QObject* obj, QEvent* e) override;

	private:
		void refreshOverlay();
		void drawAllROIs();
		void drawAllMasks();
		void drawCenterPoint();
		void drawMarker();             ///< 绘制识别匹配结果标记
		void drawFoundContours();      ///< 绘制模型轮廓

		/// 生成单个矩形区域对象
		static HalconCpp::HObject rectToRegion(const QRectF& r);

		/// 合并区域对象列表为一个区域对象
		static HalconCpp::HObject mergeObjects(const std::vector<HalconCpp::HObject>& objects);

		/// 使用 Halcon 在窗口中自由绘制一个区域
		static HalconCpp::HObject drawFreehandRegion(const HalconCpp::HTuple& windowHandle);

		QPointF widgetToImage(const QPoint& widgetPos) const;

		HalconInteractiveLabel* imageLabel_{ nullptr };

		Tool tool_{ Tool::View };

		// 拖拽状态
		bool drawing_{ false };
		QPoint drawStartWidget_;
		QPoint drawEndWidget_;

		// 数据
		std::vector<HalconCpp::HObject> roiObjects_;
		std::vector<HalconCpp::HObject> maskObjects_;
		QVector<ActionType> actionHistory_;  // 统一回撤栈

		QPointF centerPoint_;
		bool hasCenterPoint_{ false };

		// 识别标记
		bool showMarker_{ false };
		double markerRow_{ 0.0 }, markerCol_{ 0.0 }, markerAngle_{ 0.0 }, markerScore_{ 0.0 };

		// 模型轮廓
		HalconCpp::HObject modelContours_;
		bool showModelContours_{ false };

		// displayImage 重入哨兵
		bool displaying_{ false };
	};
} // namespace ui
