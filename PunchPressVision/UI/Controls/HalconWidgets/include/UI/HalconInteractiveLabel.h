#pragma once

#include "UI/HalconDisplayLabel.h"

#include <QPointF>

class QWidget;

namespace ui
{
	/// 交互式 Halcon 图像显示控件（继承 HalconDisplayLabel）。
	/// 在纯显示基础上增加视图操纵能力：
	///   - 滚轮缩放（以鼠标指针位置为中心）
	///   - 左键拖拽平移
	///   - 右键复位到完整视图
	///   - Ctrl+0: 适应窗口  Ctrl+1: 1:1 实际像素
	///   - Ctrl+左键拖拽: 选框局部放大
	///
	/// 使用透明覆盖层捕获鼠标事件，通过 eventFilter 统一处理。
	class HalconInteractiveLabel : public HalconDisplayLabel
	{
		Q_OBJECT
	public:
		explicit HalconInteractiveLabel(QWidget* parent = nullptr);
		~HalconInteractiveLabel() override;

		/// 复位到完整显示（等同 Fit to Window）
		void resetView();

		/// 缩放至图像恰好填满控件
		void zoomToFit();

		/// 1:1 实际像素映射
		void zoomToActual();

		/// 当前缩放比例（图像像素 / 屏幕像素，1.0 = 1:1）
		double currentZoom() const;

		/// 控件坐标 → 图像坐标 (col, row)
		QPointF imagePosAt(const QPointF& widgetPos) const;

		/// 允许外部（如 ImageViewer）在内部覆盖层上安装额外的事件过滤器，
		/// 用于跟踪鼠标坐标、拦截特定模式下的鼠标事件等。
		void installOverlayEventFilter(QObject* filterObj);

		/// 显示 Halcon 图像（首次自动 fit，后续保持用户视图）
		void displayImage(const HalconCpp::HImage& image) override;

	signals:
		/// 缩放比例变化时发射
		void zoomChanged(double zoom);

		/// 视图（缩放/平移）变化时发射
		void viewChanged();

	protected:
		void showEvent(QShowEvent* e) override;
		void resizeEvent(QResizeEvent* e) override;
		bool eventFilter(QObject* obj, QEvent* e) override;

	private:
		/// 将当前 zoomLevel_ / viewCenter_ 应用到 Halcon SetPart 并刷新显示
		void applyView();

		/// 设置缩放级别，anchorImagePos 为缩放锚点（图像坐标），缩放后该点保持在控件同一位置
		void setZoomLevel(double zoom, const QPointF& anchorImagePos);

		/// 当前控件尺寸下完整显示图像所需的缩放比例
		double fitZoomLevel() const;

		/// 约束 viewCenter_ 不超出图像边界
		void clampViewCenter();

		/// 当前可见图像区域尺寸 (col, row)
		QPointF visibleImageSize() const;

		/// 在当前视图上绘制选框（Halcon 原生绘制，每帧先 applyView 擦除旧框再画新框）
		void drawSelectionBox(const QPoint& startWidget, const QPoint& endWidget);

		// 覆盖层（透明 QWidget，捕获 Halcon 原生窗口无法传递的 Qt 鼠标事件）
		QWidget* overlay_{ nullptr };

		double zoomLevel_{ 1.0 };   // 1.0 = 1:1 (图像像素 : 屏幕像素)
		QPointF viewCenter_;        // 视口中心在图像坐标系中的位置 (col, row)

		// 拖拽平移状态
		bool isPanning_{ false };
		QPoint lastMousePos_;
		QPointF panStartCenter_;

		// Ctrl+左键 选框放大状态
		bool isSelecting_{ false };
		QPoint selStartPos_;

		/// 是否已经过用户交互（首次图像到来时自动 fit）
		bool viewInitialized_{ false };
	};
}
