#include "ui/Dlg_ImageStitching.h"
#include "ui_Dlg_ImageStitching.h"

#include "data/CalibParam.hpp"
#include "data/ImageStitcchingParam.hpp"
#include "Modules.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

#include <algorithm>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <halconcpp/HalconCpp.h>

#ifdef MessageBox
#undef MessageBox
#endif

#include <imghalcon/imghalcon_core.hpp>

#include "Utilty.hpp"
#include "rqwu/rqwu_MessageBox.h"
#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"

Dlg_ImageStitching::Dlg_ImageStitching(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_ImageStitchingClass())
{
	ui->setupUi(this);

	build_ui();
	build_connect();
}

Dlg_ImageStitching::~Dlg_ImageStitching()
{
	closeAllHalconWindows();

	delete _camera1Image;
	delete _camera2Image;

	delete ui;
}

// 辅助函数：创建 Halcon 宿主窗口
static QWidget* createHalconHost(QLabel* oldLabel, QObject* eventFilterTarget)
{
	auto* host = new QWidget(oldLabel->parentWidget());
	host->setObjectName(oldLabel->objectName());
	host->setStyleSheet(oldLabel->styleSheet());
	host->setSizePolicy(oldLabel->sizePolicy());
	host->setMinimumSize(oldLabel->minimumSize());
	host->setMaximumSize(oldLabel->maximumSize());

	// 让 Qt 创建 native window，确保 winId() 可用
	host->setAttribute(Qt::WA_NativeWindow, true);
	host->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

	// 鼠标缩放/拖拽
	host->setFocusPolicy(Qt::StrongFocus);
	host->installEventFilter(eventFilterTarget);

	if (auto* lay = oldLabel->parentWidget() ? oldLabel->parentWidget()->layout() : nullptr)
	{
		lay->replaceWidget(oldLabel, host);
	}
	else
	{
		host->setGeometry(oldLabel->geometry());
	}

	host->show();
	oldLabel->hide();
	oldLabel->deleteLater();

	return host;
}

void Dlg_ImageStitching::build_ui()
{
	// 创建三个 Halcon 窗口宿主
	_win1.host = createHalconHost(ui->label_imgDisplay1, this);
	_win1.originalSize = _win1.host->size();

	_win2.host = createHalconHost(ui->label_imgDisplay2, this);
	_win2.originalSize = _win2.host->size();

	_winMain.host = createHalconHost(ui->label_imgDisplay, this);
	_winMain.originalSize = _winMain.host->size();
}

void Dlg_ImageStitching::build_connect()
{
	QObject::connect(ui->btn_exit, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_exit_clicked);

	QObject::connect(ui->btn_takePicture, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_takePicture_clicked);
	QObject::connect(ui->btn_zengyi1, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang1, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_baoguang1_clicked);
	QObject::connect(ui->btn_zengyi2, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_zengyi2_clicked);
	QObject::connect(ui->btn_baoguang2, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_baoguang2_clicked);

	QObject::connect(ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked);

	QObject::connect(ui->btn_caiqiebili, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_caiqiebili_clicked);

	QObject::connect(ui->btn_buchang1, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_buchang1_clicked);
	QObject::connect(ui->btn_buchang2, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_buchang2_clicked);
	QObject::connect(ui->btn_test, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_test_clicked);

	QObject::connect(ui->btn_yongshi, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_yongshi_clicked);
	QObject::connect(ui->btn_pinjieguocheng, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_pinjieguocheng_clicked);

	QObject::connect(ui->btn_xinjianpinjieliaohao, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_xinjianpinjieliaohao_clicked);
	QObject::connect(ui->btn_liaohaomingcheng, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_liaohaomingcheng_clicked);
	QObject::connect(ui->btn_xiugaimingcheng, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_xiugaimingcheng_clicked);
	QObject::connect(ui->btn_delete, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_delete_clicked);
	QObject::connect(ui->btn_deleteAll, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_deleteAll_clicked);
	QObject::connect(ui->btn_dangqianxuanzedepinjieliaohao, &QPushButton::clicked, this, &Dlg_ImageStitching::btn_dangqianxuanzedepinjieliaohao_clicked);
}

// ========== Halcon 窗口相关方法 ==========

Dlg_ImageStitching::HalconWindowData* Dlg_ImageStitching::getWindowData(DisplayWindow win)
{
	switch (win)
	{
	case DisplayWindow::Window1: return &_win1;
	case DisplayWindow::Window2: return &_win2;
	case DisplayWindow::WindowMain: return &_winMain;
	}
	return nullptr;
}

bool Dlg_ImageStitching::ensureHalconWindow(DisplayWindow win)
{
	auto* data = getWindowData(win);
	if (!data || !data->host)
		return false;

	if (data->windowHandle && data->windowHandle->TupleLength() > 0)
		return true;

	if (!data->windowHandle)
		data->windowHandle = new HalconCpp::HTuple();

	QString err;
	const Hlong parentId = static_cast<Hlong>(data->host->winId());
	const HalconCpp::HTuple father(parentId);

	// 优先使用当前显示控件的实际尺寸
	QSize hostSize = data->host->size();
	if (hostSize.isEmpty())
		hostSize = data->originalSize;
	const int hostW = std::max(1, hostSize.width());
	const int hostH = std::max(1, hostSize.height());

	// 使用 ProcessModule 的 OpenHalconWindow
	auto& processModule = _ProcessModule;
	const bool ok = processModule.OpenHalconWindow(
		0, 0,
		hostW, hostH,
		father,
		"visible",
		*data->windowHandle,
		&err);

	if (!ok)
	{
		qWarning() << "OpenHalconWindow failed for window" << static_cast<int>(win) << ":" << err;
		return false;
	}

	return true;
}

void Dlg_ImageStitching::closeHalconWindow(DisplayWindow win)
{
	auto* data = getWindowData(win);
	if (!data) return;

	try
	{
		if (data->windowHandle && data->windowHandle->TupleLength() > 0)
		{
			HalconCpp::CloseWindow(*data->windowHandle);
		}
	}
	catch (...)
	{
	}

	delete data->windowHandle;
	data->windowHandle = nullptr;

	delete data->lastImage;
	data->lastImage = nullptr;

	data->viewPartValid = false;
	data->viewImgW = 0;
	data->viewImgH = 0;
	data->isPanning = false;
}

void Dlg_ImageStitching::closeAllHalconWindows()
{
	closeHalconWindow(DisplayWindow::Window1);
	closeHalconWindow(DisplayWindow::Window2);
	closeHalconWindow(DisplayWindow::WindowMain);
}

void Dlg_ImageStitching::redrawHalconView(DisplayWindow win, bool clearWindow)
{
	auto* data = getWindowData(win);
	if (!data) return;

	if (!ensureHalconWindow(win))
		return;
	if (!data->lastImage)
		return;
	if (!ensureHalconViewPart(win))
		return;

	const qreal dpr = data->host ? data->host->devicePixelRatioF() : 1.0;
	const int winW = std::max(1, static_cast<int>(std::lround((data->host ? data->host->width() : width()) * dpr)));
	const int winH = std::max(1, static_cast<int>(std::lround((data->host ? data->host->height() : height()) * dpr)));

	try
	{
		HalconCpp::SetWindowExtents(*data->windowHandle, 0, 0, winW, winH);
	}
	catch (...)
	{
	}

	try
	{
		using namespace HalconCpp;
		if (clearWindow)
			ClearWindow(*data->windowHandle);

		auto partToShow = data->viewPart;
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

		SetPart(*data->windowHandle, partToShow.r1, partToShow.c1, partToShow.r2, partToShow.c2);
		DispObj(*data->lastImage, *data->windowHandle);
	}
	catch (...)
	{
		return;
	}
}

bool Dlg_ImageStitching::ensureHalconViewPart(DisplayWindow win)
{
	auto* data = getWindowData(win);
	if (!data || !data->lastImage)
		return false;

	try
	{
		HalconCpp::HTuple w, h;
		HalconCpp::GetImageSize(*data->lastImage, &w, &h);
		const int imgW = w.I();
		const int imgH = h.I();
		if (imgW <= 0 || imgH <= 0)
			return false;

		if (!data->viewPartValid || imgW != data->viewImgW || imgH != data->viewImgH)
		{
			data->viewImgW = imgW;
			data->viewImgH = imgH;
			resetHalconViewPartToFullImage(win);
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void Dlg_ImageStitching::resetHalconViewPartToFullImage(DisplayWindow win)
{
	auto* data = getWindowData(win);
	if (!data) return;

	data->viewPart.r1 = 0.0;
	data->viewPart.c1 = 0.0;
	data->viewPart.r2 = std::max(0, data->viewImgH - 1);
	data->viewPart.c2 = std::max(0, data->viewImgW - 1);
	data->viewPartValid = true;
}

void Dlg_ImageStitching::zoomHalconViewAt(DisplayWindow win, const QPoint& hostPos, int steps)
{
	auto* data = getWindowData(win);
	if (!data) return;

	if (steps == 0)
		return;
	if (!ensureHalconViewPart(win))
		return;

	const int hostW = std::max(1, data->host ? data->host->width() : 1);
	const int hostH = std::max(1, data->host ? data->host->height() : 1);

	const double spanC = data->viewPart.c2 - data->viewPart.c1;
	const double spanR = data->viewPart.r2 - data->viewPart.r1;

	const double rx = (hostW > 1) ? (static_cast<double>(hostPos.x()) / static_cast<double>(hostW - 1)) : 0.5;
	const double ry = (hostH > 1) ? (static_cast<double>(hostPos.y()) / static_cast<double>(hostH - 1)) : 0.5;

	const double col = data->viewPart.c1 + rx * spanC;
	const double row = data->viewPart.r1 + ry * spanR;

	const double base = 1.2;
	const double scale = std::pow(base, -steps);

	double newSpanC = spanC * scale;
	double newSpanR = spanR * scale;

	const double eps = 1e-6;
	if (std::abs(newSpanC) < eps) newSpanC = (newSpanC >= 0.0) ? eps : -eps;
	if (std::abs(newSpanR) < eps) newSpanR = (newSpanR >= 0.0) ? eps : -eps;

	const double fullSpanC = std::max(0, data->viewImgW - 1);
	const double fullSpanR = std::max(0, data->viewImgH - 1);
	if (newSpanC >= fullSpanC || newSpanR >= fullSpanR)
	{
		resetHalconViewPartToFullImage(win);
		return;
	}

	data->viewPart.c1 = col - rx * newSpanC;
	data->viewPart.r1 = row - ry * newSpanR;

	const double maxC1 = fullSpanC - newSpanC;
	const double maxR1 = fullSpanR - newSpanR;
	data->viewPart.c1 = std::clamp(data->viewPart.c1, 0.0, std::max(0.0, maxC1));
	data->viewPart.r1 = std::clamp(data->viewPart.r1, 0.0, std::max(0.0, maxR1));

	data->viewPart.c2 = data->viewPart.c1 + newSpanC;
	data->viewPart.r2 = data->viewPart.r1 + newSpanR;
	data->viewPartValid = true;
}

void Dlg_ImageStitching::panHalconViewFromDrag(DisplayWindow win, const QPoint& dragDelta)
{
	auto* data = getWindowData(win);
	if (!data) return;

	if (!ensureHalconViewPart(win))
		return;

	const int hostW = std::max(1, data->host ? data->host->width() : 1);
	const int hostH = std::max(1, data->host ? data->host->height() : 1);

	const double spanC = data->panStartPart.c2 - data->panStartPart.c1;
	const double spanR = data->panStartPart.r2 - data->panStartPart.r1;

	const double dx = static_cast<double>(dragDelta.x());
	const double dy = static_cast<double>(dragDelta.y());

	const double dCol = (hostW > 1) ? (-(dx / static_cast<double>(hostW - 1)) * spanC) : 0.0;
	const double dRow = (hostH > 1) ? (-(dy / static_cast<double>(hostH - 1)) * spanR) : 0.0;

	data->viewPart = data->panStartPart;
	data->viewPart.c1 += dCol;
	data->viewPart.c2 += dCol;
	data->viewPart.r1 += dRow;
	data->viewPart.r2 += dRow;

	const double fullSpanC = std::max(0, data->viewImgW - 1);
	const double fullSpanR = std::max(0, data->viewImgH - 1);
	const double curSpanC = data->viewPart.c2 - data->viewPart.c1;
	const double curSpanR = data->viewPart.r2 - data->viewPart.r1;
	if (curSpanC > 1e-9 && curSpanR > 1e-9)
	{
		const double maxC1 = fullSpanC - curSpanC;
		const double maxR1 = fullSpanR - curSpanR;
		data->viewPart.c1 = std::clamp(data->viewPart.c1, 0.0, std::max(0.0, maxC1));
		data->viewPart.r1 = std::clamp(data->viewPart.r1, 0.0, std::max(0.0, maxR1));
		data->viewPart.c2 = data->viewPart.c1 + curSpanC;
		data->viewPart.r2 = data->viewPart.r1 + curSpanR;
	}
	data->viewPartValid = true;
}

void Dlg_ImageStitching::setDisplayImage(DisplayWindow win, const HalconCpp::HImage& image)
{
	auto* data = getWindowData(win);
	if (!data) return;

	if (!data->lastImage)
		data->lastImage = new HalconCpp::HImage(image);
	else
		*data->lastImage = image;

	// 重置视图区域并刷新显示
	data->viewPartValid = false;
	redrawHalconView(win, true);
}

bool Dlg_ImageStitching::eventFilter(QObject* watched, QEvent* event)
{
	// 确定哪个窗口触发了事件
	DisplayWindow activeWin = DisplayWindow::WindowMain;
	HalconWindowData* data = nullptr;

	if (watched == _win1.host)
	{
		activeWin = DisplayWindow::Window1;
		data = &_win1;
	}
	else if (watched == _win2.host)
	{
		activeWin = DisplayWindow::Window2;
		data = &_win2;
	}
	else if (watched == _winMain.host)
	{
		activeWin = DisplayWindow::WindowMain;
		data = &_winMain;
	}
	else
	{
		return QDialog::eventFilter(watched, event);
	}

	_currentActiveWindow = activeWin;

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

		zoomHalconViewAt(activeWin, e->position().toPoint(), steps);
		redrawHalconView(activeWin, true);
		return true;
	}
	case QEvent::MouseButtonPress:
	{
		auto* e = static_cast<QMouseEvent*>(event);
		if (e->button() == Qt::LeftButton)
		{
			if (!ensureHalconViewPart(activeWin))
				return true;

			data->isPanning = true;
			data->panStartPos = e->pos();
			data->panStartPart = data->viewPart;
			return true;
		}
		break;
	}
	case QEvent::MouseMove:
	{
		auto* e = static_cast<QMouseEvent*>(event);
		if (data->isPanning && (e->buttons() & Qt::LeftButton))
		{
			const QPoint delta = e->pos() - data->panStartPos;
			panHalconViewFromDrag(activeWin, delta);
			redrawHalconView(activeWin, true);
			return true;
		}
		break;
	}
	case QEvent::MouseButtonRelease:
	{
		auto* e = static_cast<QMouseEvent*>(event);
		if (e->button() == Qt::LeftButton)
		{
			data->isPanning = false;
			return true;
		}
		break;
	}
	default:
		break;
	}

	return QDialog::eventFilter(watched, event);
}

// ========== 事件处理 ==========

void Dlg_ImageStitching::showEvent(QShowEvent* show_event)
{
	QDialog::showEvent(show_event);

	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = true;

	auto& imageStitchingParam = Modules::getInstance().configManagerModule.imageStitchingParam;
	ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->setText(QString::number(imageStitchingParam.DiffHeight*1000));
	ui->btn_caiqiebili->setText(QString::number(imageStitchingParam.OverlapInPercent));

	_stitchParam = imageStitchingParam;

	ui->btn_baoguang1->setText(QString::number(30000));
	ui->btn_zengyi1->setText(QString::number(1));
	ui->btn_baoguang2->setText(QString::number(30000));
	ui->btn_zengyi2->setText(QString::number(1));

	auto& camModule = Modules::getInstance().cameraModule;
	camModule.setCamera1ExposureTime(30000);
	camModule.setCamera1Gain(1);
	camModule.setCamera2ExposureTime(30000);
	camModule.setCamera2Gain(1);

	lastIsCamera1SoftTrigger = camModule.isCamera1SoftTrigger.load();
	lastIsCamera2SoftTrigger = camModule.isCamera2SoftTrigger.load();

	camModule.setCamera1TriggerOff();
	camModule.setCamera2TriggerOff();

	// 延迟到布局完成后再创建/调整 Halcon 窗口
	QTimer::singleShot(0, this, [this]()
		{
			// 更新原始尺寸
			if (_win1.host) _win1.originalSize = _win1.host->size();
			if (_win2.host) _win2.originalSize = _win2.host->size();
			if (_winMain.host) _winMain.originalSize = _winMain.host->size();

			// 初始化三个 Halcon 窗口
			auto initWindow = [this](DisplayWindow win)
				{
					auto* data = getWindowData(win);
					if (!data || !data->host) return;

					if (!ensureHalconWindow(win))
						return;

					try
					{
						HalconCpp::SetWindowExtents(*data->windowHandle, 0, 0,
							std::max(1, data->host->width()),
							std::max(1, data->host->height()));

						// 设置窗口背景色为黑色
						HalconCpp::SetWindowParam(*data->windowHandle, "background_color", "black");
						HalconCpp::ClearWindow(*data->windowHandle);
					}
					catch (...)
					{
					}
				};

			initWindow(DisplayWindow::Window1);
			initWindow(DisplayWindow::Window2);
			initWindow(DisplayWindow::WindowMain);

			// 刷新显示（如果有图像的话）
			redrawHalconView(DisplayWindow::Window1, true);
			redrawHalconView(DisplayWindow::Window2, true);
			redrawHalconView(DisplayWindow::WindowMain, true);
		});
}

void Dlg_ImageStitching::resizeEvent(QResizeEvent* resize_event)
{
	QDialog::resizeEvent(resize_event);

	auto resizeWindow = [this](DisplayWindow win)
		{
			auto* data = getWindowData(win);
			if (!data || !data->host) return;

			if (!ensureHalconWindow(win))
				return;

			try
			{
				HalconCpp::SetWindowExtents(*data->windowHandle, 0, 0,
					std::max(1, data->host->width()),
					std::max(1, data->host->height()));
				redrawHalconView(win, false);
			}
			catch (...)
			{
			}
		};

	resizeWindow(DisplayWindow::Window1);
	resizeWindow(DisplayWindow::Window2);
	resizeWindow(DisplayWindow::WindowMain);
}

void Dlg_ImageStitching::closeEvent(QCloseEvent* close_event)
{
	QDialog::closeEvent(close_event);

	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = true;

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

	closeAllHalconWindows();
}

// ========== 按钮槽函数 ==========

void Dlg_ImageStitching::btn_exit_clicked()
{
	const auto ret = QMessageBox::question(
		this,
		"提示",
		"是否要保存参数？",
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
		QMessageBox::Yes);

	if (ret == QMessageBox::Cancel)
		return;

	if (ret == QMessageBox::Yes)
	{
		auto& config = Modules::getInstance().configManagerModule;
		config.imageStitchingParam = _stitchParam;
		config.imageStitchingParam.save(globalPath.imageStitchingParamPath.toStdString());
	}

	this->close();
}

void Dlg_ImageStitching::btn_takePicture_clicked()
{
	// 1. 准备两个相机的标定参数
	_cam1Calib = Modules::getInstance().configManagerModule.calibParam1;
	_cam2Calib = Modules::getInstance().configManagerModule.calibParam2;

	// 使用相机图像或选择图片
	if (!_camera1Image || !_camera2Image)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先获取相机图像");
		return;
	}

	_stitchParam.camera1Piccture = _camera1Image;
	_stitchParam.camera2Piccture = _camera2Image;
	_stitchParam.caltabDescrPath = globalPath.describeBoardFile.toStdString();
	_stitchParam.DiffHeight = 0.0;
	_stitchParam.OverlapInPercent = ui->btn_caiqiebili->text().toDouble();
	_stitchParam.BorderInPercent = 7.0;
	// 从相机参数估算“世界平面米/像素”(pixTowWorld)，不要用 sx(像元大小)
	bool pixTowWorldValid = false;
	try
	{
		if (_cam1Calib.cameraParameters && _cam1Calib.cameraPose &&
			_cam1Calib.cameraParameters->TupleLength() >= 13 &&
			_cam1Calib.cameraPose->TupleLength() > 0)
		{
			// 取图像中心附近，避免畸变边缘影响；deltaPx 取一个较小的像素间隔
			const double imgW = (*_cam1Calib.cameraParameters)[11].D();
			const double imgH = (*_cam1Calib.cameraParameters)[12].D();
			const double row0 = imgH * 0.5;
			const double col0 = imgW * 0.5;
			const double deltaPx = 200.0;

			HalconCpp::HTuple x0, y0, x1, y1, x2, y2;

			// (row0,col0) 与 (row0, col0+deltaPx) -> 求 X 方向每像素对应的米
			HalconCpp::ImagePointsToWorldPlane(
				*_cam1Calib.cameraParameters, *_cam1Calib.cameraPose,
				row0, col0, "m", &x0, &y0);

			HalconCpp::ImagePointsToWorldPlane(
				*_cam1Calib.cameraParameters, *_cam1Calib.cameraPose,
				row0, col0 + deltaPx, "m", &x1, &y1);

			// (row0,col0) 与 (row0+deltaPx, col0) -> 求 Y 方向每像素对应的米
			HalconCpp::ImagePointsToWorldPlane(
				*_cam1Calib.cameraParameters, *_cam1Calib.cameraPose,
				row0 + deltaPx, col0, "m", &x2, &y2);

			if (x0.TupleLength() > 0 && y0.TupleLength() > 0 &&
				x1.TupleLength() > 0 && y1.TupleLength() > 0 &&
				x2.TupleLength() > 0 && y2.TupleLength() > 0)
			{
				const double dx = x1[0].D() - x0[0].D();
				const double dy = y1[0].D() - y0[0].D();
				const double distX = std::sqrt(dx * dx + dy * dy);

				const double ex = x2[0].D() - x0[0].D();
				const double ey = y2[0].D() - y0[0].D();
				const double distY = std::sqrt(ex * ex + ey * ey);

				const double scaleX = distX / deltaPx; // m/px
				const double scaleY = distY / deltaPx; // m/px

				// 简单平均（也可以只用 scaleX 或 scaleY）
				const double scale = (scaleX + scaleY) * 0.5;

				if (scale > 0.0)
				{
					_stitchParam.pixTowWorld = scale;
					pixTowWorldValid = true;
				}
			}
		}
	}
	catch (...)
	{
		pixTowWorldValid = false;
	}

	if (!pixTowWorldValid)
	{
		_stitchParam.pixTowWorld = 0.0001; // 默认值
		rw::rqwu::MessageBox::warning(this, "提示",
			"无法从标定参数估算有效的世界平面像素当量(m/px)，将使用默认值 0.0001\n"
			"请检查相机1标定参数(cameraParameters/cameraPose)是否完整。");
	}
	_stitchParam.DistancePlates = 0.0;

	// 3. 计算映射图
	std::string errorMsg;
	if (_stitchParam.calibImage(_cam1Calib, _cam2Calib, &errorMsg))
	{
		// 显示拼接结果
		HalconCpp::HObject stitchedImage;
		if (_stitchParam.pinjieImage(*_camera1Image, *_camera2Image, stitchedImage))
		{
			// 显示拼接结果到主窗口
			HalconCpp::HImage stitchedHImage(stitchedImage);
			setDisplayImage(DisplayWindow::WindowMain, stitchedHImage);
		}
	}
	else
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString::fromStdString(errorMsg));
	}
}

void Dlg_ImageStitching::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index)
{
	// 将 Mat 转换为 Halcon 图像并保存
	try
	{
		HalconCpp::HImage hImg;
		// 假设有 Mat 到 HImage 的转换函数，这里需要根据你的实际情况调整
		hImg = rw::img::cvMatToHImage(matInfo.mat);

		if (index == 1)  // 相机1 -> 显示在 Window1
		{
			if (!_camera1Image)
				_camera1Image = new HalconCpp::HImage();
			*_camera1Image = hImg;
			//HalconCpp::WriteImage(hImg, "jpeg", 0, "C:/Users/zzw/Desktop/temp/222");

			// 更新显示
			setDisplayImage(DisplayWindow::Window1, hImg);
		}
		else if (index == 2)  // 相机2 -> 显示在 Window2
		{
			if (!_camera2Image)
				_camera2Image = new HalconCpp::HImage();
			*_camera2Image = hImg;

			// 更新显示
			setDisplayImage(DisplayWindow::Window2, hImg);
		}
	}
	catch (...)
	{
		qWarning() << "onCameraDisplay: failed to convert image";
	}
}

void Dlg_ImageStitching::btn_zengyi1_clicked()
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

void Dlg_ImageStitching::btn_baoguang1_clicked()
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

void Dlg_ImageStitching::btn_zengyi2_clicked()
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

void Dlg_ImageStitching::btn_baoguang2_clicked()
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

void Dlg_ImageStitching::btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		_stitchParam.DiffHeight = value.toDouble();
		ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->setText(value);
	}
}

void Dlg_ImageStitching::btn_caiqiebili_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		_stitchParam.OverlapInPercent = value.toDouble();
		ui->btn_caiqiebili->setText(value);
	}
}

void Dlg_ImageStitching::btn_buchang1_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		_stitchParam.DistancePlates = value.toDouble();
		ui->btn_buchang1->setText(value);
	}
}

void Dlg_ImageStitching::btn_buchang2_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		ui->btn_buchang2->setText(value);
	}
}

void Dlg_ImageStitching::btn_yongshi_clicked()
{
}

void Dlg_ImageStitching::btn_pinjieguocheng_clicked()
{
}

void Dlg_ImageStitching::btn_xinjianpinjieliaohao_clicked()
{
}

void Dlg_ImageStitching::btn_liaohaomingcheng_clicked()
{
}

void Dlg_ImageStitching::btn_xiugaimingcheng_clicked()
{
}

void Dlg_ImageStitching::btn_delete_clicked()
{
}

void Dlg_ImageStitching::btn_deleteAll_clicked()
{
}

void Dlg_ImageStitching::btn_dangqianxuanzedepinjieliaohao_clicked()
{
}

void Dlg_ImageStitching::btn_test_clicked()
{
	// 1. 准备两个相机的标定参数
	_cam1Calib = Modules::getInstance().configManagerModule.calibParam1;
	_cam2Calib = Modules::getInstance().configManagerModule.calibParam2;

	
	
	_stitchParam.caltabDescrPath = globalPath.describeBoardFile.toStdString();
	_stitchParam.DiffHeight = ui->btn_xiangjiwuliaohebiaodingbangaoduchazhi->text().toDouble()/1000;
	_stitchParam.OverlapInPercent = ui->btn_caiqiebili->text().toDouble();
	
	
	

	// 3. 计算映射图
	std::string errorMsg;
	if (_stitchParam.calibImage(_cam1Calib, _cam2Calib, &errorMsg))
	{
		// 显示拼接结果
		HalconCpp::HObject stitchedImage;
		if (_stitchParam.pinjieImage(*_camera1Image, *_camera2Image, stitchedImage))
		{
			// 显示拼接结果到主窗口
			HalconCpp::HImage stitchedHImage(stitchedImage);
			setDisplayImage(DisplayWindow::WindowMain, stitchedHImage);
		}
	}
	else
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString::fromStdString(errorMsg));
	}
}
