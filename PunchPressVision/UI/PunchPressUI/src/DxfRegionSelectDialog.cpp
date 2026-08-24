// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/DxfRegionSelectDialog.h"
#include "ui_DlgDxfRegionSelect.h"

#include <QMouseEvent>
#include <QShowEvent>

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	DxfRegionSelectDialog::DxfRegionSelectDialog(const QString& dxfPath, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::DlgDxfRegionSelectClass())
		, dxfPath_(dxfPath)
	{
		ui->setupUi(this);

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setWindowState(windowState() | Qt::WindowFullScreen);
#endif

		// 预览区域：替换 QLabel 占位为 HalconInteractiveLabel（支持拖拽缩放）
		ui->vLayout_preview->removeWidget(ui->label_imgPreview);
		ui->label_imgPreview->hide();
		labelPreview_ = new HalconInteractiveLabel(this);
		labelPreview_->setMinimumSize(320, 240);
		labelPreview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		labelPreview_->setAlignment(Qt::AlignCenter);
		ui->vLayout_preview->addWidget(labelPreview_);

		// 框选鼠标事件优先于控件的平移处理（后安装的过滤器先执行）
		labelPreview_->installOverlayEventFilter(this);

		connect(ui->pbtn_draw, &QPushButton::toggled,
			this, &DxfRegionSelectDialog::onDrawToggled);
		connect(ui->pbtn_undo, &QPushButton::clicked,
			this, &DxfRegionSelectDialog::onUndo);
		connect(ui->pbtn_confirm, &QPushButton::clicked,
			this, &DxfRegionSelectDialog::onConfirm);
		connect(ui->pbtn_cancel, &QPushButton::clicked,
			this, &DxfRegionSelectDialog::onCancel);

		// 视图缩放/平移会重绘底图，区域框需同步重绘
		connect(labelPreview_, &HalconInteractiveLabel::viewChanged,
			this, [this]() { redrawOverlays(); });
	}

	DxfRegionSelectDialog::~DxfRegionSelectDialog()
	{
		delete ui;
	}

	void DxfRegionSelectDialog::showEvent(QShowEvent* event)
	{
		QDialog::showEvent(event);
		if (parentWidget())
			resize(parentWidget()->size());

		if (!layoutReady_)
			loadAndPreview();
	}

	bool DxfRegionSelectDialog::loadAndPreview()
	{
		if (!inf::loadDxfContours(dxfPath_.toStdString(), contours_))
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("加载失败"),
				QStringLiteral("无法解析所选 DXF 文件，请确认文件有效。"));
			return false;
		}

		HalconCpp::HTuple hm;
		if (!inf::renderDxfContoursPreview(contours_, previewImage_, hm))
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("加载失败"),
				QStringLiteral("DXF 图纸内容为空或无法渲染。"));
			return false;
		}

		// 预计算 图像坐标 -> 轮廓坐标 的逆变换，供框选映射
		HalconCpp::HomMat2dInvert(hm, &img2Contour_);
		layoutReady_ = true;

		labelPreview_->displayImage(previewImage_);
		return true;
	}

	// ===== 框选交互 =====

	bool DxfRegionSelectDialog::eventFilter(QObject* obj, QEvent* event)
	{
		if (!drawMode_ || !layoutReady_)
			return QDialog::eventFilter(obj, event);

		switch (event->type())
		{
		case QEvent::MouseButtonPress:
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() != Qt::LeftButton)
				break;
			dragging_ = true;
			dragStart_ = dragCur_ = me->pos();
			return true; // 绘制模式下左键用于框选，吞掉以阻止平移
		}
		case QEvent::MouseMove:
		{
			if (!dragging_)
				break;
			dragCur_ = static_cast<QMouseEvent*>(event)->pos();
			// 重绘底图擦除上一帧，再画区域框
			labelPreview_->displayImage(previewImage_);
			redrawOverlays();
			return true;
		}
		case QEvent::MouseButtonRelease:
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() != Qt::LeftButton || !dragging_)
				break;
			dragging_ = false;

			const QRectF imgRect = imageRectFromWidget(dragStart_, me->pos());
			// 忽略误触的微小框选
			if (imgRect.width() >= 3.0 && imgRect.height() >= 3.0)
			{
				RegionEntry entry;
				entry.imageRect = imgRect;
				entry.contourRect = contourRectFromImage(imgRect);
				regions_.push_back(entry);
			}

			labelPreview_->displayImage(previewImage_);
			redrawOverlays();
			return true;
		}
		default:
			break;
		}
		return QDialog::eventFilter(obj, event);
	}

	QRectF DxfRegionSelectDialog::imageRectFromWidget(const QPoint& a, const QPoint& b) const
	{
		// imagePosAt 返回 (col, row)，映射为 QRectF 的 (x, y)
		const QPointF pa = labelPreview_->imagePosAt(QPointF(a));
		const QPointF pb = labelPreview_->imagePosAt(QPointF(b));
		return QRectF(qMin(pa.x(), pb.x()), qMin(pa.y(), pb.y()),
			qAbs(pa.x() - pb.x()), qAbs(pa.y() - pb.y()));
	}

	inf::DxfRect DxfRegionSelectDialog::contourRectFromImage(const QRectF& imageRect) const
	{
		using namespace HalconCpp;
		HTuple rOut, cOut;
		AffineTransPoint2d(img2Contour_,
			HTuple(imageRect.top()).TupleConcat(imageRect.bottom()),
			HTuple(imageRect.left()).TupleConcat(imageRect.right()),
			&rOut, &cOut);

		inf::DxfRect rc;
		rc.row1 = rOut[0].D();
		rc.row2 = rOut[1].D();
		rc.col1 = cOut[0].D();
		rc.col2 = cOut[1].D();
		if (rc.row1 > rc.row2) std::swap(rc.row1, rc.row2);
		if (rc.col1 > rc.col2) std::swap(rc.col1, rc.col2);
		return rc;
	}

	void DxfRegionSelectDialog::redrawOverlays()
	{
		if (!labelPreview_ || !labelPreview_->isReady() || !layoutReady_)
			return;

		try
		{
			const HalconCpp::HTuple h = labelPreview_->halconHandle();
			HalconCpp::SetDraw(h, "margin");
			HalconCpp::SetLineWidth(h, 2);

			// 已提交区域：黄色
			HalconCpp::SetColor(h, "yellow");
			for (const auto& r : regions_)
			{
				HalconCpp::DispRectangle1(h,
					r.imageRect.top(), r.imageRect.left(),
					r.imageRect.bottom(), r.imageRect.right());
			}

			// 正在拖拽的框：亮绿（注意 Halcon 颜色名无 "lime"，必须用十六进制）
			if (dragging_)
			{
				HalconCpp::SetColor(h, "#00FF00");
				const QRectF rc = imageRectFromWidget(dragStart_, dragCur_);
				HalconCpp::DispRectangle1(h, rc.top(), rc.left(), rc.bottom(), rc.right());
			}
		}
		catch (...)
		{
		}
	}

	// ===== 按钮 =====

	void DxfRegionSelectDialog::onDrawToggled(bool checked)
	{
		drawMode_ = checked;
		if (!drawMode_)
			dragging_ = false;
	}

	void DxfRegionSelectDialog::onUndo()
	{
		if (regions_.empty())
			return;
		regions_.pop_back();

		labelPreview_->displayImage(previewImage_);
		redrawOverlays();
	}

	void DxfRegionSelectDialog::onConfirm()
	{
		if (!layoutReady_)
		{
			reject();
			return;
		}

		if (regions_.empty())
		{
			// 未绘制区域 = 使用全部轮廓
			selectedContours_ = contours_;
		}
		else
		{
			std::vector<inf::DxfRect> rects;
			rects.reserve(regions_.size());
			for (const auto& r : regions_)
				rects.push_back(r.contourRect);

			inf::filterDxfContoursByRegions(contours_, rects, selectedContours_);

			HalconCpp::HTuple n;
			HalconCpp::CountObj(selectedContours_, &n);
			if (n.I() <= 0)
			{
				rw::rqwu::MessageBox::information(this,
					QStringLiteral("提示"),
					QStringLiteral("所选区域内没有图纸内容，请重新框选。"));
				return;
			}
		}
		accept();
	}

	void DxfRegionSelectDialog::onCancel()
	{
		reject();
	}
}
