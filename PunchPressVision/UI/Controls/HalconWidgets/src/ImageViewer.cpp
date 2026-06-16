#include "UI/ImageViewer.h"
#include "UI/HalconInteractiveLabel.h"
#include "UI/OverlayWidget.h"

#include <QDateTime>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLine>
#include <QMouseEvent>
#include <QResizeEvent>

#include <cmath>

namespace ui
{
	// ---- 构造 / 析构 ---------------------------------------------------------

	ImageViewer::ImageViewer(QWidget* parent)
		: QWidget(parent)
	{
		// L2 核心显示区（填充整个控件）
		imageLabel_ = new HalconInteractiveLabel(this);

		// 透明绘制层（悬于 imageLabel_ 之上，事件穿透到 L2）
		drawingOverlay_ = new OverlayWidget(this);
		drawingOverlay_->raise();

		// 右下角缩放倍率标签
		zoomLabel_ = new QLabel(this);
		zoomLabel_->setStyleSheet(QStringLiteral(
			"QLabel {"
			"  color: #CCCCCC;"
			"  background: rgba(0,0,0,140);"
			"  padding: 2px 6px;"
			"  border-radius: 3px;"
			"  font-size: 11px;"
			"}"));
		zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		zoomLabel_->setFixedWidth(64);
		zoomLabel_->setFixedHeight(20);

		// 左下角坐标 + 灰度标签
		coordLabel_ = new QLabel(this);
		coordLabel_->setStyleSheet(QStringLiteral(
			"QLabel {"
			"  color: #CCCCCC;"
			"  background: rgba(0,0,0,140);"
			"  padding: 2px 6px;"
			"  border-radius: 3px;"
			"  font-size: 11px;"
			"}"));
		coordLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		coordLabel_->setMinimumWidth(160);
		coordLabel_->setFixedHeight(20);

		// 监听 L2 缩放变化以更新百分比标签
		connect(imageLabel_, &HalconInteractiveLabel::zoomChanged,
		        this, &ImageViewer::updateZoomLabel);

		// 在 L2 的内部覆盖层上安装事件过滤器，用于鼠标跟踪和模式交互
		imageLabel_->installOverlayEventFilter(this);
	}

	// ---- 公共 API ------------------------------------------------------------

	void ImageViewer::displayImage(const HalconCpp::HImage& image)
	{
		imageLabel_->displayImage(image);
		updateZoomLabel();
	}

	void ImageViewer::setMode(Mode mode)
	{
		if (mode_ == mode)
			return;

		// 退出旧模式清理
		switch (mode_)
		{
		case Mode::Crosshair:
			drawingOverlay_->setCrosshairVisible(false);
			coordLabel_->clear();
			break;
		case Mode::ROI:
			if (roiDragging_)
				finishROI();
			drawingOverlay_->clearROI();
			break;
		case Mode::Measurement:
			measureActive_ = false;
			measureHasP1_ = false;
			drawingOverlay_->clearMeasureLine();
			break;
		default:
			break;
		}

		mode_ = mode;

		// 进入新模式初始化
		switch (mode_)
		{
		case Mode::Crosshair:
			drawingOverlay_->setCrosshairVisible(true);
			break;
		case Mode::Measurement:
			measureActive_ = true;
			measureHasP1_ = false;
			break;
		default:
			break;
		}
	}

	ImageViewer::Mode ImageViewer::mode() const
	{
		return mode_;
	}

	void ImageViewer::setCrosshairEnabled(bool on)
	{
		setMode(on ? Mode::Crosshair : Mode::Normal);
	}

	void ImageViewer::setROIMode(bool on)
	{
		setMode(on ? Mode::ROI : Mode::Normal);
	}

	HalconCpp::HObject ImageViewer::currentROI() const
	{
		using namespace HalconCpp;

		if (!roiRect_.isValid() || roiRect_.isEmpty())
		{
			HObject empty;
			GenEmptyObj(&empty);
			return empty;
		}

		const QPointF tl = widgetToImage(QPointF(roiRect_.topLeft()));
		const QPointF br = widgetToImage(QPointF(roiRect_.bottomRight()));

		HObject region;
		GenRectangle1(&region, tl.y(), tl.x(), br.y(), br.x());
		return region;
	}

	void ImageViewer::setPixelToWorldRatio(double mmPerPixel)
	{
		mmPerPixel_ = mmPerPixel;
	}

	void ImageViewer::addOverlay(const HalconCpp::HObject& /*obj*/, const QString& /*label*/)
	{
		// TODO: L3 扩展 — 存储 HObject 并在 paintEvent 中绘制
	}

	void ImageViewer::clearOverlays()
	{
		// TODO: L3 扩展 — 清除存储的 HObject 列表
	}

	void ImageViewer::setOverlayVisible(bool visible)
	{
		drawingOverlay_->setOverlayVisible(visible);
	}

	HalconInteractiveLabel* ImageViewer::imageLabel() const
	{
		return imageLabel_;
	}

	// ---- Qt 事件 ------------------------------------------------------------

	void ImageViewer::resizeEvent(QResizeEvent* e)
	{
		QWidget::resizeEvent(e);

		const QSize sz = e->size();

		// L2 控件 + 覆盖层填满整个区域
		imageLabel_->setGeometry(0, 0, sz.width(), sz.height());
		drawingOverlay_->setGeometry(0, 0, sz.width(), sz.height());
		drawingOverlay_->raise();

		// 右下角缩放标签
		zoomLabel_->move(sz.width() - zoomLabel_->width() - 6,
		                 sz.height() - zoomLabel_->height() - 6);

		// 左下角坐标标签
		coordLabel_->move(6, sz.height() - coordLabel_->height() - 6);
	}

	bool ImageViewer::eventFilter(QObject* /*obj*/, QEvent* e)
	{
		// 本过滤器仅安装在 HalconInteractiveLabel 的内部覆盖层上，
		// 因此到达此处的所有事件均来自该覆盖层，无需额外判断 obj。

		switch (e->type())
		{
		// -- 鼠标移动：坐标跟踪 + 模式绘制 --------------------------------
		case QEvent::MouseMove:
		{
			auto* me = static_cast<QMouseEvent*>(e);
			const QPoint pos = me->pos();

			// 坐标/灰度实时显示（Crosshair / Measurement 模式）
			if (mode_ == Mode::Crosshair || mode_ == Mode::Measurement)
			{
				updateCoordLabel(pos);
			}

			// 十字线跟随
			if (mode_ == Mode::Crosshair)
			{
				drawingOverlay_->setCrosshairPos(pos);
			}

			// ROI 拖拽
			if (mode_ == Mode::ROI && roiDragging_)
			{
				updateROI(pos);
				return true; // 消费事件，阻止 L2 平移
			}

			// 测距预览（已有 P1，悬停显示临时线）
			if (mode_ == Mode::Measurement && measureHasP1_)
			{
				updateMeasurePreview(pos);
				// 不消费，允许 L2 正常响应
			}

			return false; // 不消费，L2 正常处理平移/光标
		}

		// -- 鼠标按下：模式特定操作 ----------------------------------------
		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (mode_ == Mode::ROI && me->button() == Qt::LeftButton)
			{
				startROI(me->pos());
				return true; // 消费，阻止 L2 平移
			}

			if (mode_ == Mode::Measurement
			    && me->button() == Qt::LeftButton
			    && (me->modifiers() & Qt::ShiftModifier))
			{
				placeMeasurePoint(me->pos());
				return true;
			}

			return false; // 其他情况放行给 L2（缩放/平移/复位）
		}

		// -- 鼠标释放：完成模式操作 ----------------------------------------
		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(e);

			if (mode_ == Mode::ROI && roiDragging_)
			{
				finishROI();
				return true;
			}

			return false;
		}

		// -- 键盘：截图 + 委托 L2 快捷键 ----------------------------------
		case QEvent::KeyPress:
		{
			auto* ke = static_cast<QKeyEvent*>(e);

			// Ctrl+S: 截图
			if (ke->modifiers() == Qt::ControlModifier && ke->key() == Qt::Key_S)
			{
				saveScreenshot();
				return true;
			}

			// Ctrl+0 / Ctrl+1: 放行给 L2
			return false;
		}

		default:
			break;
		}

		return false;
	}

	// ---- 内部实现：信息标签 -------------------------------------------------

	void ImageViewer::updateZoomLabel()
	{
		const double z = imageLabel_->currentZoom();
		const int percent = static_cast<int>(z * 100.0);
		zoomLabel_->setText(QStringLiteral("%1%").arg(percent));
	}

	void ImageViewer::updateCoordLabel(QPoint widgetPos)
	{
		const QPointF img = widgetToImage(QPointF(widgetPos));
		const int col = static_cast<int>(img.x());
		const int row = static_cast<int>(img.y());
		const int gv = grayValueAt(row, col);

		if (gv >= 0)
			coordLabel_->setText(QStringLiteral("(%1, %2)  G=%3").arg(col).arg(row).arg(gv));
		else
			coordLabel_->setText(QStringLiteral("(%1, %2)").arg(col).arg(row));

		emit mousePosChanged(row, col, gv);
	}

	int ImageViewer::grayValueAt(int row, int col) const
	{
		using namespace HalconCpp;

		auto img = imageLabel_->lastImage();
		if (!img.IsInitialized())
			return -1;

		try
		{
			HTuple gray;
			GetGrayval(img, row, col, &gray);
			return static_cast<int>(gray[0].I());
		}
		catch (...)
		{
			return -1;
		}
	}

	QPointF ImageViewer::widgetToImage(const QPointF& widgetPos) const
	{
		return imageLabel_->imagePosAt(widgetPos);
	}

	// ---- 内部实现：ROI 框选 ------------------------------------------------

	void ImageViewer::startROI(QPoint widgetPos)
	{
		roiDragging_ = true;
		roiStart_ = widgetPos;
		roiRect_ = QRect();
	}

	void ImageViewer::updateROI(QPoint widgetPos)
	{
		roiRect_ = QRect(roiStart_, widgetPos).normalized();
		drawingOverlay_->setROIRect(roiRect_);
	}

	void ImageViewer::finishROI()
	{
		roiDragging_ = false;

		if (roiRect_.width() > 4 && roiRect_.height() > 4)
		{
			const QPointF tl = widgetToImage(QPointF(roiRect_.topLeft()));
			const QPointF br = widgetToImage(QPointF(roiRect_.bottomRight()));
			emit roiSelected(QRectF(tl, br));
		}

		drawingOverlay_->clearROI();
		roiRect_ = QRect();
	}

	// ---- 内部实现：双点测距 ------------------------------------------------

	void ImageViewer::placeMeasurePoint(QPoint widgetPos)
	{
		if (!measureHasP1_)
		{
			measureP1_ = widgetPos;
			measureHasP1_ = true;
			drawingOverlay_->setMeasureLine(widgetPos, widgetPos, 0);
		}
		else
		{
			measureP2_ = widgetPos;
			finishMeasurement();
		}
	}

	void ImageViewer::updateMeasurePreview(QPoint widgetPos)
	{
		const double pxDist = QLineF(measureP1_, widgetPos).length();
		drawingOverlay_->setMeasureLine(measureP1_, widgetPos, pxDist);
	}

	void ImageViewer::finishMeasurement()
	{
		measureHasP1_ = false;

		const QPointF img1 = widgetToImage(QPointF(measureP1_));
		const QPointF img2 = widgetToImage(QPointF(measureP2_));
		const double dx = img2.x() - img1.x();
		const double dy = img2.y() - img1.y();
		const double pixelDist = std::sqrt(dx * dx + dy * dy);
		const double worldDist = (mmPerPixel_ > 0.0) ? pixelDist * mmPerPixel_ : 0.0;

		// 在覆盖层上保留测量结果
		const double pxDist = QLineF(measureP1_, measureP2_).length();
		drawingOverlay_->setMeasureLine(measureP1_, measureP2_, pixelDist);

		emit measurementReady(pixelDist, worldDist);
	}

	// ---- 内部实现：截图 ---------------------------------------------------

	void ImageViewer::saveScreenshot()
	{
		using namespace HalconCpp;

		if (!imageLabel_->isReady())
			return;

		const QString defaultName = QStringLiteral("screenshot_")
			+ QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
			+ QStringLiteral(".png");

		const QString path = QFileDialog::getSaveFileName(
			this,
			QStringLiteral("保存截图"),
			defaultName,
			QStringLiteral("PNG (*.png);;BMP (*.bmp);;JPEG (*.jpg)"));

		if (path.isEmpty())
			return;

		try
		{
			// 根据扩展名推导格式
			QString fmt = QStringLiteral("png");
			if (path.endsWith(QStringLiteral(".bmp"), Qt::CaseInsensitive))
				fmt = QStringLiteral("bmp");
			else if (path.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
			         || path.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive))
				fmt = QStringLiteral("jpeg");

			HImage screenshot;
			DumpWindowImage(&screenshot, imageLabel_->halconHandle());
			screenshot.WriteImage(fmt.toStdString().c_str(), 0,
			                      path.toStdString().c_str());
		}
		catch (...) {}
	}
} // namespace ui
