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
		drawROI();
		drawCenterPoint();
	}

	HalconCpp::HObject ShapeEditor::roi() const
	{
		HalconCpp::HObject obj;
		if (hasROI_)
		{
			try
			{
				HalconCpp::GenRectangle1(&obj,
					roiRect_.top(), roiRect_.left(),
					roiRect_.bottom(), roiRect_.right());
			}
			catch (...) {}
		}
		return obj;
	}

	void ShapeEditor::setTool(Tool tool)
	{
		if (tool_ == tool)
			return;

		// 切换工具时取消未完成的 ROI 绘制（橡皮筋）
		if (roiDrawing_)
		{
			roiDrawing_ = false;
			refreshOverlay();
		}

		tool_ = tool;

		// 根据工具设置光标
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
		hasROI_ = false;
		roiRect_ = QRectF();
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
		hasROI_ = false;
		roiRect_ = QRectF();
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
					// 开始新的 ROI 拖拽前清除旧 ROI（单 ROI 模式）
					hasROI_ = false;
					roiRect_ = QRectF();
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
					roiRect_ = rect;
					hasROI_ = true;
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

		// View 工具或其他未消费事件放行给 L2
		return false;
	}

	void ShapeEditor::refreshOverlay()
	{
		if (!imageLabel_ || !imageLabel_->isReady() || displaying_)
			return;

		// 重绘底图 + 叠加 ROI / 中心点
		imageLabel_->displayImage(imageLabel_->lastImage());
		drawROI();
		drawCenterPoint();
	}

	void ShapeEditor::drawROI()
	{
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		using namespace HalconCpp;

		// 绘制已确认的 ROI（与橡皮筋使用同样的 DispRectangle1 API）
		if (hasROI_)
		{
			try
			{
				SetColor(imageLabel_->halconHandle(), "green");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				DispRectangle1(imageLabel_->halconHandle(),
					roiRect_.top(), roiRect_.left(),
					roiRect_.bottom(), roiRect_.right());
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
