#include "UI/HalconInteractiveLabel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include <QtMath>

namespace
{
	/// 透明覆盖层控件，置于 Halcon 原生窗口之上以捕获 Qt 鼠标/键盘事件。
	/// 在 Ctrl+左键选框放大时绘制半透明选框。
	class OverlayWidget : public QWidget
	{
	public:
		using QWidget::QWidget;

		void setSelectionRect(const QRect& r)
		{
			selRect_ = r;
			update();
		}

	protected:
		void paintEvent(QPaintEvent*) override
		{
			if (selRect_.isValid() && !selRect_.isEmpty())
			{
				QPainter p(this);
				p.setPen(QPen(QColor(0, 120, 215), 1.5));
				p.setBrush(QColor(0, 120, 215, 40));
				p.drawRect(selRect_);
			}
		}

	private:
		QRect selRect_;
	};
} // namespace

namespace ui
{
	HalconInteractiveLabel::HalconInteractiveLabel(QWidget* parent)
		: HalconDisplayLabel(parent)
	{
		auto* ow = new OverlayWidget(this);
		ow->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		ow->setMouseTracking(true);
		ow->setFocusPolicy(Qt::StrongFocus);
		ow->installEventFilter(this);
		ow->setCursor(Qt::ArrowCursor);
		overlay_ = ow;
	}

	HalconInteractiveLabel::~HalconInteractiveLabel() = default;

	// ---- 公共 API ----------------------------------------------------------------

	void HalconInteractiveLabel::resetView()
	{
		zoomToFit();
	}

	void HalconInteractiveLabel::zoomToFit()
	{
		zoomLevel_ = fitZoomLevel();
		if (lastImage_.IsInitialized())
		{
			HalconCpp::HTuple iw, ih;
			lastImage_.GetImageSize(&iw, &ih);
			viewCenter_ = QPointF(static_cast<double>(iw[0].I()) / 2.0,
			                       static_cast<double>(ih[0].I()) / 2.0);
		}
		clampViewCenter();
		applyView();
		emit zoomChanged(zoomLevel_);
		emit viewChanged();
	}

	void HalconInteractiveLabel::zoomToActual()
	{
		if (!lastImage_.IsInitialized())
			return;

		zoomLevel_ = 1.0;
		clampViewCenter();
		applyView();
		emit zoomChanged(zoomLevel_);
		emit viewChanged();
		viewInitialized_ = true;
	}

	double HalconInteractiveLabel::currentZoom() const
	{
		return zoomLevel_;
	}

	QPointF HalconInteractiveLabel::imagePosAt(const QPointF& widgetPos) const
	{
		const int lw = width();
		const int lh = height();
		if (lw <= 0 || lh <= 0)
			return viewCenter_;

		const QPointF visSize = visibleImageSize();
		const double row1 = viewCenter_.y() - visSize.y() / 2.0;
		const double col1 = viewCenter_.x() - visSize.x() / 2.0;

		const double imgCol = col1 + (widgetPos.x() / lw) * visSize.x();
		const double imgRow = row1 + (widgetPos.y() / lh) * visSize.y();

		return QPointF(imgCol, imgRow);
	}

	// ---- Qt 事件 ----------------------------------------------------------------

	void HalconInteractiveLabel::showEvent(QShowEvent* e)
	{
		HalconDisplayLabel::showEvent(e);
		if (overlay_)
		{
			overlay_->resize(size());
			overlay_->raise();
		}
	}

	void HalconInteractiveLabel::resizeEvent(QResizeEvent* e)
	{
		HalconDisplayLabel::resizeEvent(e);
		// 基类 resizeEvent 已通过虚函数 dispatch 调用 displayImage 处理视图更新。
		// 此处仅同步覆盖层尺寸与 z-order。
		if (overlay_)
		{
			overlay_->resize(e->size());
			overlay_->raise();
		}
	}

	void HalconInteractiveLabel::displayImage(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;

		if (!image.IsInitialized())
			return;

		ensureWindow();
		if (!windowCreated_)
			return;

		syncWindowSize();

		// 提升覆盖层至 Halcon 原生窗口之上
		if (overlay_)
			overlay_->raise();

		// 检测是否为新图像（尺寸变化视为新图像）
		bool isNewImage = !lastImage_.IsInitialized();
		if (!isNewImage)
		{
			HTuple oldW, oldH, newW, newH;
			lastImage_.GetImageSize(&oldW, &oldH);
			image.GetImageSize(&newW, &newH);
			isNewImage = (oldW[0].I() != newW[0].I()) || (oldH[0].I() != newH[0].I());
		}

		lastImage_ = image;

		if (isNewImage || !viewInitialized_)
		{
			// 新图像首次到达 → 适应窗口
			const double prevZoom = zoomLevel_;
			zoomLevel_ = fitZoomLevel();
			HTuple iw, ih;
			image.GetImageSize(&iw, &ih);
			viewCenter_ = QPointF(static_cast<double>(iw[0].I()) / 2.0,
			                       static_cast<double>(ih[0].I()) / 2.0);
			clampViewCenter();
			applyView();
			if (!qFuzzyCompare(prevZoom, zoomLevel_))
				emit zoomChanged(zoomLevel_);
			emit viewChanged();
			viewInitialized_ = true;
		}
		else
		{
			applyView();
		}
	}

	bool HalconInteractiveLabel::eventFilter(QObject* obj, QEvent* e)
	{
		if (obj != overlay_)
			return HalconDisplayLabel::eventFilter(obj, e);

		switch (e->type())
		{
		// -- 鼠标进入：自动获取焦点以启用键盘快捷键 -------------------------
		case QEvent::Enter:
			overlay_->setFocus();
			break;

		// -- 滚轮缩放 ----------------------------------------------------------
		case QEvent::Wheel:
		{
			auto* we = static_cast<QWheelEvent*>(e);
			const double steps = we->angleDelta().y() / 120.0;
			const double factor = qPow(1.15, steps);
			const QPointF anchorImg = imagePosAt(we->position());
			const double newZoom = qBound(fitZoomLevel() * 0.5,
			                              zoomLevel_ * factor, 32.0);
			setZoomLevel(newZoom, anchorImg);
			emit zoomChanged(zoomLevel_);
			emit viewChanged();
			viewInitialized_ = true;
			return true;
		}

		// -- 鼠标按下 ----------------------------------------------------------
		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			overlay_->setFocus();

			if (me->button() == Qt::RightButton)
			{
				resetView();
				return true;
			}

			if (me->button() == Qt::LeftButton)
			{
				if (me->modifiers() & Qt::ControlModifier)
				{
					// Ctrl+左键 → 选框放大
					isSelecting_ = true;
					selStartPos_ = me->pos();
					selRect_ = QRect();
					overlay_->setCursor(Qt::CrossCursor);
				}
				else
				{
					// 左键 → 平移
					isPanning_ = true;
					lastMousePos_ = me->pos();
					panStartCenter_ = viewCenter_;
					overlay_->setCursor(Qt::ClosedHandCursor);
				}
				return true;
			}
			break;
		}

		// -- 鼠标移动 ----------------------------------------------------------
		case QEvent::MouseMove:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (isPanning_)
			{
				const QPoint delta = me->pos() - lastMousePos_;
				const QPointF visSize = visibleImageSize();
				const int lw = width();
				const int lh = height();
				if (lw > 0 && lh > 0)
				{
					const double imgDx = -delta.x() * visSize.x() / lw;
					const double imgDy = -delta.y() * visSize.y() / lh;
					viewCenter_ = QPointF(panStartCenter_.x() + imgDx,
					                       panStartCenter_.y() + imgDy);
					clampViewCenter();
					applyView();
					emit viewChanged();
				}
			}
			else if (isSelecting_)
			{
				selRect_ = QRect(selStartPos_, me->pos()).normalized();
				static_cast<OverlayWidget*>(overlay_)->setSelectionRect(selRect_);
			}
			else
			{
				// 悬停时根据缩放状态更新光标
				const double fit = fitZoomLevel();
				overlay_->setCursor(zoomLevel_ > fit + 0.001
				                        ? Qt::OpenHandCursor
				                        : Qt::ArrowCursor);
			}
			return true;
		}

		// -- 鼠标释放 ----------------------------------------------------------
		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (me->button() == Qt::LeftButton)
			{
				if (isSelecting_ && selRect_.isValid()
				    && selRect_.width() > 5 && selRect_.height() > 5)
				{
					// 选框局部放大
					const QPointF topLeft = imagePosAt(selRect_.topLeft());
					const QPointF bottomRight = imagePosAt(selRect_.bottomRight());
					const double selW = std::abs(bottomRight.x() - topLeft.x());
					const double selH = std::abs(bottomRight.y() - topLeft.y());

					if (selW > 0 && selH > 0)
					{
						const double zoomW = width() / selW;
						const double zoomH = height() / selH;
						const double newZoom = qBound(fitZoomLevel(),
						                              qMin(zoomW, zoomH), 32.0);
						viewCenter_ = QPointF(
							(topLeft.x() + bottomRight.x()) / 2.0,
							(topLeft.y() + bottomRight.y()) / 2.0);
						zoomLevel_ = newZoom;
						clampViewCenter();
						applyView();
						emit zoomChanged(zoomLevel_);
						emit viewChanged();
					}
				}

				isSelecting_ = false;
				isPanning_ = false;
				static_cast<OverlayWidget*>(overlay_)->setSelectionRect(QRect());
				selRect_ = QRect();

				const double fit = fitZoomLevel();
				overlay_->setCursor(zoomLevel_ > fit + 0.001
				                        ? Qt::OpenHandCursor
				                        : Qt::ArrowCursor);
				return true;
			}
			break;
		}

		// -- 键盘快捷键 -------------------------------------------------------
		case QEvent::KeyPress:
		{
			auto* ke = static_cast<QKeyEvent*>(e);
			if (ke->modifiers() == Qt::ControlModifier)
			{
				if (ke->key() == Qt::Key_0)
				{
					zoomToFit();
					return true;
				}
				if (ke->key() == Qt::Key_1)
				{
					zoomToActual();
					return true;
				}
			}
			break;
		}

		default:
			break;
		}

		return HalconDisplayLabel::eventFilter(obj, e);
	}

	// ---- 内部实现 ----------------------------------------------------------------

	void HalconInteractiveLabel::applyView()
	{
		using namespace HalconCpp;

		if (!windowCreated_ || !lastImage_.IsInitialized())
			return;

		const int lw = width();
		const int lh = height();
		if (lw <= 0 || lh <= 0)
			return;

		const QPointF visSize = visibleImageSize();
		const double visW = visSize.x();
		const double visH = visSize.y();

		const int row1 = static_cast<int>(viewCenter_.y() - visH / 2.0);
		const int col1 = static_cast<int>(viewCenter_.x() - visW / 2.0);
		const int row2 = row1 + static_cast<int>(visH) - 1;
		const int col2 = col1 + static_cast<int>(visW) - 1;

		try
		{
			SetPart(handle_, row1, col1, row2, col2);
			ClearWindow(handle_);
			DispObj(lastImage_, handle_);
		}
		catch (...) {}
	}

	void HalconInteractiveLabel::setZoomLevel(double zoom, const QPointF& anchorImagePos)
	{
		const double oldZoom = zoomLevel_;
		zoomLevel_ = qBound(fitZoomLevel() * 0.5, zoom, 32.0);

		if (qFuzzyCompare(oldZoom, zoomLevel_))
			return;

		// 保持锚点（图像坐标）在控件中的位置不变:
		//   newCenter = anchor + (oldCenter - anchor) * (oldZoom / newZoom)
		const double ratio = oldZoom / zoomLevel_;
		viewCenter_.setX(anchorImagePos.x() + (viewCenter_.x() - anchorImagePos.x()) * ratio);
		viewCenter_.setY(anchorImagePos.y() + (viewCenter_.y() - anchorImagePos.y()) * ratio);

		clampViewCenter();
		applyView();
	}

	double HalconInteractiveLabel::fitZoomLevel() const
	{
		if (!lastImage_.IsInitialized())
			return 1.0;

		HalconCpp::HTuple iw, ih;
		lastImage_.GetImageSize(&iw, &ih);
		const double imgW = static_cast<double>(iw[0].I());
		const double imgH = static_cast<double>(ih[0].I());

		const int lw = width();
		const int lh = height();
		if (lw <= 0 || lh <= 0 || imgW <= 0 || imgH <= 0)
			return 1.0;

		return qMin(static_cast<double>(lw) / imgW, static_cast<double>(lh) / imgH);
	}

	void HalconInteractiveLabel::clampViewCenter()
	{
		if (!lastImage_.IsInitialized())
			return;

		HalconCpp::HTuple iw, ih;
		lastImage_.GetImageSize(&iw, &ih);
		const double imgW = static_cast<double>(iw[0].I());
		const double imgH = static_cast<double>(ih[0].I());

		const QPointF visSize = visibleImageSize();
		const double halfVisW = visSize.x() / 2.0;
		const double halfVisH = visSize.y() / 2.0;

		// 可见区域超出图像时居中；否则约束边界
		if (halfVisW * 2.0 >= imgW)
			viewCenter_.setX(imgW / 2.0);
		else
			viewCenter_.setX(qBound(halfVisW, viewCenter_.x(), imgW - halfVisW));

		if (halfVisH * 2.0 >= imgH)
			viewCenter_.setY(imgH / 2.0);
		else
			viewCenter_.setY(qBound(halfVisH, viewCenter_.y(), imgH - halfVisH));
	}

	QPointF HalconInteractiveLabel::visibleImageSize() const
	{
		const int lw = width();
		const int lh = height();
		if (lw <= 0 || lh <= 0 || zoomLevel_ <= 0.0)
			return QPointF(1.0, 1.0);
		return QPointF(lw / zoomLevel_, lh / zoomLevel_);
	}
} // namespace ui
