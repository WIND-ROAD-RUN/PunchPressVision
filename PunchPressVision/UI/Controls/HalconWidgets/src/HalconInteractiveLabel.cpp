#include "UI/HalconInteractiveLabel.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include <QtMath>

namespace
{
	/// 透明覆盖层控件，置于 Halcon 原生窗口之上以捕获 Qt 鼠标/键盘事件。
	/// 必须设置 WA_NativeWindow 创建独立 HWND，方可 z-order 到 Halcon 原生窗口之上。
	/// 本身不做任何绘制（选区矩形由 Halcon 原生绘制，见 drawSelectionBox）。
	class OverlayWidget : public QWidget
	{
	public:
		explicit OverlayWidget(QWidget* parent)
			: QWidget(parent)
		{
			setAttribute(Qt::WA_NativeWindow, true);
			setAutoFillBackground(false);
		}
	};
} // namespace

namespace ui
{
	HalconInteractiveLabel::HalconInteractiveLabel(QWidget* parent)
		: HalconDisplayLabel(parent)
	{
		auto* ow = new OverlayWidget(this);
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

	void HalconInteractiveLabel::installOverlayEventFilter(QObject* filterObj)
	{
		if (overlay_)
			overlay_->installEventFilter(filterObj);
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

		if (overlay_)
			overlay_->raise();

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
				drawSelectionBox(selStartPos_, me->pos());
			}
			else
			{
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
				if (isSelecting_)
				{
					const QRect sel = QRect(selStartPos_, me->pos()).normalized();
					if (sel.width() > 5 && sel.height() > 5)
					{
						// 选框局部放大
						const QPointF topLeft = imagePosAt(QPointF(sel.topLeft()));
						const QPointF bottomRight = imagePosAt(QPointF(sel.bottomRight()));
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
							// applyView 已清除选框，跳过末尾的 applyView
							isSelecting_ = false;
							isPanning_ = false;

							const double fit = fitZoomLevel();
							overlay_->setCursor(zoomLevel_ > fit + 0.001
							                        ? Qt::OpenHandCursor
							                        : Qt::ArrowCursor);
							return true;
						}
					}

					// 选框太小或无效 → 仅清除选框，恢复干净图像
					isSelecting_ = false;
					applyView();

					const double fit = fitZoomLevel();
					overlay_->setCursor(zoomLevel_ > fit + 0.001
					                        ? Qt::OpenHandCursor
					                        : Qt::ArrowCursor);
					return true;
				}

				isPanning_ = false;
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

		if (overlay_)
			overlay_->raise();
	}

	void HalconInteractiveLabel::drawSelectionBox(const QPoint& startWidget,
	                                               const QPoint& endWidget)
	{
		using namespace HalconCpp;

		if (!windowCreated_ || !lastImage_.IsInitialized())
			return;

		// 将控件坐标转为图像坐标
		const QPointF imgStart = imagePosAt(QPointF(startWidget));
		const QPointF imgEnd   = imagePosAt(QPointF(endWidget));

		const double row1 = qMin(imgStart.y(), imgEnd.y());
		const double col1 = qMin(imgStart.x(), imgEnd.x());
		const double row2 = qMax(imgStart.y(), imgEnd.y());
		const double col2 = qMax(imgStart.x(), imgEnd.x());

		// 先重绘干净图像（擦除上一帧的选框）
		applyView();

		// 在 Halcon 窗口上直接绘制选框
		try
		{
			SetColor(handle_, "#0078D7");
			SetDraw(handle_, "margin");
			SetLineWidth(handle_, 2);
			DispRectangle1(handle_, row1, col1, row2, col2);
		}
		catch (...) {}

		// 保持覆盖层在 Halcon 窗口之上
		if (overlay_)
			overlay_->raise();
	}

	void HalconInteractiveLabel::setZoomLevel(double zoom, const QPointF& anchorImagePos)
	{
		const double oldZoom = zoomLevel_;
		zoomLevel_ = qBound(fitZoomLevel() * 0.5, zoom, 32.0);

		if (qFuzzyCompare(oldZoom, zoomLevel_))
			return;

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
