#pragma once

#include <QWidget>

namespace ui
{
	/// 透明叠加绘制层，置于 HalconInteractiveLabel 之上。
	/// WA_TransparentForMouseEvents 使鼠标事件穿透到下层 L2 控件。
	/// 仅负责绘制：十字线、ROI 矩形框、测距线、自定义叠加对象。
	class OverlayWidget : public QWidget
	{
		Q_OBJECT
	public:
		explicit OverlayWidget(QWidget* parent = nullptr);

		// ---- 十字线 ----
		void setCrosshairVisible(bool visible);
		void setCrosshairPos(const QPoint& pos);

		// ---- ROI 矩形 ----
		void setROIRect(const QRect& rect);
		void clearROI();

		// ---- 测距线 ----
		void setMeasureLine(const QPoint& p1, const QPoint& p2, double pixelDist = -1);
		void clearMeasureLine();

		// ---- 叠加层总开关 ----
		void setOverlayVisible(bool visible);
		bool isOverlayVisible() const;

	protected:
		void paintEvent(QPaintEvent*) override;

	private:
		// 十字线
		bool crosshairVisible_{ false };
		QPoint crosshairPos_;

		// ROI
		bool roiVisible_{ false };
		QRect roiRect_;

		// 测距
		bool measureVisible_{ false };
		QPoint measureP1_;
		QPoint measureP2_;
		QString measureLabel_;

		// 总开关
		bool overlayVisible_{ true };
	};
} // namespace ui
