#include "Dlg_createshapemodel.h"

#include "func/ProcessModule.hpp"
#include "Modules.hpp"

#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QWheelEvent>
#include "ui_Dlg_jibianjiaozheng.h"

#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

#include "Utilty.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "halconcpp/HalconCpp.h"

#ifdef MessageBox
#undef MessageBox
#endif

#include <imghalcon/imghalcon_core.hpp>
#include <imginterop/imginterop_core.hpp>
#include <imgqt/imgqt_core.hpp>

#include "ProcessParam.hpp"
#include "ImageStitcchingParam.hpp"
#include "rqwu/rqwu_MessageBox.h"
#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"

static int halconCountObj(const HalconCpp::HObject* obj)
{
	if (!obj)
		return 0;
	try
	{
		HalconCpp::HTuple n;
		HalconCpp::CountObj(*obj, &n);
		return n.I();
	}
	catch (...)
	{
		return 0;
	}
}

static QVector<QRectF> exportRectObjToRects(const HalconCpp::HObject* obj)
{
	QVector<QRectF> rois;
	const int count = halconCountObj(obj);
	if (count <= 0)
		return rois;

	rois.reserve(count);
	try
	{
		for (int i = 1; i <= count; ++i)
		{
			HalconCpp::HObject one;
			HalconCpp::SelectObj(*obj, &one, i);

			HalconCpp::HTuple r1, c1, r2, c2;
			HalconCpp::SmallestRectangle1(one, &r1, &c1, &r2, &c2);
			rois.push_back(QRectF(c1.D(), r1.D(), c2.D() - c1.D(), r2.D() - r1.D()).normalized());
		}
	}
	catch (...)
	{
		rois.clear();
	}
	return rois;
}

Dlg_createshapemodel::Dlg_createshapemodel(QWidget* parent)
	: QDialog(parent),
	ui(new Ui::Dlg_createshapemodelClass())
{
	ui->setupUi(this);
	build_ui();
	build_connect();
	isShowImg = true;
}

Dlg_createshapemodel::~Dlg_createshapemodel()
{
	closeHalconWindow();
	
	// 清理双相机缓存
	delete _cam1Buffer;
	delete _cam2Buffer;
	_cam1Buffer = nullptr;
	_cam2Buffer = nullptr;
}

void Dlg_createshapemodel::build_ui()
{
	// 默认不显示XLD的相关参数
	this->ui->widget_XLDHide->setVisible(false);
	ui->widget_contrastHide->setVisible(false);

	auto* old = ui->label_imgDisplay;
	_labelImgDisplaySize = old ? old->size() : QSize();
	// 用 QWidget 承载 Halcon OpenWindow 的父窗口
	_halconHost = new QWidget(old->parentWidget());
	_halconHost->setObjectName(old->objectName());
	_halconHost->setStyleSheet(old->styleSheet());
	_halconHost->setSizePolicy(old->sizePolicy());
	_halconHost->setMinimumSize(old->minimumSize());
	_halconHost->setMaximumSize(old->maximumSize());

	// 让 Qt 创建 native window，确保 winId() 可用
	_halconHost->setAttribute(Qt::WA_NativeWindow, true);
	_halconHost->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

	// 鼠标缩放/拖拽
	_halconHost->setFocusPolicy(Qt::StrongFocus);
	_halconHost->installEventFilter(this);

	if (auto* lay = old->parentWidget() ? old->parentWidget()->layout() : nullptr)
	{
		lay->replaceWidget(old, _halconHost);
	}
	else
	{
		_halconHost->setGeometry(old->geometry());
	}

	_halconHost->show();

	old->hide();
	old->deleteLater();
}

void Dlg_createshapemodel::setRoiEditingUiEnabled(bool enabled)
{
	if (!ui)
		return;

	if (ui->btn_exit) ui->btn_exit->setEnabled(enabled);
	if (ui->btn_readImage) ui->btn_readImage->setEnabled(enabled);
	if (ui->btn_paintRegion) ui->btn_paintRegion->setEnabled(enabled);
	if (ui->btn_paintCenterPoint) ui->btn_paintCenterPoint->setEnabled(enabled);
	if (ui->btn_shiledRegion) ui->btn_shiledRegion->setEnabled(enabled);
	if (ui->btn_createShapeModel) ui->btn_createShapeModel->setEnabled(enabled);
}

void Dlg_createshapemodel::build_connect()
{
	QObject::connect(ui->btn_exit, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_exit_clicked);

	QObject::connect(ui->btn_readImage, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_readImage_clicked);
	QObject::connect(ui->btn_paintRegion, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_paintRegion_clicked);
	QObject::connect(ui->btn_shiledRegion, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_shiledRegion_clicked);
	QObject::connect(ui->btn_createShapeModel, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_createShapeModel_clicked);
	QObject::connect(ui->btn_clearRegion, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_clearRegion_clicked);
	QObject::connect(ui->btn_clearRegion2, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_clearRegion2_clicked);
	QObject::connect(ui->btn_paintCenterPoint, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_paintCenterPoint_clicked);

	QObject::connect(ui->btn_opening, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_opening_clicked);
	QObject::connect(ui->btn_closing, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_closing_clicked);
	QObject::connect(ui->btn_mean, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_mean_clicked);

	QObject::connect(ui->rad_findshapemodel, &QRadioButton::toggled, this, &Dlg_createshapemodel::rad_findshapemodel_toggled);
	QObject::connect(ui->rad_findshapemodelXLD, &QRadioButton::toggled, this, &Dlg_createshapemodel::rad_findshapemodelXLD_toggled);
	
	QObject::connect(ui->rbtn_auto, &QRadioButton::toggled, this, &Dlg_createshapemodel::rbtn_auto_toggled);
	QObject::connect(ui->rbtn_manual, &QRadioButton::toggled, this, &Dlg_createshapemodel::rbtn_manual_toggled);
	QObject::connect(ui->btn_contrast, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_contrast_clicked);
	QObject::connect(ui->btn_mincontrast, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_mincontrast_clicked);


	// checkbox 勾选变化也触发刷新
	if (ui->ckb_opening)
	{
		QObject::connect(ui->ckb_opening, &QCheckBox::toggled, this,
			[this](bool)
			{
				refreshTemplateImage();
			});
	}
	if (ui->ckb_closing)
	{
		QObject::connect(ui->ckb_closing, &QCheckBox::toggled, this,
			[this](bool)
			{
				refreshTemplateImage();
			});
	}
	if (ui->ckb_mean)
	{
		QObject::connect(ui->ckb_mean, &QCheckBox::toggled, this,
			[this](bool)
			{
				refreshTemplateImage();
			});
	}
	// comboBox_ImageType 变化也触发刷新
	if (ui->comboBox_ImageType)
	{
		QObject::connect(ui->comboBox_ImageType,
			QOverload<int>::of(&QComboBox::currentIndexChanged),
			this,
			[this](int)
			{
				refreshTemplateImage();
			});
	}
	QObject::connect(ui->btn_zengyi1, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang1, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_baoguang1_clicked);
	QObject::connect(ui->btn_zengyi2, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_zengyi2_clicked);
	QObject::connect(ui->btn_baoguang2, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_baoguang2_clicked);

	QObject::connect(ui->btn_angle, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_angle_clicked);

	if (ui->btn_MinLength)
		QObject::connect(ui->btn_MinLength, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_MinLength_clicked);
	if (ui->btn_min)
		QObject::connect(ui->btn_min, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_min_clicked);
	if (ui->btn_max)
		QObject::connect(ui->btn_max, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_max_clicked);
	if (ui->btn_createXLD)
		QObject::connect(ui->btn_createXLD, &QPushButton::clicked, this, &Dlg_createshapemodel::btn_createXLD_clicked);

	// 子线程 display 完成后在 UI 线程释放“运行中标记”
	QObject::connect(&_cameraDisplayWatcher, &QFutureWatcher<void>::finished, this,
		[this]()
		{
			_genMapRunning.clear(std::memory_order_release);
			_stitchRunning.clear(std::memory_order_release);
		});
}
bool Dlg_createshapemodel::ensureHalconWindow()
{
	if (!_halconHost)
		return false;

	if (_halconWindowHandle && _halconWindowHandle->TupleLength() > 0)
		return true;

	if (!_halconWindowHandle)
		_halconWindowHandle = new HalconCpp::HTuple();

	QString err;
	const Hlong parentId = static_cast<Hlong>(_halconHost->winId());
	const HalconCpp::HTuple father(parentId);

	// 优先使用当前显示控件（_halconHost，替换自 label_imgDisplay）的实际尺寸。
	// 只有在还未完成布局导致 size 为空时，才回退到缓存的 label_imgDisplay 尺寸。
	QSize hostSize = _halconHost->size();
	if (hostSize.isEmpty())
		hostSize = _labelImgDisplaySize;
	const int hostW = std::max(1, hostSize.width());
	const int hostH = std::max(1, hostSize.height());

	const bool ok = _ProcessModule.OpenHalconWindow(
		0, 0,
		hostW, hostH,
		father,
		"visible",
		*_halconWindowHandle,
		&err);

	if (!ok)
	{
		qWarning() << "OpenHalconWindow failed:" << err;
		return false;
	}

	return true;
}

void Dlg_createshapemodel::closeHalconWindow()
{
	try
	{
		if (_halconWindowHandle && _halconWindowHandle->TupleLength() > 0)
		{
			HalconCpp::CloseWindow(*_halconWindowHandle);
		}
	}
	catch (...)
	{
	}

	delete _halconWindowHandle;
	_halconWindowHandle = nullptr;

	delete _halconLastImage;
	_halconLastImage = nullptr;

	delete _centerPointXldObj;
	_centerPointXldObj = nullptr;

	_viewPartValid = false;
	_viewImgW = 0;
	_viewImgH = 0;
	_isPanning = false;
}

void Dlg_createshapemodel::redrawHalconView(bool clearWindow)
{
	if (!ensureHalconWindow())
		return;
	if (!_halconLastImage)
		return;
	if (!ensureHalconViewPart())
		return;

	const qreal dpr = _halconHost ? _halconHost->devicePixelRatioF() : 1.0;
	const int winW = std::max(1, static_cast<int>(std::lround((_halconHost ? _halconHost->width() : width()) * dpr)));
	const int winH = std::max(1, static_cast<int>(std::lround((_halconHost ? _halconHost->height() : height()) * dpr)));
	try
	{
		HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0, winW, winH);
	}
	catch (...)
	{
	}

	try
	{
		using namespace HalconCpp;
		if (clearWindow)
			ClearWindow(*_halconWindowHandle);

		HalconViewPart partToShow = _viewPart;
		const double partW = partToShow.c2 - partToShow.c1;
		const double partH = partToShow.r2 - partToShow.r1;
		const double eps = 1e-9;
		if (partW > eps && partH > eps)
		{
			const double winAspect = (winH > 0) ? (static_cast<double>(winW) / static_cast<double>(winH)) : 1.0;
			const double partAspect = partW / partH;
			if (winAspect > partAspect)
			{
				const double newW = partH * winAspect;
				const double pad = (newW - partW) * 0.5;
				partToShow.c1 -= pad;
				partToShow.c2 += pad;
			}
			else if (winAspect < partAspect)
			{
				const double newH = partW / winAspect;
				const double pad = (newH - partH) * 0.5;
				partToShow.r1 -= pad;
				partToShow.r2 += pad;
			}
		}

		SetPart(*_halconWindowHandle, partToShow.r1, partToShow.c1, partToShow.r2, partToShow.c2);
		DispObj(*_halconLastImage, *_halconWindowHandle);
	}
	catch (...)
	{
		return;
	}

	try
	{
		using namespace HalconCpp;

		SetDraw(*_halconWindowHandle, "margin");
		SetLineWidth(*_halconWindowHandle, 2);

		auto dispRois = [&](const char* color, const HalconCpp::HObject* obj)
			{
				if (!obj)
					return;

				HTuple n;
				CountObj(*obj, &n);
				const int count = n.I();
				if (count <= 0)
					return;

				SetColor(*_halconWindowHandle, color);

				for (int i = 1; i <= count; ++i)
				{
					HObject one;
					SelectObj(*obj, &one, i);
					DispObj(one, *_halconWindowHandle); // 支持矩形 + 自定义 Region
				}
			};

		dispRois("green", _processParam._paintCreateRoiObj);
		dispRois("red", _processParam._paintShieldRoiObj);

		// 显示第一个匹配轮廓（红色）
		if (_processParam._findCreateXldObj)
		{
			HTuple n;
			CountObj(*_processParam._findCreateXldObj, &n);
			if (n.I() > 0)
			{
				SetColor(*_halconWindowHandle, "green");
				SetLineWidth(*_halconWindowHandle, 2);
				DispObj(*_processParam._findCreateXldObj, *_halconWindowHandle);
			}
		}

		// 显示其他匹配轮廓（红色）
		if (_processParam._findCreateXldObj_Secondary)
		{
			HTuple n;
			CountObj(*_processParam._findCreateXldObj_Secondary, &n);
			if (n.I() > 0)
			{
				SetColor(*_halconWindowHandle, "green");
				SetLineWidth(*_halconWindowHandle, 2);
				DispObj(*_processParam._findCreateXldObj_Secondary, *_halconWindowHandle);
			}
		}

		if (_centerPointXldObj)
		{
			HTuple n;
			CountObj(*_centerPointXldObj, &n);
			if (n.I() > 0)
			{
				SetColor(*_halconWindowHandle, "yellow");
				SetLineWidth(*_halconWindowHandle, 2);
				DispObj(*_centerPointXldObj, *_halconWindowHandle);
			}
		}
	}
	catch (...)
	{
	}
}

bool Dlg_createshapemodel::ensureHalconViewPart()
{
	if (!_halconLastImage)
		return false;

	try
	{
		HalconCpp::HTuple w, h;
		HalconCpp::GetImageSize(*_halconLastImage, &w, &h);
		const int imgW = w.I();
		const int imgH = h.I();
		if (imgW <= 0 || imgH <= 0)
			return false;

		if (!_viewPartValid || imgW != _viewImgW || imgH != _viewImgH)
		{
			_viewImgW = imgW;
			_viewImgH = imgH;
			resetHalconViewPartToFullImage();
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void Dlg_createshapemodel::resetHalconViewPartToFullImage()
{
	_viewPart.r1 = 0.0;
	_viewPart.c1 = 0.0;
	_viewPart.r2 = std::max(0, _viewImgH - 1);
	_viewPart.c2 = std::max(0, _viewImgW - 1);
	_viewPartValid = true;
}

void Dlg_createshapemodel::zoomHalconViewAt(const QPoint& hostPos, int steps)
{
	if (steps == 0)
		return;
	if (!ensureHalconViewPart())
		return;

	const int hostW = std::max(1, _halconHost ? _halconHost->width() : 1);
	const int hostH = std::max(1, _halconHost ? _halconHost->height() : 1);

	const double spanC = _viewPart.c2 - _viewPart.c1;
	const double spanR = _viewPart.r2 - _viewPart.r1;

	const double rx = (hostW > 1) ? (static_cast<double>(hostPos.x()) / static_cast<double>(hostW - 1)) : 0.5;
	const double ry = (hostH > 1) ? (static_cast<double>(hostPos.y()) / static_cast<double>(hostH - 1)) : 0.5;

	const double col = _viewPart.c1 + rx * spanC;
	const double row = _viewPart.r1 + ry * spanR;

	const double base = 1.2;
	const double scale = std::pow(base, -steps);

	double newSpanC = spanC * scale;
	double newSpanR = spanR * scale;

	const double eps = 1e-6;
	if (std::abs(newSpanC) < eps) newSpanC = (newSpanC >= 0.0) ? eps : -eps;
	if (std::abs(newSpanR) < eps) newSpanR = (newSpanR >= 0.0) ? eps : -eps;

	const double fullSpanC = std::max(0, _viewImgW - 1);
	const double fullSpanR = std::max(0, _viewImgH - 1);
	if (newSpanC >= fullSpanC || newSpanR >= fullSpanR)
	{
		resetHalconViewPartToFullImage();
		return;
	}

	_viewPart.c1 = col - rx * newSpanC;
	_viewPart.r1 = row - ry * newSpanR;

	const double maxC1 = fullSpanC - newSpanC;
	const double maxR1 = fullSpanR - newSpanR;
	_viewPart.c1 = std::clamp(_viewPart.c1, 0.0, std::max(0.0, maxC1));
	_viewPart.r1 = std::clamp(_viewPart.r1, 0.0, std::max(0.0, maxR1));

	_viewPart.c2 = _viewPart.c1 + newSpanC;
	_viewPart.r2 = _viewPart.r1 + newSpanR;
	_viewPartValid = true;
}

void Dlg_createshapemodel::panHalconViewFromDrag(const QPoint& dragDelta)
{
	if (!ensureHalconViewPart())
		return;

	const int hostW = std::max(1, _halconHost ? _halconHost->width() : 1);
	const int hostH = std::max(1, _halconHost ? _halconHost->height() : 1);

	const double spanC = _panStartPart.c2 - _panStartPart.c1;
	const double spanR = _panStartPart.r2 - _panStartPart.r1;

	const double dx = static_cast<double>(dragDelta.x());
	const double dy = static_cast<double>(dragDelta.y());

	const double dCol = (hostW > 1) ? (-(dx / static_cast<double>(hostW - 1)) * spanC) : 0.0;
	const double dRow = (hostH > 1) ? (-(dy / static_cast<double>(hostH - 1)) * spanR) : 0.0;

	_viewPart = _panStartPart;
	_viewPart.c1 += dCol;
	_viewPart.c2 += dCol;
	_viewPart.r1 += dRow;
	_viewPart.r2 += dRow;

	const double fullSpanC = std::max(0, _viewImgW - 1);
	const double fullSpanR = std::max(0, _viewImgH - 1);
	const double curSpanC = _viewPart.c2 - _viewPart.c1;
	const double curSpanR = _viewPart.r2 - _viewPart.r1;
	if (curSpanC > 1e-9 && curSpanR > 1e-9)
	{
		const double maxC1 = fullSpanC - curSpanC;
		const double maxR1 = fullSpanR - curSpanR;
		_viewPart.c1 = std::clamp(_viewPart.c1, 0.0, std::max(0.0, maxC1));
		_viewPart.r1 = std::clamp(_viewPart.r1, 0.0, std::max(0.0, maxR1));
		_viewPart.c2 = _viewPart.c1 + curSpanC;
		_viewPart.r2 = _viewPart.r1 + curSpanR;
	}
	_viewPartValid = true;
}

bool Dlg_createshapemodel::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == _halconHost)
	{
		// ROI 绘制期间（DrawRectangle1/DrawRegion 等）不要触发缩放/拖拽
		if (_roiPurpose != ProcessModule::RoiPurpose::None)
			return QDialog::eventFilter(watched, event);

		switch (event->type())
		{
		case QEvent::Wheel:
		{
			auto* e = static_cast<QWheelEvent*>(event);
			const int delta = e->angleDelta().y();
			if (delta == 0)
				return true;

			int steps = delta / 120;
			if (steps == 0)
				steps = (delta > 0) ? 1 : -1;

			zoomHalconViewAt(e->position().toPoint(), steps);
			redrawHalconView(true);
			return true;
		}
		case QEvent::MouseButtonPress:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (e->button() == Qt::LeftButton)
			{
				if (!ensureHalconViewPart())
					return true;

				_isPanning = true;
				_panStartPos = e->pos();
				_panStartPart = _viewPart;
				return true;
			}
			break;
		}
		case QEvent::MouseMove:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (_isPanning && (e->buttons() & Qt::LeftButton))
			{
				const QPoint delta = e->pos() - _panStartPos;
				panHalconViewFromDrag(delta);
				redrawHalconView(true);
				return true;
			}
			break;
		}
		case QEvent::MouseButtonRelease:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (e->button() == Qt::LeftButton)
			{
				_isPanning = false;
				return true;
			}
			break;
		}
		default:
			break;
		}
	}

	return QDialog::eventFilter(watched, event);
}
void Dlg_createshapemodel::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index)
{
	if (!isShowImg)
		return;

	// Mat 转 HImage（在主线程转换，避免线程安全问题）
	HalconCpp::HImage src;
	try {
		src = rw::img::cvMatToHImage(matInfo.mat);
	}
	catch (...) {
		qWarning() << "cvMatToHImage failed.";
		return;
	}

	_isDualCameraMode = true;
	// ========== 双相机拼接模式 ==========
	if (_isDualCameraMode)
	{
		handleDualCameraStitch(src, index);
		return;
	}

	// ========== 原有单相机处理 ==========
	handleSingleCamera(src);
}

void Dlg_createshapemodel::handleDualCameraStitch(const HalconCpp::HImage& src, size_t cameraIndex)
{
	{
		std::lock_guard<std::mutex> lock(_stitchMutex);

		// 缓存当前相机图像
		if (cameraIndex == 1) {
			if (!_cam1Buffer)
				_cam1Buffer = new HalconCpp::HImage();
			*_cam1Buffer = src;
			_cam1Ready = true;
		}
		else if (cameraIndex == 2) {
			if (!_cam2Buffer)
				_cam2Buffer = new HalconCpp::HImage();
			*_cam2Buffer = src;
			_cam2Ready = true;
		}
		else {
			qWarning() << "Unknown camera index:" << cameraIndex;
			return;
		}

		// 检查是否两个相机图像都就绪
		if (!_cam1Ready || !_cam2Ready) {
			return;  // 等待另一个相机
		}
	}

	// 防止并发处理
	if (_stitchRunning.test_and_set(std::memory_order_acquire))
		return;

	// 取出缓存的图像
	HalconCpp::HImage img1, img2;
	{
		std::lock_guard<std::mutex> lock(_stitchMutex);
		img1 = *_cam1Buffer;
		img2 = *_cam2Buffer;
		_cam1Ready = false;
		_cam2Ready = false;
	}

	// 在后台线程执行拼接
	_cameraDisplayWatcher.setFuture(QtConcurrent::run(
		[this, img1, img2]() mutable
		{
			processStitchedImage(std::move(img1), std::move(img2));
		}));
}

void Dlg_createshapemodel::processStitchedImage(HalconCpp::HImage img1, HalconCpp::HImage img2)
{
	// 创建输出对象
	HalconCpp::HObject stitchedObj;

	// 调用拼接（确保之前已调用 calibImage 生成 Map）
	bool ok = _stitchParam.pinjieImage(img1, img2, stitchedObj);

	if (!ok) {
		qWarning() << "pinjieImage failed!";
		QMetaObject::invokeMethod(this, []()
		{
			qWarning() << "图像拼接失败";
		}, Qt::QueuedConnection);
		_stitchRunning.clear(std::memory_order_release);
		return;
	}

	// 转为 HImage
	HalconCpp::HImage stitchedImage(stitchedObj);
	HalconCpp::HImage himage = stitchedImage;

	// 预处理：根据创建模板时的参数进行相同的预处理
	QString err;
	HalconCpp::HImage hGray = himage; // 默认输出：原图

	// 抽取单通道（匹配用的图）
	auto typeFromIndex = [](int idx) -> ProcessModule::SingleChannelType
	{
		using T = ProcessModule::SingleChannelType;
		switch (idx)
		{
		case 0: return T::Gray;
		case 1: return T::R;
		case 2: return T::G;
		case 3: return T::B;
		case 4: return T::H;
		case 5: return T::S;
		case 6: return T::V;
		default: return T::Gray;
		}
	};

	HalconCpp::HImage hImg;
	const auto type = typeFromIndex(_processParam._createModelPreProcessType);
	if (!ProcessModule::ExtractSingleChannel(himage, type, hImg, &err))
	{
		QMetaObject::invokeMethod(this, [err]()
		{
			qWarning() << "ExtractSingleChannel failed:" << err;
		}, Qt::QueuedConnection);
		_stitchRunning.clear(std::memory_order_release);
		return;
	}

	// 按"创建模板时的参数"做同样的预处理
	hGray = hImg;
	if (_processParam._createModelUseOpening && _processParam._createModelOpeningRadius > 0)
	{
		try
		{
			HalconCpp::HImage tmp;
			HalconCpp::GrayDilationRect(
				hGray,
				&tmp,
				HalconCpp::HTuple(_processParam._createModelOpeningRadius),
				HalconCpp::HTuple(_processParam._createModelOpeningRadius));
			hGray = tmp;
		}
		catch (const HalconCpp::HException& e)
		{
			QMetaObject::invokeMethod(this, [e]()
			{
				qWarning() << "GrayDilationRect failed:" << e.ErrorMessage().Text();
			}, Qt::QueuedConnection);
			_stitchRunning.clear(std::memory_order_release);
			return;
		}
	}

	// 闭运算（Closing）- 在 Opening 后面
	if (_processParam._createModelUseClosing && _processParam._createModelClosingRadius > 0)
	{
		try
		{
			HalconCpp::HImage tmp;
			HalconCpp::GrayErosionRect(
				hGray,
				&tmp,
				HalconCpp::HTuple(_processParam._createModelClosingRadius),
				HalconCpp::HTuple(_processParam._createModelClosingRadius));
			hGray = tmp;
		}
		catch (const HalconCpp::HException& e)
		{
			QMetaObject::invokeMethod(this, [e]()
			{
				qWarning() << "GrayErosionRect (Closing) failed:" << e.ErrorMessage().Text();
			}, Qt::QueuedConnection);
			_stitchRunning.clear(std::memory_order_release);
			return;
		}
	}

	if (_processParam._createModelUseMean && _processParam._createModelMeanRadius > 0)
	{
		try
		{
			HalconCpp::HImage tmp;
			HalconCpp::MeanImage(
				hGray,
				&tmp,
				HalconCpp::HTuple(_processParam._createModelMeanRadius),
				HalconCpp::HTuple(_processParam._createModelMeanRadius));
			HalconCpp::GaussFilter(tmp, &tmp, HalconCpp::HTuple(9));
			hGray = tmp;
		}
		catch (const HalconCpp::HException& e)
		{
			QMetaObject::invokeMethod(this, [e]()
			{
				qWarning() << "MeanImage/GaussFilter failed:" << e.ErrorMessage().Text();
			}, Qt::QueuedConnection);
			_stitchRunning.clear(std::memory_order_release);
			return;
		}
	}

	// 更新显示
	QMetaObject::invokeMethod(this,
		[this, himage, hGray]() mutable
	{
		if (!_processParam._originalImage)
			_processParam._originalImage = new HalconCpp::HImage();
		if (!_processParam._templateMatImage)
			_processParam._templateMatImage = new HalconCpp::HImage();

		*_processParam._originalImage = himage;
		*_processParam._templateMatImage = hGray;

		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(hGray);
		else
			*_halconLastImage = hGray;

		redrawHalconView(true);

		// 释放处理标志
		_stitchRunning.clear(std::memory_order_release);
	}, Qt::QueuedConnection);
}

void Dlg_createshapemodel::handleSingleCamera(const HalconCpp::HImage& src)
{
	if (_genMapRunning.test_and_set(std::memory_order_acquire))
		return;

	const int idx = (ui && ui->comboBox_ImageType) ? ui->comboBox_ImageType->currentIndex() : 0;

	_cameraDisplayWatcher.setFuture(QtConcurrent::run(
		[this, src, idx]() mutable
		{
			QString err{};
			HalconCpp::HImage himage = src;

			// 校正到世界平面
			auto& calib = Modules::getInstance().configManagerModule.calibParam1;
			const bool calibValid = (calib.cameraParameters != nullptr) &&
				(calib.cameraPose != nullptr) &&
				(calib.cameraParameters->TupleLength() > 0) &&
				(calib.cameraPose->TupleLength() > 0);

			if (calibValid)
			{
				HalconCpp::HObject rectMap;
				HalconCpp::HImage rectified;
				const bool isok = calib.rectifyImageToWorldPlane(
					src,
					*calib.cameraParameters,
					*calib.cameraPose,
					calib.rectWidthMm,
					rectMap,
					rectified);

				if (isok && rectified.IsInitialized())
				{
					himage = rectified;
				}
				else
				{
					QMetaObject::invokeMethod(this, []()
					{
						qWarning() << "rectifyImageToWorldPlane failed, fallback to original image.";
					}, Qt::QueuedConnection);
				}
			}

			auto typeFromIndex = [](int idx0) -> ProcessModule::SingleChannelType
			{
				using T = ProcessModule::SingleChannelType;
				switch (idx0)
				{
				case 0: return T::Gray;
				case 1: return T::R;
				case 2: return T::G;
				case 3: return T::B;
				case 4: return T::H;
				case 5: return T::S;
				case 6: return T::V;
				default: return T::Gray;
				}
			};

			// 提取单通道
			HalconCpp::HImage hCh;
			if (!ProcessModule::ExtractSingleChannel(himage, typeFromIndex(idx), hCh, &err))
			{
				QMetaObject::invokeMethod(this, [err]()
				{
					qWarning() << "ExtractSingleChannel failed:" << err;
				}, Qt::QueuedConnection);
				return;
			}

			QMetaObject::invokeMethod(this,
				[this, himage, hCh]() mutable
			{
				if (!_processParam._originalImage)
					_processParam._originalImage = new HalconCpp::HImage();
				if (!_processParam._templateMatImage)
					_processParam._templateMatImage = new HalconCpp::HImage();

				// 原图/模板图（单通道）
				*_processParam._originalImage = himage;
				*_processParam._templateMatImage = hCh;

				// 更新 Halcon 显示图像（显示单通道）
				if (!_halconLastImage)
					_halconLastImage = new HalconCpp::HImage(hCh);
				else
					*_halconLastImage = hCh;

				redrawHalconView(true);
			},
			Qt::QueuedConnection);
		}));
}

void Dlg_createshapemodel::clearStitchBuffers()
{
	std::lock_guard<std::mutex> lock(_stitchMutex);
	delete _cam1Buffer;
	delete _cam2Buffer;
	_cam1Buffer = nullptr;
	_cam2Buffer = nullptr;
	_cam1Ready = false;
	_cam2Ready = false;
}

void Dlg_createshapemodel::btn_exit_clicked()
{
	const bool hasModel = (_processParam.hv_ModelID && _processParam.hv_ModelID->Length() > 0);
	// 未创建模板（没有 ModelID）则直接退出，不提示保存
	if (!hasModel)
	{
		close();
		return;
	}

	// 如果本次会话还没保存过模型，退出时提示是否保存
	if (!_modelSaved)
	{
		const auto ret = QMessageBox::question(
			this,
			"提示",
			"是否要保存创建的模型和参数？",
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
			QMessageBox::Yes);

		if (ret == QMessageBox::Cancel)
			return;

		if (ret == QMessageBox::Yes)
		{
			if (!saveModelData())
				return;
		}
	}

	close();
}

void Dlg_createshapemodel::btn_readImage_clicked()
{
	readImage();
}

void Dlg_createshapemodel::btn_paintRegion_clicked()
{
	paintCreateRegion();
}


void Dlg_createshapemodel::btn_shiledRegion_clicked()
{
	paintShieldRegion();
}

void Dlg_createshapemodel::btn_createShapeModel_clicked()
{
	const bool isCreate = createShapeModel();
	if (!isCreate)
		return;

	// === 新增：创建完成后，自动在“当前绘制的创建ROI”中心生成中心点 ===
	try
	{
		if (_processParam._paintCreateRoiObj && halconCountObj(_processParam._paintCreateRoiObj) > 0)
		{
			using namespace HalconCpp;

			HObject hoCreateUnion;
			Union1(*_processParam._paintCreateRoiObj, &hoCreateUnion);

			HTuple r1, c1, r2, c2;
			SmallestRectangle1(hoCreateUnion, &r1, &c1, &r2, &c2);

			if (r1.TupleLength() > 0 && c1.TupleLength() > 0 &&
				r2.TupleLength() > 0 && c2.TupleLength() > 0)
			{
				const double row = (r1[0].D() + r2[0].D()) * 0.5;
				const double col = (c1[0].D() + c2[0].D()) * 0.5;

				// 保存默认中心点（像素坐标：X=col, Y=row）
				_processParam.centerX = col;
				_processParam.centerY = row;

				// 绘制一个十字作为中心点标记（替换旧中心点）
				HObject hoCross;
				GenCrossContourXld(&hoCross, row, col, 30.0, 0.0);

				if (!_centerPointXldObj)
					_centerPointXldObj = new HObject();
				*_centerPointXldObj = hoCross;

				redrawHalconView(true);
			}
		}
	}
	catch (...)
	{
		// 自动生成中心点失败不影响创建流程
	}
}

void Dlg_createshapemodel::btn_clearRegion_clicked()
{
	ProcessParam::clearCreateRegionAndXld(_processParam);
	_roiPurpose = ProcessModule::RoiPurpose::None;

	delete _centerPointXldObj;
	_centerPointXldObj = nullptr;

	setRoiEditingUiEnabled(true);
	isShowImg = true;

	redrawHalconView(true);
}

void Dlg_createshapemodel::btn_paintCenterPoint_clicked()
{
	isShowImg = false;

	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		isShowImg = true;
		return;
	}
	if (!ensureHalconWindow())
	{
		isShowImg = true;
		return;
	}

	setRoiEditingUiEnabled(false);

	try
	{
		using namespace HalconCpp;

		HTuple w, h;
		GetImageSize(*_processParam._templateMatImage, &w, &h);
		if (w.I() <= 0 || h.I() <= 0)
		{
			setRoiEditingUiEnabled(true);
			isShowImg = true;
			return;
		}

		HTuple row, col;
		DrawPoint(*_halconWindowHandle, &row, &col);

		const double x = col.D();
		const double y = row.D();
		if (x < 0.0 || y < 0.0 || x >= static_cast<double>(w.I()) || y >= static_cast<double>(h.I()))
		{
			rw::rqwu::MessageBox::warning(this, "提示", "中心点超出图像范围。");
			setRoiEditingUiEnabled(true);
			isShowImg = true;
			return;
		}

		// 保存用户设置的中心点（像素坐标：X=col, Y=row）
		_processParam.centerX = x;
		_processParam.centerY = y;

		// 绘制一个十字作为中心点标记
		HObject hoCross;
		GenCrossContourXld(&hoCross, row, col, 30.0, 0.0);

		// 如果上一次已绘制中心点，则直接替换（避免越画越多）
		if (!_centerPointXldObj)
			_centerPointXldObj = new HObject();
		*_centerPointXldObj = hoCross;

		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...)
	{
	}

	setRoiEditingUiEnabled(true);
	isShowImg = true;

}

void Dlg_createshapemodel::rad_findshapemodel_toggled(bool checked)
{
	if (checked)
	{
		this->ui->widget_XLDHide->setVisible(false);
	}
}

void Dlg_createshapemodel::rad_findshapemodelXLD_toggled(bool checked)
{
	if (checked)
	{
		this->ui->widget_XLDHide->setVisible(true);
	}
}

void Dlg_createshapemodel::btn_MinLength_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept != QDialog::Accepted)
		return;

	const QString value = numKeyBord.getValue();
	bool ok = false;
	const double v = value.toDouble(&ok);
	if (!ok || v <= 0.0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请输入大于0的数值");
		return;
	}

	_processParam.xld_Minlength = v;
	if (ui && ui->btn_MinLength)
		ui->btn_MinLength->setText(value);
}

void Dlg_createshapemodel::btn_min_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept != QDialog::Accepted)
		return;

	const QString value = numKeyBord.getValue();
	bool ok = false;
	const double v = value.toDouble(&ok);
	if (!ok || v < 0.0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请输入大于等于0的数值");
		return;
	}
	if (v >= _processParam.xld_max)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "min 必须小于 max");
		return;
	}

	_processParam.xld_min = v;
	if (ui && ui->btn_min)
		ui->btn_min->setText(value);
}

void Dlg_createshapemodel::btn_max_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept != QDialog::Accepted)
		return;

	const QString value = numKeyBord.getValue();
	bool ok = false;
	const double v = value.toDouble(&ok);
	if (!ok || v < 0.0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请输入大于等于0的数值");
		return;
	}
	if (v <= _processParam.xld_min)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "max 必须大于 min");
		return;
	}

	_processParam.xld_max = v;
	if (ui && ui->btn_max)
		ui->btn_max->setText(value);
}

void Dlg_createshapemodel::btn_createXLD_clicked()
{
	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}
	if (!_processParam._paintCreateRoiObj || halconCountObj(_processParam._paintCreateRoiObj) <= 0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先绘制ROI。");
		return;
	}

	try
	{
		using namespace HalconCpp;

		HObject hoCreateUnion;
		Union1(*_processParam._paintCreateRoiObj, &hoCreateUnion);

		HObject hoFinal = hoCreateUnion;
		if (_processParam._paintShieldRoiObj && halconCountObj(_processParam._paintShieldRoiObj) > 0)
		{
			HObject hoShieldUnion;
			Union1(*_processParam._paintShieldRoiObj, &hoShieldUnion);

			HObject hoDiff;
			Difference(hoCreateUnion, hoShieldUnion, &hoDiff);
			hoFinal = hoDiff;
		}

		HImage imgReduced;
		ReduceDomain(*_processParam._templateMatImage, hoFinal, &imgReduced);

		HXLDCont ho_Edges;
		HXLDCont ho_SelectedXLD;
		EdgesSubPix(imgReduced, &ho_Edges, "canny", 0.5, _processParam.xld_min, _processParam.xld_max);
		SelectShapeXld(ho_Edges, &ho_SelectedXLD, "contlength", "and", _processParam.xld_Minlength, 5000000);

		HTuple n;
		CountObj(ho_SelectedXLD, &n);

		if (!_processParam._findCreateXldObj)
			_processParam._findCreateXldObj = new HObject();

		if (n.I() > 0)
		{
			*_processParam._findCreateXldObj = ho_SelectedXLD;
		}
		else
		{
			GenEmptyObj(_processParam._findCreateXldObj);
			rw::rqwu::MessageBox::information(this, "提示", "未找到满足条件的XLD轮廓。\n请检查 MinLength/min/max 参数。");
		}

		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
}

void Dlg_createshapemodel::rbtn_auto_toggled(bool checked)
{
	if (checked)
	{
		ui->widget_contrastHide->setVisible(false);
	}
}

void Dlg_createshapemodel::rbtn_manual_toggled(bool checked)
{
	if (checked)
	{
		ui->widget_contrastHide->setVisible(true);
	}
}

void Dlg_createshapemodel::btn_contrast_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept != QDialog::Accepted)
		return;

	const QString value = numKeyBord.getValue();

	if (ui && ui->btn_contrast)
		ui->btn_contrast->setText(value);
}

void Dlg_createshapemodel::btn_mincontrast_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept != QDialog::Accepted)
		return;

	const QString value = numKeyBord.getValue();

	if (ui && ui->btn_mincontrast)
		ui->btn_mincontrast->setText(value);
}

void Dlg_createshapemodel::btn_clearRegion2_clicked()
{
	using namespace HalconCpp;

	auto popLastFrom = [](HalconCpp::HObject* obj) -> bool
		{
			if (!obj)
				return false;

			HTuple n;
			CountObj(*obj, &n);
			const int count = n.I();
			if (count <= 0)
				return false;

			if (count == 1)
			{
				GenEmptyObj(obj);
				return true;
			}

			HObject out;
			GenEmptyObj(&out);

			bool hasAny = false;
			for (int i = 1; i <= count - 1; ++i)
			{
				HObject one;
				SelectObj(*obj, &one, i);
				if (!hasAny)
				{
					out = one;
					hasAny = true;
				}
				else
				{
					HObject tmp;
					ConcatObj(out, one, &tmp);
					out = tmp;
				}
			}

			*obj = out;
			return true;
		};

	if (_lastPaintPurpose != ProcessModule::RoiPurpose::Create &&
		_lastPaintPurpose != ProcessModule::RoiPurpose::Shield)
	{
		rw::rqwu::MessageBox::information(this, "提示", "没有可撤销的ROI（请先绘制）。");
		return;
	}

	HalconCpp::HObject* target = nullptr;
	if (_lastPaintPurpose == ProcessModule::RoiPurpose::Create)
	{
		if (!_processParam._paintCreateRoiObj)
			_processParam._paintCreateRoiObj = new HalconCpp::HObject();
		target = _processParam._paintCreateRoiObj;
	}
	else
	{
		if (!_processParam._paintShieldRoiObj)
			_processParam._paintShieldRoiObj = new HalconCpp::HObject();
		target = _processParam._paintShieldRoiObj;
	}

	if (!popLastFrom(target))
	{
		rw::rqwu::MessageBox::information(this, "提示", "当前没有可删除的ROI。");
		return;
	}

	redrawHalconView(true);
}


void Dlg_createshapemodel::btn_zengyi1_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入大于0的数值");
			return;
		}
		ui->btn_zengyi1->setText(value);
		auto& camera1 = Modules::getInstance().cameraModule.camera1;
		if (camera1)
		{
			camera1->setGain(static_cast<size_t>(value.toDouble()));
		}
	}
}

void Dlg_createshapemodel::btn_baoguang1_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入大于0的数值");
			return;
		}
		ui->btn_baoguang1->setText(value);
		auto& camera1 = Modules::getInstance().cameraModule.camera1;
		if (camera1)
		{
			camera1->setExposureTime(static_cast<size_t>(value.toDouble()));
		}
	}
}

void Dlg_createshapemodel::btn_zengyi2_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入大于0的数值");
			return;
		}
		ui->btn_zengyi2->setText(value);
		auto& camera2 = Modules::getInstance().cameraModule.camera2;
		if (camera2)
		{
			camera2->setGain(static_cast<size_t>(value.toDouble()));
		}
	}
}

void Dlg_createshapemodel::btn_baoguang2_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入大于0的数值");
			return;
		}
		ui->btn_baoguang2->setText(value);
		auto& camera2 = Modules::getInstance().cameraModule.camera2;
		if (camera2)
		{
			camera2->setExposureTime(static_cast<size_t>(value.toDouble()));
		}
	}
}

void Dlg_createshapemodel::btn_angle_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < -360 || value.toDouble() > 360)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入-360~360的数值");
			return;
		}
		ui->btn_angle->setText(value);
	}
}

void Dlg_createshapemodel::btn_opening_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 3 || value.toDouble() > 30)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入3~30的数值");
			return;
		}
		ui->btn_opening->setText(value);

		// 数值变化后刷新图像
		refreshTemplateImage();
	}
}

void Dlg_createshapemodel::btn_closing_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 3 || value.toDouble() > 30)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入3~30的数值");
			return;
		}
		ui->btn_closing->setText(value);

		// 数值变化后刷新图像
		refreshTemplateImage();
	}
}

void Dlg_createshapemodel::btn_mean_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 3 || value.toDouble() > 30)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入-360~360的数值");
			return;
		}
		ui->btn_mean->setText(value);

		// 数值变化后刷新图像
		refreshTemplateImage();
	}
}

bool Dlg_createshapemodel::saveModelData()
{
	const QString ts = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
	const QString currentRootPath = globalPath.modelRootPath + "\\" + ts + "\\";

	_processParam.modelPath = currentRootPath;

	// 新创建的模型，offset重置为0
	_processParam.offsetX = 0.0;
	_processParam.offsetY = 0.0;
	_processParam.offsetAngle = 0.0;

	_processParam.save(currentRootPath.toStdString());
	auto& processParam = Modules::getInstance().configManagerModule.processParam1;
	processParam = _processParam;

	RunningInfo newModelInfo{};
	newModelInfo.baseInfo.name = ts.toStdString();
	newModelInfo.baseInfo.trainDate = ts.toStdString();
	newModelInfo.cameraSet.exposureTime1 = static_cast<size_t>(ui->btn_baoguang1->text().toInt());
	newModelInfo.cameraSet.gain1 = static_cast<size_t>(ui->btn_zengyi1->text().toInt());
	newModelInfo.cameraSet.exposureTime2 = static_cast<size_t>(ui->btn_baoguang2->text().toInt());
	newModelInfo.cameraSet.gain2 = static_cast<size_t>(ui->btn_zengyi2->text().toInt());
	newModelInfo.filePath.modelPath = QString(currentRootPath + globalPath.modelName).toStdString();
	newModelInfo.filePath.prePareImgPath = QString(currentRootPath + globalPath.prepareImgName).toStdString();
	newModelInfo.filePath.trainsouceImgPath = QString(currentRootPath + globalPath.trainSourceImgName).toStdString();
	newModelInfo.lightSet = Modules::getInstance().configManagerModule.runningInfo.lightSet;
	newModelInfo.modelInfo.preProcessType = ui->comboBox_ImageType->currentIndex();
	Modules::getInstance().configManagerModule.addModel(newModelInfo, ts.toStdString());

	rw::rqwu::MessageBox::information(this, "成功", "创建成功");
	_modelSaved = true;

	return true;
}

void Dlg_createshapemodel::refreshTemplateImage()
{
	if (!_processParam._originalImage)
		return;

	try
	{
		HalconCpp::HTuple w0, h0;
		HalconCpp::GetImageSize(*_processParam._originalImage, &w0, &h0);
		if (w0.I() <= 0 || h0.I() <= 0)
			return;
	}
	catch (...)
	{
		return;
	}

	auto typeFromIndex = [](int idx) -> ProcessModule::SingleChannelType
		{
			using T = ProcessModule::SingleChannelType;
			switch (idx)
			{
			case 0: return T::Gray;
			case 1: return T::R;
			case 2: return T::G;
			case 3: return T::B;
			case 4: return T::H;
			case 5: return T::S;
			case 6: return T::V;
			default: return T::Gray;
			}
		};

	const int idx = (ui && ui->comboBox_ImageType) ? ui->comboBox_ImageType->currentIndex() : 0;

	HalconCpp::HImage hCh;
	QString err;
	if (!ProcessModule::ExtractSingleChannel(*_processParam._originalImage, typeFromIndex(idx), hCh, &err))
	{
		qWarning() << "refreshTemplateImage ExtractSingleChannel failed:" << err;
		return;
	}

	HalconCpp::HImage hProcessed = hCh;

	// 膨胀（沿用你的 UI：ckb_opening / btn_opening）
	const bool useOpening = (ui && ui->ckb_opening && ui->ckb_opening->isChecked());
	int openingRadius = 0;
	if (useOpening)
	{
		bool ok = false;
		openingRadius = (ui && ui->btn_opening) ? ui->btn_opening->text().toInt(&ok) : 0;
		if (ok && openingRadius > 0)
		{
			try
			{
				HalconCpp::HImage tmp;
				HalconCpp::GrayDilationRect(
					hProcessed,
					&tmp,
					HalconCpp::HTuple(openingRadius),
					HalconCpp::HTuple(openingRadius));
				hProcessed = tmp;
			}
			catch (const HalconCpp::HException& e)
			{
				qWarning() << "GrayDilationRect failed:" << QString::fromLocal8Bit(e.ErrorMessage().Text());
			}
		}
	}

	// 闭运算（Closing）- 在 Opening 后面
	const bool useClosing = (ui && ui->ckb_closing && ui->ckb_closing->isChecked());
	int closingRadius = 0;
	if (useClosing)
	{
		bool ok = false;
		closingRadius = (ui && ui->btn_closing) ? ui->btn_closing->text().toInt(&ok) : 0;
		if (ok && closingRadius > 0)
		{
			try
			{
				HalconCpp::HImage tmp;
				HalconCpp::GrayErosionRect(
					hProcessed,
					&tmp,
					HalconCpp::HTuple(closingRadius),
					HalconCpp::HTuple(closingRadius));
				hProcessed = tmp;
			}
			catch (const HalconCpp::HException& e)
			{
				qWarning() << "GrayErosionRect (Closing) failed:" << QString::fromLocal8Bit(e.ErrorMessage().Text());
			}
		}
	}

	// 均值滤波（ckb_mean / btn_mean）
	const bool useMean = (ui && ui->ckb_mean && ui->ckb_mean->isChecked());
	int meanRadius = 0;
	if (useMean)
	{
		bool ok = false;
		meanRadius = (ui && ui->btn_mean) ? ui->btn_mean->text().toInt(&ok) : 0;
		if (ok && meanRadius > 0)
		{
			try
			{
				HalconCpp::HImage tmp;
				HalconCpp::MeanImage(hProcessed, &tmp,
					HalconCpp::HTuple(meanRadius),
					HalconCpp::HTuple(meanRadius));
				hProcessed = tmp;
			}
			catch (const HalconCpp::HException& e)
			{
				qWarning() << "MeanImage failed:" << QString::fromLocal8Bit(e.ErrorMessage().Text());
			}
		}
	}

	// 记录参数（用于后续保存/回显）
	_processParam._createModelPreProcessType = idx;
	_processParam._createModelUseOpening = useOpening;
	_processParam._createModelOpeningRadius = openingRadius;
	_processParam._createModelUseClosing = useClosing;
	_processParam._createModelClosingRadius = closingRadius;
	_processParam._createModelUseMean = useMean;
	_processParam._createModelMeanRadius = meanRadius;

	if (!_processParam._templateMatImage)
		_processParam._templateMatImage = new HalconCpp::HImage();
	*_processParam._templateMatImage = hProcessed;

	if (!_halconLastImage)
		_halconLastImage = new HalconCpp::HImage(hProcessed);
	else
		*_halconLastImage = hProcessed;

	redrawHalconView(true);
}


void Dlg_createshapemodel::showEvent(QShowEvent* show_event)
{
	QDialog::showEvent(show_event);

	_modelSaved = false;
	isShowImg = true;
	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = true;

	// 进入建模界面：切换相机到实时出图（触发关闭/FreeRun）
	auto& camMod = Modules::getInstance().cameraModule;
	if (camMod.camera1)
		camMod.camera1->startMonitor();
	lastIsCamera1SoftTrigger = camMod.isCamera1SoftTrigger.load();
	lastIsCamera2SoftTrigger = camMod.isCamera2SoftTrigger.load();
	camMod.setCamera1TriggerOff();
	camMod.setCamera2TriggerOff();

	// 加载图像拼接参数
	_stitchParam = Modules::getInstance().configManagerModule.imageStitchingParam;

	ui->btn_angle->setText(QString::number(0));
	auto& cfg = Modules::getInstance().configManagerModule;
	auto& runningInfo = cfg.runningInfo;
	auto& curParam = cfg.processParam1;

	_processParam.offsetX = curParam.offsetX;
	_processParam.offsetY = curParam.offsetY;
	_processParam.offsetAngle = curParam.offsetAngle;
	_processParam.upperLightOn = curParam.upperLightOn;
	_processParam.lowerLightOn = curParam.lowerLightOn;
	_processParam._SingleChannelType = curParam._SingleChannelType;
	_processParam._createModelExposureTime = curParam._createModelExposureTime;
	_processParam._createModelGain = curParam._createModelGain;
	_processParam._createModelPreProcessType = curParam._createModelPreProcessType;
	_processParam._createModelUseOpening = curParam._createModelUseOpening;
	_processParam._createModelOpeningRadius = curParam._createModelOpeningRadius;
	_processParam._createModelUseClosing = curParam._createModelUseClosing;
	_processParam._createModelClosingRadius = curParam._createModelClosingRadius;
	_processParam._createModelUseMean = curParam._createModelUseMean;
	_processParam._createModelMeanRadius = curParam._createModelMeanRadius;
	_processParam.isSelectXLDToCreateModel = curParam.isSelectXLDToCreateModel;
	_processParam.xld_Minlength = curParam.xld_Minlength;
	_processParam.xld_min = curParam.xld_min;
	_processParam.xld_max = curParam.xld_max;

	if (ui->comboBox_ImageType)
		ui->comboBox_ImageType->setCurrentIndex(curParam._createModelPreProcessType);
	if (ui->ckb_opening)
		ui->ckb_opening->setChecked(curParam._createModelUseOpening);
	if (ui->btn_opening)
		ui->btn_opening->setText(QString::number(curParam._createModelOpeningRadius));
	if (ui->ckb_closing)
		ui->ckb_closing->setChecked(curParam._createModelUseClosing);
	if (ui->btn_closing)
		ui->btn_closing->setText(QString::number(curParam._createModelClosingRadius));
	if (ui->ckb_mean)
		ui->ckb_mean->setChecked(curParam._createModelUseMean);
	if (ui->btn_mean)
		ui->btn_mean->setText(QString::number(curParam._createModelMeanRadius));
	if (ui->btn_MinLength)
		ui->btn_MinLength->setText(QString::number(curParam.xld_Minlength));
	if (ui->btn_min)
		ui->btn_min->setText(QString::number(curParam.xld_min));
	if (ui->btn_max)
		ui->btn_max->setText(QString::number(curParam.xld_max));

	ui->btn_baoguang1->setText(QString::number(runningInfo.cameraSet.exposureTime1));
	ui->btn_zengyi1->setText(QString::number(runningInfo.cameraSet.gain1));
	ui->btn_baoguang2->setText(QString::number(runningInfo.cameraSet.exposureTime2));
	ui->btn_zengyi2->setText(QString::number(runningInfo.cameraSet.gain2));
	_processParam._createModelExposureTime = static_cast<double>(runningInfo.cameraSet.exposureTime1);
	_processParam._createModelGain = static_cast<double>(runningInfo.cameraSet.gain1);
	if (camMod.camera1)
	{
		camMod.camera1->setExposureTime(static_cast<size_t>(runningInfo.cameraSet.exposureTime1));
		camMod.camera1->setGain(static_cast<size_t>(runningInfo.cameraSet.gain1));
	}

	ProcessParam::clearCreateRegionAndXld(_processParam);
	_roiPurpose = ProcessModule::RoiPurpose::None;

	// 延迟到布局完成后再创建/调整 Halcon 窗口，避免初次创建时尺寸偏小。
	QTimer::singleShot(0, this, [this]()
		{
			if (_halconHost)
				_labelImgDisplaySize = _halconHost->size();
			if (!ensureHalconWindow())
				return;
			try
			{
				HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0,
					std::max(1, _halconHost->width()),
					std::max(1, _halconHost->height()));
			}
			catch (...)
			{
			}
			redrawHalconView(true);
		});
}

void Dlg_createshapemodel::closeEvent(QCloseEvent* close_event)
{
	if (!lastIsCamera1SoftTrigger)
	{
		Modules::getInstance().cameraModule.setCamera1HardwareTrigger();
	}
	else
	{
		Modules::getInstance().cameraModule.setCamera1TriggerOff();
	}
	if (!lastIsCamera2SoftTrigger)
	{
		Modules::getInstance().cameraModule.setCamera2HardwareTrigger();
	}
	else
	{
		Modules::getInstance().cameraModule.setCamera2TriggerOff();
	}
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = true;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = false;

	// 清理双相机缓存
	clearStitchBuffers();
	_isDualCameraMode = false;

	closeHalconWindow();
}

void Dlg_createshapemodel::resizeEvent(QResizeEvent* e)
{
	QDialog::resizeEvent(e);

	if (!ensureHalconWindow())
		return;

	try
	{
		HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0,
			std::max(1, _halconHost->width()),
			std::max(1, _halconHost->height()));
		redrawHalconView(false);
	}
	catch (...)
	{
	}
}

void Dlg_createshapemodel::readImage()
{
	// 选择 Halcon 可读的图像（tif/tiff 常用）
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"选择图片",
		QString(),
		"Image Files (*.tif *.tiff *.png *.bmp *.jpg *.jpeg);;All Files (*.*)");

	if (filePath.isEmpty())
		return;

	if (!ensureHalconWindow())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "Halcon窗体创建失败，无法显示图片。");
		return;
	}

	try
	{
		// 1) 读取 Halcon 图像
		HalconCpp::HImage hImg;
		HalconCpp::ReadImage(&hImg, filePath.toStdString().c_str());

		// 2) 保存原图到 ProcessParam（Halcon 格式）
		if (!_processParam._originalImage)
			_processParam._originalImage = new HalconCpp::HImage();
		*_processParam._originalImage = hImg;

		// 3) 根据下拉框选择通道
		auto typeFromIndex = [](int idx) -> ProcessModule::SingleChannelType
			{
				using T = ProcessModule::SingleChannelType;
				switch (idx)
				{
				case 0: return T::Gray;
				case 1: return T::R;
				case 2: return T::G;
				case 3: return T::B;
				case 4: return T::H;
				case 5: return T::S;
				case 6: return T::V;
				default: return T::Gray;
				}
			};

		const int idx = (ui && ui->comboBox_ImageType) ? ui->comboBox_ImageType->currentIndex() : 0;

		// 4) 提取单通道（Halcon 输入/输出）
		HalconCpp::HImage hCh;
		QString err;
		if (!ProcessModule::ExtractSingleChannel(hImg, typeFromIndex(idx), hCh, &err))
		{
			rw::rqwu::MessageBox::warning(this, "提示", QString("通道转换失败：%1").arg(err));
			return;
		}

		// 5) 根据 checkbox 进行开运算/均值处理（对单通道图处理）
		HalconCpp::HImage hProcessed = hCh;

		const bool useOpening = (ui && ui->ckb_opening && ui->ckb_opening->isChecked());
		int openingRadius = 0;
		if (useOpening)
		{
			bool ok = false;
			openingRadius = (ui && ui->btn_opening) ? ui->btn_opening->text().toInt(&ok) : 0;
			if (!ok || openingRadius <= 0)
			{
				rw::rqwu::MessageBox::warning(this, "提示", "开运算半径无效，已跳过开运算。");
			}
			else
			{
				try
				{
					HalconCpp::HImage tmp;

					// 按你的要求：GrayDilationRect 替换 OpeningCircle
					HalconCpp::GrayDilationRect(
						hProcessed,
						&tmp,
						HalconCpp::HTuple(openingRadius),
						HalconCpp::HTuple(openingRadius));

					hProcessed = tmp;
				}
				catch (const HalconCpp::HException& e)
				{
					rw::rqwu::MessageBox::warning(this, "Halcon异常",
						QString("开运算失败：%1").arg(QString::fromLocal8Bit(e.ErrorMessage().Text())));
				}
			}
		}

		const bool useMean = (ui && ui->ckb_mean && ui->ckb_mean->isChecked());
		int meanRadius = 0;
		if (useMean)
		{
			bool ok = false;
			meanRadius = (ui && ui->btn_mean) ? ui->btn_mean->text().toInt(&ok) : 0;
			if (!ok || meanRadius <= 0)
			{
				rw::rqwu::MessageBox::warning(this, "提示", "均值半径无效，已跳过均值滤波。");
			}
			else
			{
				try
				{
					HalconCpp::HImage tmp;
					HalconCpp::MeanImage(hProcessed, &tmp, HalconCpp::HTuple(meanRadius), HalconCpp::HTuple(meanRadius));
					hProcessed = tmp;
				}
				catch (const HalconCpp::HException& e)
				{
					rw::rqwu::MessageBox::warning(this, "Halcon异常",
						QString("均值滤波失败：%1").arg(QString::fromLocal8Bit(e.ErrorMessage().Text())));
				}
			}
		}

		// 6) 保存处理参数（用于后续 save/load 或建模记录）
		_processParam._createModelPreProcessType = idx;
		_processParam._createModelUseOpening = useOpening;
		_processParam._createModelOpeningRadius = openingRadius;
		_processParam._createModelUseMean = useMean;
		_processParam._createModelMeanRadius = meanRadius;

		// 7) 模板图（单通道，且已预处理）
		if (!_processParam._templateMatImage)
			_processParam._templateMatImage = new HalconCpp::HImage();
		*_processParam._templateMatImage = hProcessed;

		// 8) 显示也切到处理后的单通道
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(hProcessed);
		else
			*_halconLastImage = hProcessed;

		// 9) 清空已有 ROI/叠加（避免旧数据干扰）
		ProcessParam::clearCreateRegionAndXld(_processParam);
		_roiPurpose = ProcessModule::RoiPurpose::None;

		// 10) 重绘
		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...)
	{
		rw::rqwu::MessageBox::warning(this, "错误", "读取/显示图片失败（未知异常）。");
	}
}

void Dlg_createshapemodel::paintCreateRegion()
{
	isShowImg = false;

	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}
	if (!ensureHalconWindow())
		return;

	setRoiEditingUiEnabled(false);
	_roiPurpose = ProcessModule::RoiPurpose::Create;

	try
	{
		using namespace HalconCpp;

		const bool drawRectangle = (ui && ui->rad_rectangle && ui->rad_rectangle->isChecked());

		HObject roiObj;
		HTuple w, h;
		GetImageSize(*_processParam._templateMatImage, &w, &h);

		if (drawRectangle)
		{
			HTuple row1, col1, row2, col2;
			DrawRectangle1(*_halconWindowHandle, &row1, &col1, &row2, &col2);

			const QRectF roi(col1.D(), row1.D(), col2.D() - col1.D(), row2.D() - row1.D());
			const QString vErr = ProcessModule::validateRois(QVector<QRectF>{ roi }, w.I(), h.I());
			if (!vErr.isEmpty())
			{
				rw::rqwu::MessageBox::warning(this, "ROI错误", vErr);
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}

			GenRectangle1(&roiObj, roi.top(), roi.left(), roi.bottom(), roi.right());
		}
		else
		{
			// 自定义区域：draw_polygon 直接返回 Region
			DrawRegion(&roiObj, *_halconWindowHandle);

			HTuple r1, c1, r2, c2;
			SmallestRectangle1(roiObj, &r1, &c1, &r2, &c2);
			const QRectF bbox(c1.D(), r1.D(), c2.D() - c1.D(), r2.D() - r1.D());

			const QString vErr = ProcessModule::validateRois(QVector<QRectF>{ bbox }, w.I(), h.I());
			if (!vErr.isEmpty())
			{
				rw::rqwu::MessageBox::warning(this, "ROI错误", vErr);
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}

			HTuple area, rr, cc;
			AreaCenter(roiObj, &area, &rr, &cc);
			if (area.TupleLength() <= 0 || area.D() <= 1.0)
			{
				rw::rqwu::MessageBox::warning(this, "提示", "绘制区域为空或过小。");
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}
		}

		if (!_processParam._paintCreateRoiObj)
			_processParam._paintCreateRoiObj = new HObject();

		HTuple n;
		CountObj(*_processParam._paintCreateRoiObj, &n);
		if (n.I() <= 0)
		{
			*_processParam._paintCreateRoiObj = roiObj;
		}
		else
		{
			HObject out;
			ConcatObj(*_processParam._paintCreateRoiObj, roiObj, &out);
			*_processParam._paintCreateRoiObj = out;
		}

		_lastPaintPurpose = ProcessModule::RoiPurpose::Create;
		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...)
	{
	}

	_roiPurpose = ProcessModule::RoiPurpose::None;
	setRoiEditingUiEnabled(true);
}

void Dlg_createshapemodel::paintShieldRegion()
{
	isShowImg = false;

	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}
	if (!ensureHalconWindow())
		return;

	setRoiEditingUiEnabled(false);
	_roiPurpose = ProcessModule::RoiPurpose::Shield;

	try
	{
		using namespace HalconCpp;

		const bool drawRectangle = (ui && ui->rad_rectangle && ui->rad_rectangle->isChecked());

		HObject roiObj;
		HTuple w, h;
		GetImageSize(*_processParam._templateMatImage, &w, &h);

		if (drawRectangle)
		{
			HTuple row1, col1, row2, col2;
			DrawRectangle1(*_halconWindowHandle, &row1, &col1, &row2, &col2);

			const QRectF roi(col1.D(), row1.D(), col2.D() - col1.D(), row2.D() - row1.D());
			const QString vErr = ProcessModule::validateRois(QVector<QRectF>{ roi }, w.I(), h.I());
			if (!vErr.isEmpty())
			{
				rw::rqwu::MessageBox::warning(this, "ROI错误", vErr);
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}

			GenRectangle1(&roiObj, roi.top(), roi.left(), roi.bottom(), roi.right());
		}
		else
		{
			// 自定义区域：draw_polygon 直接返回 Region
			DrawRegion(&roiObj ,*_halconWindowHandle);

			HTuple r1, c1, r2, c2;
			SmallestRectangle1(roiObj, &r1, &c1, &r2, &c2);
			const QRectF bbox(c1.D(), r1.D(), c2.D() - c1.D(), r2.D() - r1.D());

			const QString vErr = ProcessModule::validateRois(QVector<QRectF>{ bbox }, w.I(), h.I());
			if (!vErr.isEmpty())
			{
				rw::rqwu::MessageBox::warning(this, "ROI错误", vErr);
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}

			HTuple area, rr, cc;
			AreaCenter(roiObj, &area, &rr, &cc);
			if (area.TupleLength() <= 0 || area.D() <= 1.0)
			{
				rw::rqwu::MessageBox::warning(this, "提示", "绘制区域为空或过小。");
				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
				return;
			}
		}

		if (!_processParam._paintShieldRoiObj)
			_processParam._paintShieldRoiObj = new HObject();

		HTuple n;
		CountObj(*_processParam._paintShieldRoiObj, &n);
		if (n.I() <= 0)
		{
			*_processParam._paintShieldRoiObj = roiObj;
		}
		else
		{
			HObject out;
			ConcatObj(*_processParam._paintShieldRoiObj, roiObj, &out);
			*_processParam._paintShieldRoiObj = out;
		}

		_lastPaintPurpose = ProcessModule::RoiPurpose::Shield;
		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...)
	{
	}

	_roiPurpose = ProcessModule::RoiPurpose::None;
	setRoiEditingUiEnabled(true);
}
// ====== 下面 createShapeModelFromRois / createShapeModel 保持你原逻辑不变 ======
// 仅去掉了原来依赖 panZoomLabel 的显示层逻辑（你要显示轮廓的话，需要另外用 Halcon 方式 DispObj XLD）


bool Dlg_createshapemodel::createShapeModelFromRois(ProcessParam& _processParam)
{
	try
	{
		if (!_processParam._templateMatImage)
			return false;

		const HalconCpp::HImage baseTemplate = *_processParam._templateMatImage;

		if (!_processParam._paintCreateRoiObj || halconCountObj(_processParam._paintCreateRoiObj) <= 0)
			return false;

		HalconCpp::HObject hoCreate = *_processParam._paintCreateRoiObj;
		HalconCpp::HObject hoCreateUnion;
		HalconCpp::Union1(hoCreate, &hoCreateUnion);

		HalconCpp::HObject hoFinal = hoCreateUnion;
		if (_processParam._paintShieldRoiObj && halconCountObj(_processParam._paintShieldRoiObj) > 0)
		{
			HalconCpp::HObject hoShield = *_processParam._paintShieldRoiObj;
			HalconCpp::HObject hoShieldUnion;
			HalconCpp::Union1(hoShield, &hoShieldUnion);

			HalconCpp::HObject hoDiff;
			HalconCpp::Difference(hoCreateUnion, hoShieldUnion, &hoDiff);
			hoFinal = hoDiff;
		}

		HalconCpp::HImage imgToReduce = baseTemplate;

		const bool useOpening = (ui && ui->ckb_opening && ui->ckb_opening->isChecked());
		if (useOpening)
		{
			bool ok = false;
			const int openingRadius = (ui && ui->btn_opening) ? ui->btn_opening->text().toInt(&ok) : 0;
			if (ok && openingRadius > 0)
			{
				try
				{
					HalconCpp::HImage tmp;
					HalconCpp::GrayDilationRect(
						imgToReduce,
						&tmp,
						HalconCpp::HTuple(openingRadius),
						HalconCpp::HTuple(openingRadius));
					imgToReduce = tmp;
				}
				catch (const HalconCpp::HException& e)
				{
					qWarning() << "createShapeModelFromRois GrayDilationRect failed:"
						<< QString::fromLocal8Bit(e.ErrorMessage().Text());
				}
			}
		}

		const bool useMean = (ui && ui->ckb_mean && ui->ckb_mean->isChecked());
		if (useMean)
		{
			bool ok = false;
			const int meanRadius = (ui && ui->btn_mean) ? ui->btn_mean->text().toInt(&ok) : 0;
			if (ok && meanRadius > 0)
			{
				try
				{
					HalconCpp::HImage tmp;
					HalconCpp::MeanImage(
						imgToReduce,
						&tmp,
						HalconCpp::HTuple(meanRadius),
						HalconCpp::HTuple(meanRadius));
					HalconCpp::GaussFilter(tmp, &tmp, HalconCpp::HTuple(9));
					imgToReduce = tmp;
				}
				catch (const HalconCpp::HException& e)
				{
					qWarning() << "createShapeModelFromRois MeanImage/GaussFilter failed:"
						<< QString::fromLocal8Bit(e.ErrorMessage().Text());
				}
			}
		}

		HalconCpp::HImage imgReduced;
		HalconCpp::ReduceDomain(imgToReduce, hoFinal, &imgReduced);

		//// 保存 reduce 后的图像到桌面（用于调试确认 ROI / Shield 效果）
		//try
		//{
		//	const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
		//	if (!desktop.isEmpty())
		//	{
		//		const QString filePath = QDir(desktop)
		//			.filePath(QString("imgReduced_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")));
		//		HalconCpp::WriteImage(imgReduced, "jpg", 0, filePath.toStdString().c_str());
		//		qInfo() << "imgReduced saved to:" << filePath;
		//	}
		//}
		//catch (...)
		//{
		//}

		const bool useXldModel = (ui && ui->rad_findshapemodelXLD && ui->rad_findshapemodelXLD->isChecked());
		_processParam.isSelectXLDToCreateModel = useXldModel;

		HalconCpp::HTuple modelIdLocal;
		if (!useXldModel)
		{
			if (ui && ui->rbtn_manual && ui->rbtn_manual->isChecked())
			{
				auto contrast = (ui && ui->btn_contrast) ? ui->btn_contrast->text().toDouble() : 0.0;
				auto minContrast = (ui && ui->btn_mincontrast) ? ui->btn_mincontrast->text().toDouble() : 0.0;

				HalconCpp::CreateShapeModel(
					imgReduced,
					"auto",
					0, HalconCpp::HTuple(360).TupleRad(),
					"auto",
					"auto",
					"use_polarity",
					contrast,
					minContrast,
					&modelIdLocal);
			}
			else
			{
				HalconCpp::CreateShapeModel(
					imgReduced,
					"auto",
					0, HalconCpp::HTuple(360).TupleRad(),
					"auto",
					"auto",
					"use_polarity",
					"auto",
					"auto",
					&modelIdLocal);
			}
		}
		else
		{
			// 新方法：基于 XLD（EdgesSubPix + SelectShapeXld）创建模板
			HalconCpp::HXLDCont ho_Edges;
			HalconCpp::HXLDCont ho_SelectedXLD;

			HalconCpp::EdgesSubPix(imgReduced, &ho_Edges, "canny", 0.5, _processParam.xld_min, _processParam.xld_max);
			HalconCpp::SelectShapeXld(ho_Edges, &ho_SelectedXLD, "contlength", "and", _processParam.xld_Minlength, 5000);

			HalconCpp::HTuple n;
			HalconCpp::CountObj(ho_SelectedXLD, &n);
			if (n.I() <= 0)
			{
				rw::rqwu::MessageBox::warning(this, "提示", "未找到满足条件的XLD轮廓，无法创建模板。\n请检查 MinLength/min/max 参数。");
				return false;
			}

			HalconCpp::CreateShapeModelXld(
				ho_SelectedXLD,
				"auto",
				-0.39,
				0.79,
				"auto",
				"auto",
				"ignore_local_polarity",
				5,
				&modelIdLocal);
		}

		delete _processParam.hv_ModelID;
		_processParam.hv_ModelID = new HalconCpp::HTuple(modelIdLocal);

		HalconCpp::HTuple hv_Row, hv_Column, hv_Angle, hv_Score;
		HalconCpp::FindShapeModel(
			imgToReduce,
			*_processParam.hv_ModelID,
			0, HalconCpp::HTuple(360).TupleRad(),
			0.1,
			1,
			0.5,
			"least_squares",
			0,
			0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		if (hv_Row.TupleLength() < 1)
			return false;

		// 保存匹配到的中心点坐标（Halcon: Row=Y, Column=X）
		_processParam.findCenterX = (hv_Column.TupleLength() > 0) ? hv_Column[0].D() : 0.0;
		_processParam.findCenterY = (hv_Row.TupleLength() > 0) ? hv_Row[0].D() : 0.0;
		// 注意：centerX/centerY 是用户自定义中心点（创建模板时绘制的点），不要在建模时覆盖

		HalconCpp::HObject ho_ModelContours;
		HalconCpp::HObject ho_ContoursAffineTrans;
		HalconCpp::HTuple hv_HomMat2D;

		HalconCpp::GetShapeModelContours(&ho_ModelContours, *_processParam.hv_ModelID, 1);
		HalconCpp::VectorAngleToRigid(0, 0, 0, hv_Row[0], hv_Column[0], hv_Angle[0], &hv_HomMat2D);
		HalconCpp::AffineTransContourXld(ho_ModelContours, &ho_ContoursAffineTrans, hv_HomMat2D);

		if (!_processParam._findCreateXldObj)
			_processParam._findCreateXldObj = new HalconCpp::HObject();
		*_processParam._findCreateXldObj = ho_ContoursAffineTrans;

		if (!_processParam._findCreateXldObj_Secondary)
			_processParam._findCreateXldObj_Secondary = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(_processParam._findCreateXldObj_Secondary);

		redrawHalconView(true);

		const double score = (hv_Score.TupleLength() > 0) ? hv_Score[0].D() : 0.0;
		rw::rqwu::MessageBox::information(this, "提示", QString("建模成功，初次匹配score=%1").arg(score));
		return true;
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::critical(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	return false;
}

bool Dlg_createshapemodel::createShapeModel()
{
	if (!_processParam._paintCreateRoiObj || halconCountObj(_processParam._paintCreateRoiObj) <= 0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先绘制ROI。");
		return false;
	}

	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return false;
	}


	if (!createShapeModelFromRois(_processParam))
	{
		return false;
	}
	else
	{


		return true;
	}

}
















