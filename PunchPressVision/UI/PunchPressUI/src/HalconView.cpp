#include "UI/HalconView.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <QtMath>
#include <algorithm>

namespace
{
	/// 透明覆盖层控件，置于 Halcon 原生窗口之上以捕获 Qt 鼠标/键盘事件。
	/// 必须设置 WA_NativeWindow 创建独立 HWND，方可 z-order 到 Halcon 原生窗口之上。
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
	/// 内部事件过滤器，将覆盖层上的 Qt 鼠标事件转发给 HalconView 处理。
	class HalconView::Filter : public QObject
	{
	public:
		explicit Filter(HalconView* view, QObject* parent = nullptr)
			: QObject(parent), view_(view) {}

	protected:
		bool eventFilter(QObject* obj, QEvent* e) override;

	private:
		HalconView* view_;
	};

	bool HalconView::Filter::eventFilter(QObject* obj, QEvent* e)
	{
		if (!view_ || obj != view_->overlay_)
			return QObject::eventFilter(obj, e);

		switch (e->type())
		{
		// -- 滚轮缩放 ----------------------------------------------------------
		case QEvent::Wheel:
		{
			auto* we = static_cast<QWheelEvent*>(e);
			const double steps = we->angleDelta().y() / 120.0;
			const double factor = qPow(1.15, steps);
			const QPointF anchorImg = view_->imagePosAt(we->position());
			const double newZoom = qBound(view_->fitZoomLevel() * 0.5,
			                              view_->zoomLevel_ * factor, 32.0);
			if (!qFuzzyCompare(view_->zoomLevel_, newZoom))
			{
				const double oldZoom = view_->zoomLevel_;
				view_->zoomLevel_ = newZoom;
				const double ratio = oldZoom / newZoom;
				view_->viewCenter_.setX(anchorImg.x() + (view_->viewCenter_.x() - anchorImg.x()) * ratio);
				view_->viewCenter_.setY(anchorImg.y() + (view_->viewCenter_.y() - anchorImg.y()) * ratio);
				view_->clampViewCenter();
				view_->applyView();
			}
			view_->viewInitialized_ = true;
			return true;
		}

		// -- 鼠标按下 ----------------------------------------------------------
		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (me->button() == Qt::LeftButton)
			{
				view_->panning_ = true;
				view_->lastMousePos_ = me->pos();
				view_->panStartCenter_ = view_->viewCenter_;
				if (view_->overlay_)
					view_->overlay_->setCursor(Qt::ClosedHandCursor);
				return true;
			}
			break;
		}

		// -- 鼠标移动 ----------------------------------------------------------
		case QEvent::MouseMove:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (view_->panning_)
			{
				const QPoint delta = me->pos() - view_->lastMousePos_;
				const QPointF visSize = view_->visibleImageSize();
				if (view_->overlay_)
				{
					const int lw = view_->overlay_->width();
					const int lh = view_->overlay_->height();
					if (lw > 0 && lh > 0)
					{
						const double imgDx = -delta.x() * visSize.x() / lw;
						const double imgDy = -delta.y() * visSize.y() / lh;
						view_->viewCenter_ = QPointF(
							view_->panStartCenter_.x() + imgDx,
							view_->panStartCenter_.y() + imgDy);
						view_->clampViewCenter();
						view_->applyView();
					}
				}
			}
			else
			{
				const double fit = view_->fitZoomLevel();
				if (view_->overlay_)
					view_->overlay_->setCursor(view_->zoomLevel_ > fit + 0.001
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
				view_->panning_ = false;
				const double fit = view_->fitZoomLevel();
				if (view_->overlay_)
					view_->overlay_->setCursor(view_->zoomLevel_ > fit + 0.001
						? Qt::OpenHandCursor
						: Qt::ArrowCursor);
				return true;
			}
			break;
		}

		// -- 双击 → 自适应窗口 ------------------------------------------------
		case QEvent::MouseButtonDblClick:
		{
			if (static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton)
			{
				view_->fitToWindow();
				return true;
			}
			break;
		}

		default:
			break;
		}

		return QObject::eventFilter(obj, e);
	}

	// ===== HalconView 实现 ======================================================

	HalconView::~HalconView()
	{
		close();
	}

	bool HalconView::ensure(QWidget* host)
	{
		using namespace HalconCpp;
		host_ = host;
		if (!host_)
			return false;
		if (handle_.Length() > 0)
			return true;

		try
		{
			const Hlong winId = static_cast<Hlong>(host_->winId());
			OpenWindow(0, 0, host_->width(), host_->height(), winId, "visible", "", &handle_);
			SetWindowAttr("background_color", "gray");

			// 创建透明覆盖层用于捕获鼠标事件
			if (!overlay_)
			{
				auto* ow = new OverlayWidget(host_);
				ow->setMouseTracking(true);
				ow->setGeometry(0, 0, host_->width(), host_->height());
				ow->raise();
				ow->setCursor(Qt::ArrowCursor);

				filter_ = new Filter(this, ow);
				ow->installEventFilter(filter_);

				overlay_ = ow;
			}

			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void HalconView::display(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;

		if (!image.IsInitialized() || handle_.Length() == 0)
			return;

		// 确保覆盖层置顶
		if (overlay_)
		{
			overlay_->resize(host_->width(), host_->height());
			overlay_->raise();
		}

		bool isNewImage = !last_.IsInitialized();
		if (!isNewImage)
		{
			HTuple oldW, oldH, newW, newH;
			last_.GetImageSize(&oldW, &oldH);
			image.GetImageSize(&newW, &newH);
			isNewImage = (oldW[0].I() != newW[0].I()) || (oldH[0].I() != newH[0].I());
		}

		last_ = image;

		if (isNewImage || !viewInitialized_)
		{
			zoomLevel_ = fitZoomLevel();
			HTuple iw, ih;
			image.GetImageSize(&iw, &ih);
			viewCenter_ = QPointF(static_cast<double>(iw[0].I()) / 2.0,
			                       static_cast<double>(ih[0].I()) / 2.0);
			clampViewCenter();
			applyView();
			viewInitialized_ = true;
		}
		else
		{
			applyView();
		}
	}

	void HalconView::fitToWindow()
	{
		zoomLevel_ = fitZoomLevel();
		if (last_.IsInitialized())
		{
			HalconCpp::HTuple iw, ih;
			last_.GetImageSize(&iw, &ih);
			viewCenter_ = QPointF(static_cast<double>(iw[0].I()) / 2.0,
			                       static_cast<double>(ih[0].I()) / 2.0);
		}
		clampViewCenter();
		applyView();
	}

	void HalconView::resizeToHost()
	{
		using namespace HalconCpp;
		if (handle_.Length() == 0 || !host_)
			return;

		try
		{
			SetWindowExtents(handle_, 0, 0, host_->width(), host_->height());
			if (overlay_)
			{
				overlay_->resize(host_->width(), host_->height());
				overlay_->raise();
			}
			if (last_.IsInitialized())
			{
				if (!viewInitialized_)
				{
					fitToWindow();
					viewInitialized_ = true;
				}
				else
				{
					applyView();
				}
			}
		}
		catch (...) {}
	}

	void HalconView::close()
	{
		using namespace HalconCpp;

		if (filter_)
		{
			delete filter_;
			filter_ = nullptr;
		}
		if (overlay_)
		{
			overlay_->deleteLater();
			overlay_ = nullptr;
		}
		if (handle_.Length() == 0)
			return;

		try { CloseWindow(handle_); }
		catch (...) {}
		handle_ = HTuple();
		viewInitialized_ = false;
	}

	// ===== 内部实现 =============================================================

	void HalconView::applyView()
	{
		using namespace HalconCpp;

		if (handle_.Length() == 0 || !last_.IsInitialized())
			return;

		const int lw = host_ ? host_->width() : 0;
		const int lh = host_ ? host_->height() : 0;
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
			DispObj(last_, handle_);
		}
		catch (...) {}

		if (overlay_)
			overlay_->raise();
	}

	double HalconView::fitZoomLevel() const
	{
		if (!last_.IsInitialized())
			return 1.0;

		HalconCpp::HTuple iw, ih;
		last_.GetImageSize(&iw, &ih);
		const double imgW = static_cast<double>(iw[0].I());
		const double imgH = static_cast<double>(ih[0].I());

		const int lw = host_ ? host_->width() : 0;
		const int lh = host_ ? host_->height() : 0;
		if (lw <= 0 || lh <= 0 || imgW <= 0 || imgH <= 0)
			return 1.0;

		return qMin(static_cast<double>(lw) / imgW, static_cast<double>(lh) / imgH);
	}

	QPointF HalconView::visibleImageSize() const
	{
		const int lw = host_ ? host_->width() : 0;
		const int lh = host_ ? host_->height() : 0;
		if (lw <= 0 || lh <= 0 || zoomLevel_ <= 0.0)
			return QPointF(1.0, 1.0);
		return QPointF(lw / zoomLevel_, lh / zoomLevel_);
	}

	void HalconView::clampViewCenter()
	{
		if (!last_.IsInitialized())
			return;

		HalconCpp::HTuple iw, ih;
		last_.GetImageSize(&iw, &ih);
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

	QPointF HalconView::imagePosAt(const QPointF& widgetPos) const
	{
		const int lw = host_ ? host_->width() : 0;
		const int lh = host_ ? host_->height() : 0;
		if (lw <= 0 || lh <= 0)
			return viewCenter_;

		const QPointF visSize = visibleImageSize();
		const double row1 = viewCenter_.y() - visSize.y() / 2.0;
		const double col1 = viewCenter_.x() - visSize.x() / 2.0;

		const double imgCol = col1 + (widgetPos.x() / lw) * visSize.x();
		const double imgRow = row1 + (widgetPos.y() / lh) * visSize.y();

		return QPointF(imgCol, imgRow);
	}
}
