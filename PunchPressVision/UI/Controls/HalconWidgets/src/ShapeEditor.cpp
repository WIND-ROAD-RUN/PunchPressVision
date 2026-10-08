#include "UI/ShapeEditor.h"

#include "UI/HalconInteractiveLabel.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>

#include <cmath>

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
		baseImage_ = image;
		displaying_ = true;
		renderToLabel();
		displaying_ = false;
		drawStampPattern();
		drawAllROIs();
		drawAllMasks();
		drawCenterPoint();
		drawMarker();
		drawMatchRegion();
	}

	// === 套版叠加与对齐编辑 ===

	void ShapeEditor::setStampPattern(const HalconCpp::HImage& rgba, double row, double col,
		double angle, double scale, int alpha)
	{
		stampPatternImage_ = rgba;
		stampRow_ = row;
		stampCol_ = col;
		stampAngle_ = angle;
		stampScale_ = scale;
		stampAlpha_ = alpha;
		hasStampPattern_ = true;
		stampDragging_ = false;
		++stampGeneration_;
		// 已定义中心点时，套版中心（图中心红十字）需与中心点重合
		if (hasCenterPoint_)
			alignStampCenterTo(centerPoint_);
		refreshOverlay();
	}

	void ShapeEditor::clearStampPattern()
	{
		hasStampPattern_ = false;
		stampPatternImage_ = HalconCpp::HImage();
		stampDragging_ = false;
		++stampGeneration_;
		stampDisplayObj_ = HalconCpp::HImage();
		stampCacheValid_ = false;
		refreshOverlay();
	}

	void ShapeEditor::renderToLabel()
	{
		if (!imageLabel_ || !baseImage_.IsInitialized())
			return;

		// 套版不做像素预合成：底图直接显示，套版由 drawStampPattern() 以 DispObj
		// 显示时绘制。视图缩放/平移仅重绘可见区域，不再触发 40MP 全图合成。
		imageLabel_->displayImage(baseImage_);
	}

	HalconCpp::HTuple ShapeEditor::stampHomMat2D(double row, double col, double angle, double scale)
	{
		HalconCpp::HTuple h;
		HalconCpp::HomMat2dIdentity(&h);
		HalconCpp::HomMat2dScale(h, scale, scale, 0.0, 0.0, &h);
		HalconCpp::HomMat2dRotate(h, angle, 0.0, 0.0, &h);
		HalconCpp::HomMat2dTranslate(h, row, col, &h);
		return h;
	}

	void ShapeEditor::alignStampCenterTo(const QPointF& imagePoint)
	{
		if (!hasStampPattern_ || !stampPatternImage_.IsInitialized())
			return;

		try
		{
			// 套版图中心（渲染时红十字固定在包围盒中心，即图像中心）
			const double pr = stampPatternImage_.Height().D() / 2.0;
			const double pc = stampPatternImage_.Width().D() / 2.0;

			// 用不含平移的变换（缩放→旋转）求图案中心的落点，
			// 平移量 = 目标点 - 落点，使图案中心精确对齐目标点
			const HalconCpp::HTuple h = stampHomMat2D(0.0, 0.0, stampAngle_, stampScale_);
			HalconCpp::HTuple r, c;
			HalconCpp::AffineTransPoint2d(h, pr, pc, &r, &c);
			if (r.TupleLength() > 0 && c.TupleLength() > 0)
			{
				stampRow_ = imagePoint.y() - r[0].D();
				stampCol_ = imagePoint.x() - c[0].D();
			}
		}
		catch (...) {}
	}

	QPointF ShapeEditor::stampCenterImagePoint() const
	{
		if (!hasStampPattern_ || !stampPatternImage_.IsInitialized())
			return QPointF();

		try
		{
			const double pr = stampPatternImage_.Height().D() / 2.0;
			const double pc = stampPatternImage_.Width().D() / 2.0;
			const HalconCpp::HTuple h = stampHomMat2D(stampRow_, stampCol_, stampAngle_, stampScale_);
			HalconCpp::HTuple r, c;
			HalconCpp::AffineTransPoint2d(h, pr, pc, &r, &c);
			if (r.TupleLength() > 0 && c.TupleLength() > 0)
				return QPointF(c[0].D(), r[0].D());
		}
		catch (...) {}
		return QPointF();
	}

	void ShapeEditor::setStampOffsetFromCenter(double dRow, double dCol, double angleRad)
	{
		if (!hasStampPattern_ || !hasCenterPoint_)
			return;

		// 先更新旋转角（alignStampCenterTo 使用当前角度计算落点），
		// 再把套版中心放置到 中心点 + 位移 处
		stampAngle_ = angleRad;
		alignStampCenterTo(QPointF(centerPoint_.x() + dCol, centerPoint_.y() + dRow));
		refreshOverlay();
		emit stampPatternChanged();
	}

	void ShapeEditor::drawStampPattern()
	{
		if (!hasStampPattern_ || !stampPatternImage_.IsInitialized() || stampAlpha_ <= 0)
			return;
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		// 仅对齐参数或套版图版本变化时重建缓存；视图缩放/平移不改这些量，
		// 直接复用缓存做小图 DispObj，避免每次视图变化做全图合成
		if (!stampCacheValid_
			|| stampRow_ != cachedStampRow_ || stampCol_ != cachedStampCol_
			|| stampAngle_ != cachedStampAngle_ || stampScale_ != cachedStampScale_
			|| stampAlpha_ != cachedStampAlpha_
			|| stampGeneration_ != cachedStampGeneration_)
		{
			rebuildStampDisplayCache();
		}

		if (!stampDisplayObj_.IsInitialized())
			return;

		try
		{
			HalconCpp::DispObj(stampDisplayObj_, imageLabel_->halconHandle());
		}
		catch (...) {}
	}

	void ShapeEditor::rebuildStampDisplayCache()
	{
		stampDisplayObj_ = HalconCpp::HImage();

		try
		{
			using namespace HalconCpp;

			// 与 PunchPressApp::dispStampPatternOverlays 同逻辑：
			// 仿射套版 RGBA 图（AdaptImageSize="true"，输出为变换后包围盒大小，
			// domain 位于图像绝对坐标），按 alpha>0 裁剪 domain，透明背景不覆盖底图。
			// affine_trans_image 的 HomMat2D 为"输入 -> 输出"方向，直接传入。
			const HTuple h = stampHomMat2D(stampRow_, stampCol_, stampAngle_, stampScale_);

			HImage stampT;
			AffineTransImage(stampPatternImage_, &stampT, h, "constant", "true");

			HImage R, G, B, A;
			Decompose4(stampT, &R, &G, &B, &A);

			HRegion alphaRegion;
			Threshold(A, &alphaRegion, 1.0, 255.0);
			if (alphaRegion.Area().D() > 0.0)
			{
				HImage rgb;
				Compose3(R, G, B, &rgb);
				stampDisplayObj_ = rgb.ReduceDomain(alphaRegion);
			}
		}
		catch (...) {}

		cachedStampRow_ = stampRow_;
		cachedStampCol_ = stampCol_;
		cachedStampAngle_ = stampAngle_;
		cachedStampScale_ = stampScale_;
		cachedStampAlpha_ = stampAlpha_;
		cachedStampGeneration_ = stampGeneration_;
		stampCacheValid_ = true;
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

	HalconCpp::HObject ShapeEditor::mergeObjects(const std::vector<HalconCpp::HObject>& objects)
	{
		using namespace HalconCpp;
		HObject result;
		if (objects.empty())
			return result;

		try
		{
			result = objects[0];
			for (size_t i = 1; i < objects.size(); ++i)
			{
				HObject merged;
				Union2(result, objects[i], &merged);
				result = merged;
			}
		}
		catch (...) {}

		return result;
	}

	HalconCpp::HObject ShapeEditor::drawFreehandRegion(const HalconCpp::HTuple& windowHandle, Tool tool)
	{
		HalconCpp::HObject region;
		try
		{
			HalconCpp::SetColor(windowHandle, (tool == Tool::FreehandMask) ? "red" : "green");
			HalconCpp::SetDraw(windowHandle, "margin");
			HalconCpp::SetLineWidth(windowHandle, 2);
			HalconCpp::DrawRegion(&region, windowHandle);
		}
		catch (...) {}
		return region;
	}

	HalconCpp::HObject ShapeEditor::roi() const
	{
		return mergeObjects(roiObjects_);
	}

	HalconCpp::HObject ShapeEditor::mask() const
	{
		return mergeObjects(maskObjects_);
	}

	void ShapeEditor::setRoiObjects(const std::vector<HalconCpp::HObject>& objects)
	{
		roiObjects_ = objects;
		actionHistory_.erase(
			std::remove(actionHistory_.begin(), actionHistory_.end(), ActionType::ROI),
			actionHistory_.end());
		for (size_t i = 0; i < objects.size(); ++i)
			actionHistory_.append(ActionType::ROI);
		drawing_ = false;
		refreshOverlay();
		emit roiChanged();
	}

	void ShapeEditor::setMaskObjects(const std::vector<HalconCpp::HObject>& objects)
	{
		maskObjects_ = objects;
		actionHistory_.erase(
			std::remove(actionHistory_.begin(), actionHistory_.end(), ActionType::Mask),
			actionHistory_.end());
		for (size_t i = 0; i < objects.size(); ++i)
			actionHistory_.append(ActionType::Mask);
		drawing_ = false;
		refreshOverlay();
		emit maskChanged();
	}

	void ShapeEditor::setCenterPoint(const QPointF& point)
	{
		const bool hadCenter = hasCenterPoint_;
		const QPointF oldPoint = centerPoint_;
		centerPoint_ = point;
		hasCenterPoint_ = true;
		if (hasStampPattern_)
		{
			if (hadCenter)
			{
				// 中心点平移时套版整体跟随，保持与中心点的相对位移（偏移量不变）
				stampRow_ += point.y() - oldPoint.y();
				stampCol_ += point.x() - oldPoint.x();
			}
			else
			{
				// 首次定义中心点：套版中心与其重合（偏移量归零）
				alignStampCenterTo(point);
			}
			emit stampPatternChanged();
		}
		refreshOverlay();
		emit centerPointChanged();
	}

	// === 工具切换 ===

	void ShapeEditor::setTool(Tool tool)
	{
		if (tool_ == tool)
			return;

		if (drawing_)
		{
			drawing_ = false;
			dragMode_ = DragMode::None;
			resizeEdges_ = 0;
			refreshOverlay();
		}

		// 自由绘制 ROI / Mask：立即进入 Halcon 交互绘制，完成后回到 View
		if (tool == Tool::FreehandROI || tool == Tool::FreehandMask)
		{
			if (imageLabel_ && imageLabel_->isReady())
			{
				HalconCpp::HObject region = drawFreehandRegion(imageLabel_->halconHandle(), tool);
				if (region.IsInitialized())
				{
					if (tool == Tool::FreehandROI)
					{
						roiObjects_.push_back(region);
						actionHistory_.append(ActionType::ROI);
						emit roiChanged();
					}
					else
					{
						maskObjects_.push_back(region);
						actionHistory_.append(ActionType::Mask);
						emit maskChanged();
					}
				}
				refreshOverlay();
			}
			tool_ = Tool::View;
			emit toolChanged(tool_);
			return;
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
		roiObjects_.clear();
		drawing_ = false;
		refreshOverlay();
		emit roiChanged();
	}

	void ShapeEditor::clearMask()
	{
		actionHistory_.erase(
			std::remove(actionHistory_.begin(), actionHistory_.end(), ActionType::Mask),
			actionHistory_.end());
		maskObjects_.clear();
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
		roiObjects_.clear();
		maskObjects_.clear();
		actionHistory_.clear();
		hasCenterPoint_ = false;
		drawing_ = false;
		showMarker_ = false;
		showModelContours_ = false;
		modelContours_ = HalconCpp::HObject();
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

	void ShapeEditor::drawModelContours(const HalconCpp::HObject& contours)
	{
		modelContours_ = contours;
		showModelContours_ = true;
		refreshOverlay();
	}

	void ShapeEditor::clearModelContours()
	{
		showModelContours_ = false;
		modelContours_ = HalconCpp::HObject();
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
			if (!roiObjects_.empty())
			{
				roiObjects_.pop_back();
				emit roiChanged();
			}
			break;
		case ActionType::Mask:
			if (!maskObjects_.empty())
			{
				maskObjects_.pop_back();
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

	namespace
	{
		constexpr uint8_t kEdgeLeft   = 1 << 0;
		constexpr uint8_t kEdgeRight  = 1 << 1;
		constexpr uint8_t kEdgeTop    = 1 << 2;
		constexpr uint8_t kEdgeBottom = 1 << 3;
		constexpr double kEdgeHitThreshold = 10.0; // widget 像素，判定“靠近边”的距离
	}

	bool ShapeEditor::eventFilter(QObject* /*obj*/, QEvent* e)
	{
		switch (e->type())
		{
		case QEvent::Wheel:
		{
			if (tool_ != Tool::StampPattern || !hasStampPattern_)
				break;
			auto* we = static_cast<QWheelEvent*>(e);
			const double steps = we->angleDelta().y() / 120.0;
			if (we->modifiers() & Qt::ControlModifier)
			{
				// Ctrl+滚轮 → 缩放
				stampScale_ = qBound(0.05, stampScale_ * std::pow(1.10, steps), 20.0);
			}
			else
			{
				// 滚轮 → 旋转（每格 1°）
				constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
				stampAngle_ += steps * kDegToRad;
			}
			refreshOverlay();
			emit stampPatternChanged();
			return true;
		}

		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(e);
			if (me->button() == Qt::LeftButton)
			{
				if (tool_ == Tool::RectangleROI || tool_ == Tool::RectangleMask)
				{
					const QPoint pos = me->pos();

					if (drawing_)
					{
						// 已有预览矩形 → 根据点击位置决定 移动/缩放/新建
						const QRectF r = QRectF(drawStartWidget_, drawEndWidget_).normalized();

						const bool nearLeft   = qAbs(pos.x() - r.left())   < kEdgeHitThreshold;
						const bool nearRight  = qAbs(pos.x() - r.right())  < kEdgeHitThreshold;
						const bool nearTop    = qAbs(pos.y() - r.top())    < kEdgeHitThreshold;
						const bool nearBottom = qAbs(pos.y() - r.bottom()) < kEdgeHitThreshold;

						if (nearLeft || nearRight || nearTop || nearBottom)
						{
							// 靠近边/角 → 缩放模式
							dragMode_ = DragMode::Resize;
							resizeEdges_ = 0;
							if (nearLeft)   resizeEdges_ |= kEdgeLeft;
							if (nearRight)  resizeEdges_ |= kEdgeRight;
							if (nearTop)    resizeEdges_ |= kEdgeTop;
							if (nearBottom) resizeEdges_ |= kEdgeBottom;
							dragAnchor_ = pos;
							dragStartOrig_ = drawStartWidget_;
							dragEndOrig_   = drawEndWidget_;
						}
						else if (r.contains(pos))
						{
							// 在矩形内部但远离边 → 移动模式
							dragMode_ = DragMode::Move;
							dragAnchor_ = pos;
							dragStartOrig_ = drawStartWidget_;
							dragEndOrig_   = drawEndWidget_;
						}
						else
						{
							// 在矩形外部 → 开始新的矩形
							dragMode_ = DragMode::New;
							drawStartWidget_ = pos;
							drawEndWidget_   = pos;
						}
					}
					else
					{
						// 无预览 → 开始新绘制
						dragMode_ = DragMode::New;
						drawing_ = true;
						drawStartWidget_ = pos;
						drawEndWidget_   = pos;
					}

					refreshOverlay();
					return true;
				}
				else if (tool_ == Tool::CenterPoint)
				{
					// 与 setCenterPoint 同一入口：套版跟随逻辑集中在一处
					setCenterPoint(widgetToImage(me->pos()));
					return true;
				}
				else if (tool_ == Tool::StampPattern && hasStampPattern_)
				{
					// 左键拖动：平移套版
					stampDragging_ = true;
					stampDragAnchorWidget_ = me->pos();
					stampDragStartRow_ = stampRow_;
					stampDragStartCol_ = stampCol_;
					return true;
				}
			}
			else if (me->button() == Qt::RightButton)
			{
				// 右键确认绘制完成（Halcon draw 风格）
				if ((tool_ == Tool::RectangleROI || tool_ == Tool::RectangleMask) && drawing_)
				{
					drawing_ = false;
					dragMode_ = DragMode::None;
					resizeEdges_ = 0;

					const QPointF p1 = widgetToImage(drawStartWidget_);
					const QPointF p2 = widgetToImage(drawEndWidget_);
					const QRectF rect = QRectF(p1, p2).normalized();

					if (rect.width() > 1.0 && rect.height() > 1.0)
					{
						HalconCpp::HObject obj = rectToRegion(rect);
						if (obj.IsInitialized())
						{
							if (tool_ == Tool::RectangleROI)
							{
								roiObjects_.push_back(obj);
								actionHistory_.append(ActionType::ROI);
								emit roiChanged();
							}
							else if (tool_ == Tool::RectangleMask)
							{
								maskObjects_.push_back(obj);
								actionHistory_.append(ActionType::Mask);
								emit maskChanged();
							}
						}
					}

					refreshOverlay();
					// 确认后返回 View，必须再次点击按钮才能进行下一次绘制
					setTool(Tool::View);
					return true;
				}
				if (tool_ == Tool::StampPattern)
				{
					stampDragging_ = false;
					setTool(Tool::View);
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
				const QPoint delta = me->pos() - dragAnchor_;

				switch (dragMode_)
				{
				case DragMode::Move:
					// 整体平移
					drawStartWidget_ = dragStartOrig_ + delta;
					drawEndWidget_   = dragEndOrig_   + delta;
					break;

				case DragMode::Resize:
				{
					// 根据靠近的边调整对应角点
					int dx = delta.x();
					int dy = delta.y();
					// 对调左右边，使拖拽方向与矩形边距方向一致：
					// 比如拖拽左边 → 只需要 dx 影响 drawStartWidget_.x
					if (resizeEdges_ & kEdgeLeft)
						drawStartWidget_.setX(dragStartOrig_.x() + dx);
					if (resizeEdges_ & kEdgeRight)
						drawEndWidget_.setX(dragEndOrig_.x() + dx);
					if (resizeEdges_ & kEdgeTop)
						drawStartWidget_.setY(dragStartOrig_.y() + dy);
					if (resizeEdges_ & kEdgeBottom)
						drawEndWidget_.setY(dragEndOrig_.y() + dy);
					break;
				}

				case DragMode::New:
				default:
					// 首次拖拽：固定起点，移动终点
					drawEndWidget_ = me->pos();
					break;
				}

				refreshOverlay();
				return true;
			}
			else if (tool_ == Tool::StampPattern && stampDragging_)
			{
				const QPointF imgCur = widgetToImage(me->pos());
				const QPointF imgAnchor = widgetToImage(stampDragAnchorWidget_);
				stampRow_ = stampDragStartRow_ + (imgCur.y() - imgAnchor.y());
				stampCol_ = stampDragStartCol_ + (imgCur.x() - imgAnchor.x());
				refreshOverlay();
				emit stampPatternChanged();
				return true;
			}
			break;
		}

		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(e);
			if (tool_ == Tool::StampPattern && me->button() == Qt::LeftButton)
			{
				stampDragging_ = false;
				return true;
			}
			// 矩形绘制不再在左键释放时确认，改为右键确认
			// 左键释放后预览保持，用户可继续调整或右键确认
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
					dragMode_ = DragMode::None;
					resizeEdges_ = 0;
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

		renderToLabel();
		drawStampPattern();
		drawAllROIs();
		drawAllMasks();
		drawCenterPoint();
		drawMarker();
		drawFoundContours();
		drawMatchRegion();
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

	void ShapeEditor::drawFoundContours()
	{
		if (!showModelContours_ || !imageLabel_ || !imageLabel_->isReady())
			return;

		try
		{
			using namespace HalconCpp;
			SetColor(imageLabel_->halconHandle(), "cyan");
			SetLineWidth(imageLabel_->halconHandle(), 2);
			DispObj(modelContours_, imageLabel_->halconHandle());
		}
		catch (...) {}
	}

	void ShapeEditor::drawAllROIs()
	{
		if (!imageLabel_ || !imageLabel_->isReady())
			return;

		using namespace HalconCpp;

		if (!roiObjects_.empty())
		{
			try
			{
				SetColor(imageLabel_->halconHandle(), "green");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				for (const auto& obj : roiObjects_)
				{
					if (obj.IsInitialized())
						DispObj(obj, imageLabel_->halconHandle());
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
				SetColor(imageLabel_->halconHandle(), "green");
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

		if (!maskObjects_.empty())
		{
			try
			{
				SetColor(imageLabel_->halconHandle(), "red");
				SetDraw(imageLabel_->halconHandle(), "margin");
				SetLineWidth(imageLabel_->halconHandle(), 2);
				for (const auto& obj : maskObjects_)
				{
					if (obj.IsInitialized())
						DispObj(obj, imageLabel_->halconHandle());
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
				SetColor(imageLabel_->halconHandle(), "red");
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

	void ShapeEditor::drawMatchRegion()
	{
		if (matchRegions_.empty() || !imageLabel_ || !imageLabel_->isReady())
			return;

		try
		{
			HalconCpp::SetColor(imageLabel_->halconHandle(), "#00BCD4");  // cyan
			HalconCpp::SetDraw(imageLabel_->halconHandle(), "margin");
			HalconCpp::SetLineWidth(imageLabel_->halconHandle(), 2);
			for (const auto& r : matchRegions_)
			{
				if (r.IsInitialized())
					HalconCpp::DispObj(r, imageLabel_->halconHandle());
			}
		}
		catch (...) {}
	}

	void ShapeEditor::setMatchRegion(const HalconCpp::HObject& region)
	{
		matchRegions_.clear();
		if (region.IsInitialized())
			matchRegions_.push_back(region);
		refreshOverlay();
	}

	void ShapeEditor::setMatchRegions(const std::vector<HalconCpp::HObject>& regions)
	{
		matchRegions_ = regions;
		refreshOverlay();
	}

	void ShapeEditor::addMatchRegion(const HalconCpp::HObject& region)
	{
		if (region.IsInitialized())
		{
			matchRegions_.push_back(region);
			refreshOverlay();
		}
	}

	void ShapeEditor::removeLastMatchRegion()
	{
		if (!matchRegions_.empty())
		{
			matchRegions_.pop_back();
			refreshOverlay();
		}
	}

	void ShapeEditor::clearMatchRegion()
	{
		matchRegions_.clear();
		refreshOverlay();
	}

	QPointF ShapeEditor::widgetToImage(const QPoint& widgetPos) const
	{
		if (!imageLabel_)
			return QPointF();
		return imageLabel_->imagePosAt(QPointF(widgetPos));
	}
} // namespace ui
