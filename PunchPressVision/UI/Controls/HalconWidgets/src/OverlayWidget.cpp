#include "UI/OverlayWidget.h"

#include <QPainter>
#include <QPainterPath>

namespace ui
{
	OverlayWidget::OverlayWidget(QWidget* parent)
		: QWidget(parent)
	{
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setAttribute(Qt::WA_TranslucentBackground, true);
		setAutoFillBackground(false);
	}

	// ---- 十字线 ---------------------------------------------------------------

	void OverlayWidget::setCrosshairVisible(bool visible)
	{
		if (crosshairVisible_ == visible)
			return;
		crosshairVisible_ = visible;
		update();
	}

	void OverlayWidget::setCrosshairPos(const QPoint& pos)
	{
		if (crosshairPos_ == pos)
			return;
		crosshairPos_ = pos;
		if (crosshairVisible_)
			update();
	}

	// ---- ROI 矩形 -------------------------------------------------------------

	void OverlayWidget::setROIRect(const QRect& rect)
	{
		roiVisible_ = true;
		roiRect_ = rect;
		update();
	}

	void OverlayWidget::clearROI()
	{
		if (!roiVisible_)
			return;
		roiVisible_ = false;
		roiRect_ = QRect();
		update();
	}

	// ---- 测距线 ---------------------------------------------------------------

	void OverlayWidget::setMeasureLine(const QPoint& p1, const QPoint& p2, double pixelDist)
	{
		measureVisible_ = true;
		measureP1_ = p1;
		measureP2_ = p2;
		if (pixelDist >= 0)
			measureLabel_ = QStringLiteral("%1 px").arg(pixelDist, 0, 'f', 1);
		else
			measureLabel_.clear();
		update();
	}

	void OverlayWidget::clearMeasureLine()
	{
		if (!measureVisible_)
			return;
		measureVisible_ = false;
		measureLabel_.clear();
		update();
	}

	// ---- 叠加层总开关 ---------------------------------------------------------

	void OverlayWidget::setOverlayVisible(bool visible)
	{
		if (overlayVisible_ == visible)
			return;
		overlayVisible_ = visible;
		update();
	}

	bool OverlayWidget::isOverlayVisible() const
	{
		return overlayVisible_;
	}

	// ---- 绘制 -----------------------------------------------------------------

	void OverlayWidget::paintEvent(QPaintEvent*)
	{
		if (!overlayVisible_)
			return;

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const int w = width();
		const int h = height();

		// --- 十字线 ---
		if (crosshairVisible_)
		{
			const QColor chColor(255, 255, 255, 100);
			p.setPen(QPen(chColor, 1, Qt::DashLine));
			// 水平线
			p.drawLine(0, crosshairPos_.y(), w, crosshairPos_.y());
			// 垂直线
			p.drawLine(crosshairPos_.x(), 0, crosshairPos_.x(), h);
		}

		// --- ROI 矩形 ---
		if (roiVisible_ && roiRect_.isValid() && !roiRect_.isEmpty())
		{
			p.setPen(QPen(QColor(0, 120, 215), 2));
			p.setBrush(QColor(0, 120, 215, 30));
			p.drawRect(roiRect_);
		}

		// --- 测距线 ---
		if (measureVisible_)
		{
			// 端点标记（小圆）
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(0, 255, 128));
			p.drawEllipse(measureP1_, 4, 4);
			p.drawEllipse(measureP2_, 4, 4);

			// 连线
			p.setPen(QPen(QColor(0, 255, 128), 2));
			p.setBrush(Qt::NoBrush);
			p.drawLine(measureP1_, measureP2_);

			// 距离标签（线段中点）
			if (!measureLabel_.isEmpty())
			{
				const QPoint mid = (measureP1_ + measureP2_) / 2;
				const QRect textRect(mid.x() + 8, mid.y() - 10, 120, 20);

				QPainterPath textBg;
				textBg.addRoundedRect(QRectF(textRect), 4, 4);
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(0, 0, 0, 160));
				p.drawPath(textBg);

				p.setPen(QColor(0, 255, 128));
				p.setFont(QFont(QStringLiteral("Segoe UI"), 9));
				p.drawText(textRect, Qt::AlignCenter, measureLabel_);
			}
		}
	}
} // namespace ui
