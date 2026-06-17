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

	// === 图像显示 ===

	void ShapeEditor::displayImage(const HalconCpp::HImage& image)
	{
		displaying_ = true;
		imageLabel_->displayImage(image);
		displaying_ = false;
		drawAllROIs();
		drawAllMasks();
		drawCenterPoint();
		drawMarker();
	}

	// === HObject 导出 ===

	HalconCpp::HObject ShapeEditor::rectToRegion(const QRectF& r)
	{
		HalconCpp::HObject obj;
		try
		{
			HalconCpp::GenRectangle1(&obj,
				r.top(), r.left(), r.bottom(), r.right());
		}
		catch (...) {}
		return obj;
	}

	HalconCpp::HObject ShapeEditor::mergeRects(const QVector<QRectF>& rects)
	{
		using namespace HalconCpp;
		HObject result;
		if (rects.isEmpty())
			return result;

		try
		{
			result = rectToRegion(rects[0]);
			for (int i = 1; i < rects.size(); ++i)
			{
				HObject next = rectToRegion(rects[i]);
				HObject merged;
				Union2(result, next, &merged);
				result = merged;
			}
		}
		catch (...) {}

		return result;
	}

	HalconCpp::HObject ShapeEditor::roi() const
	{
		return mergeRects(roiRects_);
	}

	HalconCpp::HObject ShapeEditor::mask() const
	{
		return mergeRects(maskRects_);
	}

	// === 工具切换 ===

	void ShapeEditor::setTool(Tool tool)
	{
		if (tool_ == tool)
			return;

		if (drawing_)
		{
			drawing_ = false;
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

	// === 编辑操作 ===

	void ShapeEditor::clearROI()
	{
		actionHistory_.erase(
			std::remove(actionHistory_.begin(), actionHistory_.end(), ActionType::ROI),
			actionHistory_.end());
		roiRects_.clear();
		drawing_ = false;
		refreshOverlay();
		emit roiChanged();
	}

	void ShapeEditor::clearMask()
	{
		actionHistory_.erase(
			std::remove(actionHistory_.begin(), actionHistory_.end(), ActionType::Mask),
			actionHistory_.end());
		maskRects_.clear();
		drawing_ = false;
		refreshOverlay();
		emit maskChanged();
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
		maskRects_.clear();
		actionHistory_.clear();
		hasCenterPoint_ = false;
		drawing_ = false;
		showMarker_ = false;
		refreshOverlay();
		emit roiChanged();
		emit maskChanged();
		emit centerPointChanged();
	}

	void ShapeEditor::drawRecognitionMarker(double row, double col, double angle, double score)
	{
		markerRow_ = row;
		markerCol_ = col;
		markerAngle_ = angle;
		markerScore_ = score;
		showMarker_ = true;
		refreshOverlay();
	}

	void ShapeEditor::clearMarker()
	{
		showMarker_ = false;
		refreshOverlay();
	}

	void ShapeEditor::undo()
	{
		if (actionHistory_.isEmpty())
			return;

		const ActionType last = actionHistory_.takeLast();

		switch (last)
		{
		case ActionType::ROI:
			if (!roiRects_.isEmpty())
			{
				roiRects_.pop_back();
				emit roiChanged();
			}
			break;
		case ActionType::Mask:
			if (!maskRects_.isEmpty())
			{
				maskRects_.pop_back();
				emit maskChanged();
			}
			break;
		}

		drawing_ = false;
		refreshOverlay();
	}

	// === Qt 事件 ===

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
				if (tool_ == Tool::RectangleROI || tool_ == Tool::RectangleMask)
				{
					drawing_ = true;
					drawStartWidget_ = me->pos();
					drawEndWidget_ = me->pos();
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
			if ((tool_ == Tool::RectangleROI || tool_ == Tool::RectangleMask) && drawing_)
			{
				drawEndWidget_ = me->pos();
				refreshOverlay();
				return true;
			}
			break;
		}

		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(e);
			if (me->button() == Qt::LeftButton && drawing_)
			{
				drawEndWidget_ = me->pos();
				drawing_ = false;

				const QPointF p1 = widgetToImage(drawStartWidget_);
				const QPointF p2 = widgetToImage(drawEndWidget_);
				const QRectF rect = QRectF(p1, p2).normalized();

				if (rect.width() > 1.0 && rect.height() > 1.0)
				{
					if (tool_ == Tool::RectangleROI)
					{
						roiRects_.append(rect);
						actionHistory_.append(ActionType::ROI);
						emit roiChanged();
					}
					else if (tool_ == Tool::RectangleMask)
					{
						maskRects_.append(rect);
						actionHistory_.append(ActionType::Mask);
						emit maskChanged();
					}
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
				if (drawing_)
				{
					drawing_ = false;
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

	// === 内部绘制 ===

	void ShapeEditor::refreshOverlay()
	{
		if (!imageLabel_ || !imageLabel_->isReady() || displaying_)
			return;

		imageLabel_->displayImage(imageLabel_->lastImage());
		drawAllROIs();
		drawAllMasks();
		drawCenterPoint();
		drawMarker();
	}

	void ShapeEditor::drawMarker()
	{
		if (!showMarker_ || !imageLabel_ || !imageLabel_->isReady())
			return;

		try
		{
			using namespace HalconCpp;
			SetColor(imageLabel_->halconHandle(), "#00FF00");
			SetLineWidth(imageLabel_->halconHandle(), 3);
			DispCross(imageLabel_->halconHandle(),
				markerRow_, markerCol_, 60, markerAngle_);

			SetLineWidth(imageLabel_->halconHandle(), 1);
			QString scoreText = QString("Score: %1%").arg(markerScore_ * 100.0, 0, 'f', 1);
			SetTposition(imageLabel_->halconHandle(),
				static_cast<HTuple>(markerRow_ + 80),
				static_cast<HTuple>(markerCol_ - 40));
			WriteString(imageLabel_->halconHandle(),
				scoreText.toStdString().c_str());
		}
		catch (...) {}
	}

	void ShapeEditor::drawAllROIs()
	{
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		using namespace HalconCpp;

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

		if (drawing_ && tool_ == Tool::RectangleROI)
		{
			const QPointF p1 = widgetToImage(drawStartWidget_);
			const QPointF p2 = widgetToImage(drawEndWidget_);
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

	void ShapeEditor::drawAllMasks()
	{
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		using namespace HalconCpp;

		if (!maskRects_.isEmpty())
		{
			try
			{
				SetColor(imageLabel_->halconHandle(), "magenta");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				for (const auto& r : maskRects_)
				{
					DispRectangle1(imageLabel_->halconHandle(),
						r.top(), r.left(), r.bottom(), r.right());
				}
			}
			catch (...) {}
		}

		if (drawing_ && tool_ == Tool::RectangleMask)
		{
			const QPointF p1 = widgetToImage(drawStartWidget_);
			const QPointF p2 = widgetToImage(drawEndWidget_);
			const QRectF rect = QRectF(p1, p2).normalized();

			try
			{
				SetColor(imageLabel_->halconHandle(), "orange");
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
			HalconCpp::SetColor(imageLabel_->halconHandle(), "blue");
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
