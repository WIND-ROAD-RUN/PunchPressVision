#include "UI/ShapeEditor.h"

#include "UI/HalconInteractiveLabel.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>

namespace ui
{
	ShapeEditor::ShapeEditor(QWidget* parent)
		: QWidget(parent)
	{
		imageLabel_ = new HalconInteractiveLabel(this);
		imageLabel_->installOverlayEventFilter(this);

		connect(imageLabel_, &HalconInteractiveLabel::viewChanged,
		        this, &ShapeEditor::refreshOverlay);
	}

	ShapeEditor::~ShapeEditor() = default;

	void ShapeEditor::displayImage(const HalconCpp::HImage& image)
	{
		displaying_ = true;
		imageLabel_->displayImage(image);
		displaying_ = false;
		drawAllROIs();
		drawCenterPoint();
	}

	HalconCpp::HObject ShapeEditor::roi() const
	{
		using namespace HalconCpp;

		HalconCpp::HObject result;
		if (roiRects_.isEmpty())
			return result;

		try
		{
			// 首个矩形
			GenRectangle1(&result,
				roiRects_[0].top(), roiRects_[0].left(),
				roiRects_[0].bottom(), roiRects_[0].right());

			// 后续矩形逐一合并
			for (int i = 1; i < roiRects_.size(); ++i)
			{
				HObject next;
				GenRectangle1(&next,
					roiRects_[i].top(), roiRects_[i].left(),
					roiRects_[i].bottom(), roiRects_[i].right());
				HObject merged;
				Union2(result, next, &merged);
				result = merged;
			}
		}
		catch (...) {}

		return result;
	}

	void ShapeEditor::setTool(Tool tool)
	{
		if (tool_ == tool)
			return;

		if (roiDrawing_)
		{
			roiDrawing_ = false;
			refreshOverlay();
		}

		tool_ = tool;

		if (imageLabel_)
		{
			imageLabel_->setCursor(tool_ == Tool::View
				? Qt::ArrowCursor
				: Qt::CrossCursor);
		}

		emit toolChanged(tool_);
	}

	void ShapeEditor::clearROI()
	{
		roiRects_.clear();
		roiDrawing_ = false;
		refreshOverlay();
		emit roiChanged();
	}

	void ShapeEditor::undoROI()
	{
		if (roiRects_.isEmpty())
			return;

		roiRects_.pop_back();
		roiDrawing_ = false;
		refreshOverlay();
		emit roiChanged();
	}

	void ShapeEditor::clearCenterPoint()
	{
		hasCenterPoint_ = false;
		refreshOverlay();
		emit centerPointChanged();
	}

	void ShapeEditor::clearAll()
	{
		roiRects_.clear();
		roiDrawing_ = false;
		hasCenterPoint_ = false;
		refreshOverlay();
		emit roiChanged();
		emit centerPointChanged();
	}

	void ShapeEditor::resizeEvent(QResizeEvent* e)
	{
		QWidget::resizeEvent(e);
		if (imageLabel_)
			imageLabel_->setGeometry(0, 0, e->size().width(), e->size().height());
	}

	bool ShapeEditor::eventFilter(QObject* /*obj*/, QEvent* e)
	{
		switch (e->type())
		{
		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (me->button() == Qt::LeftButton)
			{
				if (tool_ == Tool::RectangleROI)
				{
					// 开始新 ROI 拖拽（不清除已有 ROI，支持多个区域）
					roiDrawing_ = true;
					roiStartWidget_ = me->pos();
					roiEndWidget_ = me->pos();
					refreshOverlay();
					return true;
				}
				else if (tool_ == Tool::CenterPoint)
				{
					centerPoint_ = widgetToImage(me->pos());
					hasCenterPoint_ = true;
					refreshOverlay();
					emit centerPointChanged();
					return true;
				}
			}
			break;
		}

		case QEvent::MouseMove:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (tool_ == Tool::RectangleROI && roiDrawing_)
			{
				roiEndWidget_ = me->pos();
				refreshOverlay();
				return true;
			}
			break;
		}

		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (me->button() == Qt::LeftButton && tool_ == Tool::RectangleROI && roiDrawing_)
			{
				roiEndWidget_ = me->pos();
				roiDrawing_ = false;

				const QPointF p1 = widgetToImage(roiStartWidget_);
				const QPointF p2 = widgetToImage(roiEndWidget_);
				const QRectF rect = QRectF(p1, p2).normalized();

				if (rect.width() > 1.0 && rect.height() > 1.0)
				{
					roiRects_.append(rect);  // 追加到列表
					emit roiChanged();
				}

				refreshOverlay();
				return true;
			}
			break;
		}

		case QEvent::KeyPress:
		{
			auto* ke = static_cast<QKeyEvent*>(e);
			if (ke->key() == Qt::Key_Escape)
			{
				if (roiDrawing_)
				{
					roiDrawing_ = false;
					refreshOverlay();
				}
				setTool(Tool::View);
				return true;
			}
			break;
		}

		default:
			break;
		}

		return false;
	}

	void ShapeEditor::refreshOverlay()
	{
		if (!imageLabel_ || !imageLabel_->isReady() || displaying_)
			return;

		imageLabel_->displayImage(imageLabel_->lastImage());
		drawAllROIs();
		drawCenterPoint();
	}

	void ShapeEditor::drawAllROIs()
	{
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		using namespace HalconCpp;

		// 绘制所有已确认的 ROI
		if (!roiRects_.isEmpty())
		{
			try
			{
				SetColor(imageLabel_->halconHandle(), "green");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				for (const auto& r : roiRects_)
				{
					DispRectangle1(imageLabel_->halconHandle(),
						r.top(), r.left(), r.bottom(), r.right());
				}
			}
			catch (...) {}
		}

		// 绘制拖拽中的橡皮筋矩形
		if (roiDrawing_)
		{
			const QPointF p1 = widgetToImage(roiStartWidget_);
			const QPointF p2 = widgetToImage(roiEndWidget_);
			const QRectF rect = QRectF(p1, p2).normalized();

			try
			{
				SetColor(imageLabel_->halconHandle(), "yellow");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				DispRectangle1(imageLabel_->halconHandle(),
					rect.top(), rect.left(), rect.bottom(), rect.right());
			}
			catch (...) {}
		}
	}

	void ShapeEditor::drawCenterPoint()
	{
		if (!hasCenterPoint_ || !imageLabel_ || !imageLabel_->isReady())
			return;

		try
		{
			HalconCpp::SetColor(imageLabel_->halconHandle(), "red");
			HalconCpp::SetLineWidth(imageLabel_->halconHandle(), 2);
			HalconCpp::DispCross(imageLabel_->halconHandle(),
				centerPoint_.y(), centerPoint_.x(), 40, 0.785398);
		}
		catch (...) {}
	}

	QPointF ShapeEditor::widgetToImage(const QPoint& widgetPos) const
	{
		if (!imageLabel_)
			return QPointF();
		return imageLabel_->imagePosAt(QPointF(widgetPos));
	}
} // namespace ui
