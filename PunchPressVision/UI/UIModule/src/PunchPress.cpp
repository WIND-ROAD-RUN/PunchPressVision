#include "ui_PunchPress.h"

#include "PunchPress.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QEvent>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QDebug>

#include "DlgCloseForm.h"
#include "Modules.hpp"
#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"
#include "Utilty.hpp"
#include "rqw_LabelWarning.h"
#include "Dlg_jibianjiaozheng.h"
#include "Dlg_createshapemodel.h"
#include "Dlg_changeshapemodel.h"
#include "DlgJiudianbiaoding.h"
#include "Dlg_ImageStitching.h"
#include "Dlg_OffSet.h"
#include "rqwu/rqwu_MessageBox.h"
#include "func/ProcessModule.hpp"

#include <ProcessParam.hpp>
#include <qtconcurrentrun.h>
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

#include <QProgressDialog>
#include <imgqt/imgqt_core.hpp>

PunchPress::PunchPress(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::PunchPressClass())
{
	ui->setupUi(this);

	initializeComponents();
}

void PunchPress::showEvent(QShowEvent* e)
{
	QMainWindow::showEvent(e);

	QTimer::singleShot(0, this, [this]()
		{
			if (_halconHost)
				_imgDis1Size = _halconHost->size();
			if (!ensureHalconWindow())
				return;
			try
			{
				const qreal dpr = _halconHost ? _halconHost->devicePixelRatioF() : 1.0;
				HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0,
					(std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->width() : width()) * dpr))),
					(std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->height() : height()) * dpr))));
			}
			catch (...)
			{
			}

			redrawHalconView(true);
		});
}

PunchPress::~PunchPress()
{
	destroyComponents();
	Modules::getInstance().stop();
	Modules::getInstance().destroy();
	delete ui;
}

void PunchPress::build_ui()
{
	ui->rbtn_downLight->setAutoExclusive(false);
	ui->rbtn_upLight->setAutoExclusive(false);
	build_HalconDisplay();
	build_PunchPressData();
}

void PunchPress::build_HalconDisplay()
{
	auto* old = ui ? ui->label_imgDisplay_1 : nullptr;
	_imgDis1Size = old ? old->size() : QSize();
	if (!old)
		return;

	_halconHost = new QWidget(old->parentWidget());
	_halconHost->setObjectName(old->objectName());
	_halconHost->setStyleSheet(old->styleSheet());
	_halconHost->setSizePolicy(old->sizePolicy());
	_halconHost->setMinimumSize(old->minimumSize());
	_halconHost->setMaximumSize(old->maximumSize());
	_halconHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

void PunchPress::build_connect()
{
	connect(ui->pbtn_exit, &QPushButton::clicked, this, &PunchPress::pbtn_exit_clicked);
	connect(ui->rbtn_debug, &QRadioButton::toggled, this, &PunchPress::rbtn_debug_checked);
	connect(ui->rbtn_removeFunc, &QRadioButton::toggled, this, &PunchPress::rbtn_removeFunc_checked);
	connect(ui->btn_save, &QPushButton::clicked, this, &PunchPress::btn_save_clicked);

	connect(ui->btn_baoguang1, &QPushButton::clicked, this, &PunchPress::btn_baoguang1_clicked);
	connect(ui->btn_zengyi1, &QPushButton::clicked, this, &PunchPress::btn_zengyi1_clicked);
	connect(ui->btn_baoguang2, &QPushButton::clicked, this, &PunchPress::btn_baoguang2_clicked);
	connect(ui->btn_zengyi2, &QPushButton::clicked, this, &PunchPress::btn_zengyi2_clicked);

	connect(ui->btn_saveProcessParam, &QPushButton::clicked, this, &PunchPress::btn_saveProcessParam_clicked);
	connect(ui->btn_setOffSet, &QPushButton::clicked, this, &PunchPress::btn_setOffSet_clicked);

	connect(ui->btn_createModel, &QPushButton::clicked, this, &PunchPress::btn_createModel_clicked);
	connect(ui->btn_changeModel, &QPushButton::clicked, this, &PunchPress::btn_changeModel_clicked);
	connect(ui->btn_loadModel, &QPushButton::clicked, this, &PunchPress::btn_loadModel_clicked);
	connect(ui->btn_jibianjiaozheng, &QPushButton::clicked, this, &PunchPress::btn_jibianjiaozheng_clicked);
	connect(ui->btn_jiudianbiaoding, &QPushButton::clicked, this, &PunchPress::btn_jiudianbiaoding_clicked);
	connect(ui->btn_imageStitching, &QPushButton::clicked, this, &PunchPress::btn_imageStitching_clicked);

	connect(ui->rbtn_upLight, &QRadioButton::clicked, this, &PunchPress::rbtn_upLight_clicked);
	connect(ui->rbtn_downLight, &QRadioButton::clicked, this, &PunchPress::rbtn_downLight_clicked);

	QObject::connect(ui->ckb_jioucaiqie, &QCheckBox::clicked, this, &PunchPress::ckb_jioucaiqie_checked);


	// 连接显示标题
	QObject::connect(clickableTitle, &rw::rqw::ClickableLabel::clicked, this, &PunchPress::lb_title_clicked);
}

void PunchPress::build_PunchPressData()
{
	auto& cfg = Modules::getInstance().configManagerModule.runningInfo;

	ui->rbtn_debug->setChecked(false);
	ui->rbtn_removeFunc->setChecked(true);
	ui->rbtn_upLight->setChecked(cfg.lightSet.upLight);
	ui->rbtn_downLight->setChecked(cfg.lightSet.downLight);
	ui->tabWidget->setCurrentIndex(0);
	ui->btn_baoguang1->setText(QString::number(cfg.cameraSet.exposureTime1));
	ui->btn_zengyi1->setText(QString::number(cfg.cameraSet.gain1));
	ui->btn_baoguang2->setText(QString::number(cfg.cameraSet.exposureTime2));
	ui->btn_zengyi2->setText(QString::number(cfg.cameraSet.gain2));

	_picturesViewer = new PictureViewerThumbnails(this);

	ini_clickableTitle();
	rbtn_removeFunc_checked(true);

	ui->ckb_jioucaiqie->setChecked(false);
}

void PunchPress::ini_clickableTitle()
{
	// 初始化标题label
	clickableTitle = new rw::rqw::ClickableLabel(this);
	auto layoutTitle = ui->groupBox_head->layout();
	layoutTitle->replaceWidget(ui->label_title, clickableTitle);
	delete ui->label_title;
	clickableTitle->setText("冲床检测");
	clickableTitle->setStyleSheet("QLabel {font-size: 30px;font-weight: bold;color: rgb(255, 255, 255);padding: 5px 5px;border-bottom: 2px solid #cccccc;}");
}

void PunchPress::initializeComponents()
{
	build_ui();

	build_DlgModelManager();

	getCameraStateAndUpdateUi();

	auto isConnect = Modules::getInstance().plcModule.modbusDevice->isConnected();
	updateCameraLabelState(0, isConnect);

	loadCompanyTXT();

	build_connect();
}

void PunchPress::destroyComponents()
{
	closeHalconWindow();

	destroy_DlgModelManager();
}

void PunchPress::getCameraStateAndUpdateUi()
{
	auto& cameraModules = Modules::getInstance().cameraModule;
	auto errors = cameraModules.getBuildResults();
	updateCameraLabelState(1, true);
	updateCameraLabelState(2, true);

	for (const auto& error : errors)
	{
		auto index = static_cast<int>(error);
		updateCameraLabelState(index, false);
	}
}

bool PunchPress::ensureHalconWindow()
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

	QSize hostSize = _halconHost->size();
	if (hostSize.isEmpty())
		hostSize = _imgDis1Size;
	const qreal dpr = _halconHost->devicePixelRatioF();
	const int hostW = (std::max)(1, static_cast<int>(std::lround(hostSize.width() * dpr)));
	const int hostH = (std::max)(1, static_cast<int>(std::lround(hostSize.height() * dpr)));

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

void PunchPress::closeHalconWindow()
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

	delete _halconLastXld;
	_halconLastXld = nullptr;

	_viewPartValid = false;
	_viewImgW = 0;
	_viewImgH = 0;
	_panCandidate = false;
	_isPanning = false;
}

void PunchPress::redrawHalconView(bool clearWindow)
{
	if (!ensureHalconWindow())
		return;
	if (!_halconLastImage)
		return;
	if (!ensureHalconViewPart())
		return;

	const qreal dpr = _halconHost ? _halconHost->devicePixelRatioF() : 1.0;
	const int winW = (std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->width() : width()) * dpr)));
	const int winH = (std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->height() : height()) * dpr)));

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

		// 为了“自适应窗口且不拉伸”，根据窗口宽高比对 Part 做等比扩展（letterbox/pillarbox）
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
		if (_halconLastXld)
		{
			HTuple n;
			CountObj(*_halconLastXld, &n);
			if (n.I() > 0)
			{
				HalconCpp::SetColor(*_halconWindowHandle, "green");
				HalconCpp::SetLineWidth(*_halconWindowHandle, 2);
				DispObj(*_halconLastXld, *_halconWindowHandle);
			}
		}
	}
	catch (...)
	{
		return;
	}
}

void PunchPress::onImageProcessingFrameReady()
{
	QElapsedTimer totalTimer;
	totalTimer.start();

	if (!ensureHalconWindow())
		return;

	// 如果上一帧还没画完，直接跳过，避免主线程堆积
	if (_isDrawing.exchange(true))
		return;

	auto* imgPro = Modules::getInstance().imgProModule._imgProModule;
	if (!imgPro)
	{
		_isDrawing = false;
		return;
	}

	HalconCpp::HImage img;
	HalconCpp::HObject xld;

	QElapsedTimer t;
	t.start();
	if (!imgPro->tryGetLastResult(img, xld))
	{
		_isDrawing = false;
		return;
	}
	qDebug() << "[onImageProcessingFrameReady] tryGetLastResult took" << t.elapsed() << "ms";

	try
	{
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(img);
		else
			*_halconLastImage = img;

		if (!_halconLastXld)
			_halconLastXld = new HalconCpp::HObject(xld);
		else
			*_halconLastXld = xld;

		t.restart();
		redrawHalconView(true);
		qDebug() << "[onImageProcessingFrameReady] redrawHalconView took" << t.elapsed() << "ms";
	}
	catch (...)
	{
	}

	_isDrawing = false;
	qDebug() << "[onImageProcessingFrameReady] total took" << totalTimer.elapsed() << "ms";
}

void PunchPress::onUpdateTabWidgetResult(const float centerX, const float centerY, const float angle)
{
	if (!ui || !ui->tabWidget_info)
		return;

	auto* table = ui->tabWidget_info;

	// 3列：中心点X / 中心点Y / 角度
	if (table->columnCount() < 3)
		table->setColumnCount(3);

	// 表头只初始化一次（避免每次 new/覆盖）
	if (!table->horizontalHeaderItem(0))
		table->setHorizontalHeaderItem(0, new QTableWidgetItem(tr("中心点X")));
	if (!table->horizontalHeaderItem(1))
		table->setHorizontalHeaderItem(1, new QTableWidgetItem(tr("中心点Y")));
	if (!table->horizontalHeaderItem(2))
		table->setHorizontalHeaderItem(2, new QTableWidgetItem(tr("角度")));

	auto makeItem = [](const QString& text)
		{
			auto* item = new QTableWidgetItem(text);
			item->setTextAlignment(Qt::AlignCenter);
			item->setFlags(item->flags() & ~Qt::ItemIsEditable);
			return item;
		};

	// 追加一行
	const int row = table->rowCount();
	table->insertRow(row);

	table->setItem(row, 0, makeItem(QString::number(static_cast<double>(centerX), 'f', 3)));
	table->setItem(row, 1, makeItem(QString::number(static_cast<double>(centerY), 'f', 3)));
	table->setItem(row, 2, makeItem(QString::number(static_cast<double>(angle), 'f', 6)));

	// 可选：滚动到最新一行
	table->scrollToBottom();
}

bool PunchPress::ensureHalconViewPart()
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

void PunchPress::resetHalconViewPartToFullImage()
{
	_viewPart.r1 = 0.0;
	_viewPart.c1 = 0.0;
	_viewPart.r2 = (std::max)(0, _viewImgH - 1);
	_viewPart.c2 = (std::max)(0, _viewImgW - 1);
	_viewPartValid = true;
}

void PunchPress::zoomHalconViewAt(const QPoint& hostPos, int steps)
{
	if (steps == 0)
		return;
	if (!ensureHalconViewPart())
		return;

	const int hostW = (std::max)(1, _halconHost ? _halconHost->width() : 1);
	const int hostH = (std::max)(1, _halconHost ? _halconHost->height() : 1);

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

	const double fullSpanC = (std::max)(0, _viewImgW - 1);
	const double fullSpanR = (std::max)(0, _viewImgH - 1);

	// 限制“缩小(zoom out)”最多到原图全幅，不能无限缩小
	if (newSpanC >= fullSpanC || newSpanR >= fullSpanR)
	{
		resetHalconViewPartToFullImage();
		return;
	}

	_viewPart.c1 = col - rx * newSpanC;
	_viewPart.r1 = row - ry * newSpanR;

	// 限制 Part 不越界
	const double maxC1 = fullSpanC - newSpanC;
	const double maxR1 = fullSpanR - newSpanR;
	_viewPart.c1 = std::clamp(_viewPart.c1, 0.0, (std::max)(0.0, maxC1));
	_viewPart.r1 = std::clamp(_viewPart.r1, 0.0, (std::max)(0.0, maxR1));

	_viewPart.c2 = _viewPart.c1 + newSpanC;
	_viewPart.r2 = _viewPart.r1 + newSpanR;
	_viewPartValid = true;
}

void PunchPress::panHalconViewFromDrag(const QPoint& dragDelta)
{
	if (!ensureHalconViewPart())
		return;

	const int hostW = (std::max)(1, _halconHost ? _halconHost->width() : 1);
	const int hostH = (std::max)(1, _halconHost ? _halconHost->height() : 1);

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

	const double fullSpanC = (std::max)(0, _viewImgW - 1);
	const double fullSpanR = (std::max)(0, _viewImgH - 1);
	const double curSpanC = _viewPart.c2 - _viewPart.c1;
	const double curSpanR = _viewPart.r2 - _viewPart.r1;

	if (curSpanC > 1e-9 && curSpanR > 1e-9)
	{
		const double maxC1 = fullSpanC - curSpanC;
		const double maxR1 = fullSpanR - curSpanR;
		_viewPart.c1 = std::clamp(_viewPart.c1, 0.0, (std::max)(0.0, maxC1));
		_viewPart.r1 = std::clamp(_viewPart.r1, 0.0, (std::max)(0.0, maxR1));
		_viewPart.c2 = _viewPart.c1 + curSpanC;
		_viewPart.r2 = _viewPart.r1 + curSpanR;
	}

	_viewPartValid = true;
}

void PunchPress::resizeEvent(QResizeEvent* e)
{
	QMainWindow::resizeEvent(e);

	if (!ensureHalconWindow())
		return;

	try
	{
		const qreal dpr = _halconHost ? _halconHost->devicePixelRatioF() : 1.0;
		HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0,
			(std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->width() : width()) * dpr))),
			(std::max)(1, static_cast<int>(std::lround((_halconHost ? _halconHost->height() : height()) * dpr))));
		redrawHalconView(false);
	}
	catch (...)
	{
	}
}

bool PunchPress::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == _halconHost)
	{
		switch (event->type())
		{
		case QEvent::Wheel:
		{
			auto* e = static_cast<QWheelEvent*>(event);
			const int delta = e->angleDelta().y();
			if (delta == 0)
				return true;
			if (!_halconLastImage)
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
				// 不抢占 click：先作为候选，只有拖动超过阈值才进入 panning
				_panCandidate = true;
				_isPanning = false;
				_panStartPos = e->pos();
				_panStartPart = _viewPart;
				return false;
			}
			break;
		}
		case QEvent::MouseMove:
		{
			auto* e = static_cast<QMouseEvent*>(event);
			if (_panCandidate && (e->buttons() & Qt::LeftButton) && _halconLastImage)
			{
				const QPoint delta = e->pos() - _panStartPos;
				if (!_isPanning)
				{
					if (delta.manhattanLength() < 3)
						return false;
					_isPanning = true;
				}

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
				const bool wasPanning = _isPanning;
				_panCandidate = false;
				_isPanning = false;
				// 如果发生过拖拽，吞掉 release，避免触发 clicked
				return wasPanning;
			}
			break;
		}
		default:
			break;
		}
	}

	return QMainWindow::eventFilter(watched, event);
}

void PunchPress::build_DlgModelManager()
{
	rw::rqwu::DlgModelManagerConfig cfg;

	cfg.vec_classid_cfg[0] = rw::rqwu::ClassIDCfgItem{ "原始图片" };
	cfg.vec_classid_cfg[1] = rw::rqwu::ClassIDCfgItem{ "预处理图片" };

	_dlgModelManager = new rw::rqwu::DlgModelManager(cfg);

	onUpdateDlgModelManager();

	// connect
	connect(_dlgModelManager, &rw::rqwu::DlgModelManager::deleteModel,
		this, &PunchPress::onDeleteModelModelManager);
	connect(_dlgModelManager, &rw::rqwu::DlgModelManager::loadModel,
		this, &PunchPress::onLoadModelModelManager);
	connect(_dlgModelManager, &rw::rqwu::DlgModelManager::modelSelectChange,
		this, &PunchPress::onModelSelectChangeModelManager);
	connect(_dlgModelManager, &rw::rqwu::DlgModelManager::reNameModel,
		this, &PunchPress::onReNameModel);
}

void PunchPress::destroy_DlgModelManager()
{
	if (_dlgModelManager)
	{
		delete _dlgModelManager;
	}
}

void PunchPress::updateCameraLabelState(int cameraIndex, bool state)
{
	switch (cameraIndex)
	{
	case 0:
		if (state) {
			ui->label_PlcState->setText("连接成功");
			ui->label_PlcState->setStyleSheet(QString("QLabel{color:rgb(0, 230, 0);} "));
		}
		else {
			ui->label_PlcState->setText("连接失败");
			ui->label_PlcState->setStyleSheet(QString("QLabel{color:rgb(230, 0, 0);} "));
		}
		break;
	case 1:
		if (state) {
			ui->label_camera1State->setText("连接成功");
			ui->label_camera1State->setStyleSheet(QString("QLabel{color:rgb(0, 230, 0);} "));
		}
		else {
			ui->label_camera1State->setText("连接失败");
			ui->label_camera1State->setStyleSheet(QString("QLabel{color:rgb(230, 0, 0);} "));
		}
		break;
	case 2:
		if (state) {
			ui->label_camera2State->setText("连接成功");
			ui->label_camera2State->setStyleSheet(QString("QLabel{color:rgb(0, 230, 0);} "));
		}
		else {
			ui->label_camera2State->setText("连接失败");
			ui->label_camera2State->setStyleSheet(QString("QLabel{color:rgb(230, 0, 0);} "));
		}
		break;
	default:
		break;
	}
}

void PunchPress::onUpdateStatisticalInfoUI()
{

}

void PunchPress::onUpdateDlgModelManager()
{
	QVector<rw::rqwu::ModelInfo> model_infos;

	auto& cfgModule = Modules::getInstance().configManagerModule;
	auto runningInfos = cfgModule.loadAllRunningInfo();
	const std::string curModelDir = cfgModule.punchPressInfo.lastLoadModelPath;

	for (int i = runningInfos.size() - 1; i >= 0; i--)
	{
		const auto& runningInfo = runningInfos[i];

		rw::rqwu::ModelInfo mi;

		mi.name = QString::fromStdString(runningInfo.baseInfo.name);

		const QString parentDir = QFileInfo(QString::fromStdString(runningInfo.filePath.modelPath)).dir().absolutePath();
		const std::string parentDirStd = parentDir.toStdString();
		const bool isCurrentModel = cfgModule.isLoaderSuccess && !curModelDir.empty() && (parentDirStd == curModelDir);

		const auto& showRunningInfo = isCurrentModel ? cfgModule.runningInfo : runningInfo;

		// cameraSet
		mi.model_param_list["增益1"] = showRunningInfo.cameraSet.gain1;
		mi.model_param_list["曝光1"] = showRunningInfo.cameraSet.exposureTime1;
		mi.model_param_list["像素当量1"] = showRunningInfo.cameraSet.pixelEquivalent1;
		mi.model_param_list["增益2"] = showRunningInfo.cameraSet.gain2;
		mi.model_param_list["曝光2"] = showRunningInfo.cameraSet.exposureTime2;
		mi.model_param_list["像素当量2"] = showRunningInfo.cameraSet.pixelEquivalent2;

		// lightSet
		mi.model_param_list["上光源"] = showRunningInfo.lightSet.upLight;
		mi.model_param_list["下光源"] = showRunningInfo.lightSet.downLight;

		// modelInfo
		mi.model_param_list["预处理类型"] = showRunningInfo.modelInfo.preProcessType;
		mi.model_param_list["预处理角度"] = showRunningInfo.modelInfo.angle;

		// baseInfo
		mi.model_param_list["模型训练时间"] = QString::fromStdString(runningInfo.baseInfo.trainDate);
		mi.model_param_list["模型名称"] = QString::fromStdString(runningInfo.baseInfo.name);

		// filePath
		mi.model_param_list["原始图片路径"] = QString::fromStdString(runningInfo.filePath.trainsouceImgPath);
		mi.model_param_list["预处理图片路径"] = QString::fromStdString(runningInfo.filePath.prePareImgPath);
		mi.model_param_list["模型路径"] = QString::fromStdString(runningInfo.filePath.modelPath);

		// offSet
		if (isCurrentModel)
		{
			// 用于判断模型的name是否相等
			auto isModelNameMatch = [](const QString& modelPath, const QString& name)
				{
					const QString baseName = QFileInfo(QDir::fromNativeSeparators(modelPath)).fileName();
					return baseName == name;
				};

			if (isModelNameMatch(cfgModule.processParams[0].modelPath,
				QString::fromStdString(runningInfo.baseInfo.name)))
			{
				mi.model_param_list["左右"] = cfgModule.processParams[0].offsetX;
				mi.model_param_list["前后"] = cfgModule.processParams[0].offsetY;
				mi.model_param_list["角度"] = cfgModule.processParams[0].offsetAngle;
				mi.model_param_list["查找数目"] = cfgModule.processParams[0].findnumber;
				mi.model_param_list["是否启用"] = cfgModule.processParams[0].enabled;
				mi.model_param_list["旋转角度"] = cfgModule.processParams[0].rotateAngle;
			}
			else if (isModelNameMatch(cfgModule.processParams[1].modelPath,
				QString::fromStdString(runningInfo.baseInfo.name)))
			{
				mi.model_param_list["左右"] = cfgModule.processParams[1].offsetX;
				mi.model_param_list["前后"] = cfgModule.processParams[1].offsetY;
				mi.model_param_list["角度"] = cfgModule.processParams[1].offsetAngle;
				mi.model_param_list["查找数目"] = cfgModule.processParams[1].findnumber;
				mi.model_param_list["是否启用"] = cfgModule.processParams[1].enabled;
				mi.model_param_list["旋转角度"] = cfgModule.processParams[1].rotateAngle;
			}
			else
			{
				mi.model_param_list["左右"] = cfgModule.processParam1.offsetX;
				mi.model_param_list["前后"] = cfgModule.processParam1.offsetY;
				mi.model_param_list["角度"] = cfgModule.processParam1.offsetAngle;
				mi.model_param_list["查找数目"] = cfgModule.processParam1.findnumber;
				mi.model_param_list["是否启用"] = cfgModule.processParam1.enabled;
				mi.model_param_list["旋转角度"] = cfgModule.processParam1.rotateAngle;
			}
		}
		else
		{
			ProcessParam pp;
			if (pp.loadMetaOnly(parentDirStd))
			{
				mi.model_param_list["左右"] = pp.offsetX;
				mi.model_param_list["前后"] = pp.offsetY;
				mi.model_param_list["角度"] = pp.offsetAngle;
				mi.model_param_list["查找数目"] = pp.findnumber;
				mi.model_param_list["是否启用"] = pp.enabled;
				mi.model_param_list["旋转角度"] = pp.rotateAngle;
			}
			else
			{
				mi.model_param_list["左右"] = 0.0;
				mi.model_param_list["前后"] = 0.0;
				mi.model_param_list["角度"] = 0.0;
				mi.model_param_list["查找数目"] = 0;
				mi.model_param_list["是否启用"] = false;
				mi.model_param_list["旋转角度"] = 0.0;
			}
		}

		model_infos.append(mi);
	}

	_dlgModelManager->setModelInfos(model_infos);
}

void PunchPress::onDeleteModelModelManager(rw::rqwu::ModelInfo info)
{
	auto modelPath = info.model_param_list["模型路径"].toString().toStdString();

	const QString parentDir = QFileInfo(QString::fromStdString(modelPath)).dir().absolutePath();

	Modules::getInstance().configManagerModule.eraseModel(parentDir.toStdString());
}

void PunchPress::onLoadModelModelManager(rw::rqwu::ModelInfo info)
{
	QMessageBox selectBox(this);
	selectBox.setWindowTitle("提示");
	selectBox.setText("请确认模型加载到何处");
	auto* btnAModel = selectBox.addButton("A模型", QMessageBox::AcceptRole);
	auto* btnBModel = selectBox.addButton("B模型", QMessageBox::AcceptRole);
	selectBox.exec();

	const auto* clickedBtn = selectBox.clickedButton();
	if (!clickedBtn)
	{
		return;
	}

	auto modelPath = info.model_param_list["模型路径"].toString().toStdString();

	const QString parentDir = QFileInfo(QString::fromStdString(modelPath)).dir().absolutePath();

	auto& configManagerModule = Modules::getInstance().configManagerModule;
	if (!configManagerModule.loadModel(parentDir.toStdString()))
	{
		rw::rqwu::MessageBox::warning(nullptr, "警告", "加载模型失败");
		return;
	}

	auto& processParam1 = Modules::getInstance().configManagerModule.processParam1;
	auto& processParams = Modules::getInstance().configManagerModule.processParams;

	processParam1.load(parentDir.toStdString());
	processParam1.modelPath = parentDir;

	auto& runningInfo = Modules::getInstance().configManagerModule.runningInfo;

	// 确保 processParams 有足够的大小
	if (processParams.size() < 2)
	{
		processParams.resize(2);
	}

	if (clickedBtn == btnAModel)
	{
		qDebug() << "A";
		auto tempProcessParam = processParam1;
		tempProcessParam.enabled = true;
        tempProcessParam.modelPath = parentDir;
		processParams[0] = tempProcessParam;
		ui->lb_showModelIdA->setText(QString::fromStdString(runningInfo.baseInfo.name));
	}
	else if (clickedBtn == btnBModel)
	{
		qDebug() << "B";
		auto tempProcessParam = processParam1;
		tempProcessParam.enabled = true;
        tempProcessParam.modelPath = parentDir;
		processParams[1] = tempProcessParam;
		ui->lb_showModelIdB->setText(QString::fromStdString(runningInfo.baseInfo.name));
	}
	else
	{
		return;
	}
	

	// 更新UI
	ui->rbtn_upLight->setChecked(runningInfo.lightSet.upLight);
	ui->rbtn_downLight->setChecked(runningInfo.lightSet.downLight);
	ui->btn_baoguang1->setText(QString::number(runningInfo.cameraSet.exposureTime1));
	ui->btn_zengyi1->setText(QString::number(runningInfo.cameraSet.gain1));
	ui->btn_baoguang2->setText(QString::number(runningInfo.cameraSet.exposureTime2));
	ui->btn_zengyi2->setText(QString::number(runningInfo.cameraSet.gain2));

	//更新PLC
	PLCControl::setDownLight(runningInfo.lightSet.downLight);
	PLCControl::setUpLight(runningInfo.lightSet.upLight);

	//更新相机
	Modules::getInstance().cameraModule.setCamera1ExposureTime(runningInfo.cameraSet.exposureTime1);
	Modules::getInstance().cameraModule.setCamera1Gain(runningInfo.cameraSet.gain1);
	Modules::getInstance().cameraModule.setCamera2ExposureTime(runningInfo.cameraSet.exposureTime2);
	Modules::getInstance().cameraModule.setCamera2Gain(runningInfo.cameraSet.gain2);

	_dlgModelManager->close();
	rw::rqwu::MessageBox::information(this, "提示", "模型加载成功!");

	Modules::getInstance().configManagerModule.isLoaderSuccess = true;
}

void PunchPress::onModelSelectChangeModelManager(rw::rqwu::ModelInfo info, QMap<rw::rqwu::ClassID, QLabel*> classIdToLabel)
{
	if (_dlgModelManager)
	{
		auto sourceImgPath = info.model_param_list["原始图片路径"].toString().toStdString();
		auto prePareImgPath = info.model_param_list["预处理图片路径"].toString().toStdString();
		QPixmap sourcePix;
		if (QFileInfo::exists(QString::fromStdString(sourceImgPath)))
		{
			sourcePix.load(QString::fromStdString(sourceImgPath));
		}
		else
		{
			classIdToLabel[0]->setText("未找到原始图片!");
		}
		QPixmap preParePix;
		if (QFileInfo::exists(QString::fromStdString(prePareImgPath)))
		{
			preParePix.load(QString::fromStdString(prePareImgPath));
		}
		else
		{
			classIdToLabel[1]->setText("未找到预处理图片!");
		}

		classIdToLabel[0]->setPixmap(sourcePix.scaled(classIdToLabel[0]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		classIdToLabel[1]->setPixmap(preParePix.scaled(classIdToLabel[1]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

	}
}

void PunchPress::onReNameModel(rw::rqwu::ModelInfo info, QString newName)
{
	auto modelPath = info.model_param_list["模型路径"].toString().toStdString();
	const QString parentDir = QFileInfo(QString::fromStdString(modelPath)).dir().absolutePath();
	Modules::getInstance().configManagerModule.reNameModel(parentDir.toStdString(), newName.toStdString());
}

void PunchPress::lb_title_clicked()
{
	if (0 != minimizeCount)
	{
		minimizeCount--;
	}
	else if (0 >= minimizeCount)
	{
		// 最小化主窗体
		this->showMinimized();

		// 最小化所有子窗体（如果已创建且可见）
		if (_picturesViewer && _picturesViewer->isVisible())
			_picturesViewer->showMinimized();

		minimizeCount = 3; // 重置最小化计数器
	}
}

void PunchPress::pbtn_exit_clicked()
{
#ifdef NDEBUG
	auto& _dlgCloseForm = Modules::getInstance().uiModule._dlgCloseForm;
	if (_dlgCloseForm)
	{
		_dlgCloseForm->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
		_dlgCloseForm->exec();
	}
#else
	this->close();
#endif
}

void PunchPress::rbtn_debug_checked(bool checked)
{
	if (!Modules::getInstance().configManagerModule.isLoadCalibParam1)
	{
		if (!_warnedNoCalibParam)
		{
			_warnedNoCalibParam = true;
			rw::rqwu::MessageBox::warning(this, "提示", "未加载畸变标定参数，将直接显示原始图像。");
		}
	}

	auto& runningState = Modules::getInstance().runtimeInfoModule.runningState;
	ui->rbtn_debug->setChecked(checked);
	ui->rbtn_removeFunc->setChecked(!checked);
	if (checked)
	{
		runningState = RunningState::Debug;
		Modules::getInstance().cameraModule.setCamera1TriggerOff();
		Modules::getInstance().cameraModule.setCamera2TriggerOff();

	}
	else
	{
		runningState = RunningState::Stop;
	}
}

void PunchPress::rbtn_removeFunc_checked(bool checked)
{
	if (!Modules::getInstance().configManagerModule.isLoadCalibParam1)
	{
		if (!_warnedNoCalibParam)
		{
			_warnedNoCalibParam = true;
			rw::rqwu::MessageBox::warning(this, "提示", "未加载畸变标定参数，将直接显示原始图像。");
		}
	}

	auto& runningState = Modules::getInstance().runtimeInfoModule.runningState;
	auto& camera1 = Modules::getInstance().cameraModule.camera1;
	auto& camera2 = Modules::getInstance().cameraModule.camera2;
	ui->rbtn_removeFunc->setChecked(checked);
	ui->rbtn_debug->setChecked(!checked);
	if (checked)
	{
		runningState = RunningState::OpenRemoveFunc;
		if (camera1)
		{
			Modules::getInstance().cameraModule.setCamera1HardwareTrigger();
			camera1->setFrameRate(50);
		}
		if (camera2)
		{
			Modules::getInstance().cameraModule.setCamera2HardwareTrigger();
			camera2->setFrameRate(50);
		}
	}
	else
	{
		runningState = RunningState::Stop;
	}
}

void PunchPress::btn_saveProcessParam_clicked()
{
	auto& cfg = Modules::getInstance().configManagerModule;
	
	// 确保 processParams 有足够的大小
	if (cfg.processParams.size() < 2)
	{
		cfg.processParams.resize(2);
	}
	
	if (!cfg.processParams[0].modelPath.isEmpty())
	{
		cfg.processParams[0].save(cfg.processParams[0].modelPath.toStdString());
	}
	if (!cfg.processParams[1].modelPath.isEmpty())
	{
		cfg.processParams[1].save(cfg.processParams[1].modelPath.toStdString());
	}
	
	Modules::getInstance().configManagerModule.isModelDirty = true;

	rw::rqwu::MessageBox::information(this, "成功", "修改成功");
}

void PunchPress::btn_setOffSet_clicked()
{
	auto& dlg_OffSet = Modules::getInstance().uiModule._dlgOffSet;

	dlg_OffSet->setFixedSize(800,600);
	dlg_OffSet->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	dlg_OffSet->exec();
}

void PunchPress::btn_save_clicked()
{
	auto& cm = Modules::getInstance().configManagerModule;
	if (!cm.isLoaderSuccess || cm.punchPressInfo.lastLoadModelPath.empty())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "当前没有加载任何模型，无法保存。");
		return;
	}

	try
	{
		// 保存当前模型参数（offsetX/offsetY/offsetAngle 等都在 processParam1 内）
		cm.processParam1.save(cm.punchPressInfo.lastLoadModelPath);
		cm.dataSave.save(cm.punchPressInfo.lastLoadModelPath, cm.runningInfo);
		cm.isModelDirty = true;
		rw::rqwu::MessageBox::information(this, "提示", "保存成功");
	}
	catch (...)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "保存失败");
	}
}

void PunchPress::btn_baoguang1_clicked()
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
		Modules::getInstance().cameraModule.setCamera1ExposureTime(value.toInt());
		auto& cameraConfig = Modules::getInstance().configManagerModule.runningInfo.cameraSet;
		cameraConfig.exposureTime1 = value.toInt();
	}
}

void PunchPress::btn_zengyi1_clicked()
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
		Modules::getInstance().cameraModule.setCamera1Gain(value.toInt());
		auto& cameraConfig = Modules::getInstance().configManagerModule.runningInfo.cameraSet;
		cameraConfig.gain1 = value.toInt();
	}
}

void PunchPress::btn_baoguang2_clicked()
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
		Modules::getInstance().cameraModule.setCamera2ExposureTime(value.toInt());
		auto& cameraConfig = Modules::getInstance().configManagerModule.runningInfo.cameraSet;
		cameraConfig.exposureTime2 = value.toInt();
	}
}

void PunchPress::btn_zengyi2_clicked()
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
		Modules::getInstance().cameraModule.setCamera2Gain(value.toInt());
		auto& cameraConfig = Modules::getInstance().configManagerModule.runningInfo.cameraSet;
		cameraConfig.gain2 = value.toInt();
	}
}


void PunchPress::btn_createModel_clicked()
{
	auto& dlg_createshapemodel = Modules::getInstance().uiModule._dlgCreateShapeModel;

	dlg_createshapemodel->setFixedSize(this->width(), this->height());

	dlg_createshapemodel->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	dlg_createshapemodel->exec();

}

void PunchPress::btn_changeModel_clicked()
{
	if (!Modules::getInstance().configManagerModule.isLoaderSuccess)
	{
		rw::rqwu::MessageBox::warning(this, "错误", "你还没有加载任何模型");
		return;
	}

	auto& dlgChangeShapeModel = Modules::getInstance().uiModule._dlgChangeShapeModel;

	dlgChangeShapeModel->setFixedSize(this->width(), this->height());

	dlgChangeShapeModel->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	dlgChangeShapeModel->exec();
}

void PunchPress::btn_loadModel_clicked()
{
	if (_dlgModelManager)
	{
		QProgressDialog loading(tr("正在加载..."), QString(), 0, 0, this);
		loading.setWindowModality(Qt::WindowModal);
		loading.setCancelButton(nullptr);
		loading.setMinimumDuration(0);
		loading.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
		loading.show();
		QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

		auto& cm = Modules::getInstance().configManagerModule;
		auto& isModelChanged = cm.isAddModel;
		if (isModelChanged || cm.isModelDirty)
		{
			onUpdateDlgModelManager();
			cm.isAddModel = false;
		}

		loading.close();
		QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

		_dlgModelManager->setFixedSize(this->width(), this->height());
		_dlgModelManager->exec();
	}
}

void PunchPress::btn_jiudianbiaoding_clicked()
{
	auto& dlgJiuDianBiaoDing = Modules::getInstance().uiModule._dlgJiudianbiaoding;
	//dlgJiuDianBiaoDing->setFixedSize(this->width(), this->height());
	//dlgJiuDianBiaoDing->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	dlgJiuDianBiaoDing->exec();
}

void PunchPress::btn_jibianjiaozheng_clicked()
{
	auto& dlg_jibianjiaozheng = Modules::getInstance().uiModule._dlgJiBianJiaoZheng;
	//dlg_jibianjiaozheng->setFixedSize(this->width(), this->height());
	//dlg_jibianjiaozheng->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	dlg_jibianjiaozheng->exec();
}

void PunchPress::btn_imageStitching_clicked()
{
	auto& dlg_imageStitching = Modules::getInstance().uiModule._dlgImageStitching;
	dlg_imageStitching->exec();
}

void PunchPress::ckb_jioucaiqie_checked(bool checked)
{
	auto& config = Modules::getInstance().configManagerModule.punchPressInfo;
	config.jioucaiqie = checked;
}

void PunchPress::rbtn_upLight_clicked()
{
	auto& runningInfo = Modules::getInstance().configManagerModule.runningInfo;
	runningInfo.lightSet.upLight = ui->rbtn_upLight->isChecked();
	PLCControl::setUpLight(ui->rbtn_upLight->isChecked());
}

void PunchPress::rbtn_downLight_clicked()
{
	auto& runningInfo = Modules::getInstance().configManagerModule.runningInfo;
	runningInfo.lightSet.downLight = ui->rbtn_downLight->isChecked();
	PLCControl::setDownLight(ui->rbtn_downLight->isChecked());
}


void PunchPress::loadCompanyTXT()
{
	const QString companyRootPath = globalPath.companyTxtPath;
	QFile file(companyRootPath);
	if (!file.exists())
	{
		ui->label_companyInfo->setText(QString("company.txt不存在：%1").arg(companyRootPath));
		return;
	}

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		ui->label_companyInfo->setText(QString("company.txt打开失败：%1").arg(companyRootPath));
		return;
	}

	QTextStream in(&file);
	in.setEncoding(QStringConverter::Utf8);

	const QString content = in.readAll().trimmed();
	ui->label_companyInfo->setText(content);

	file.close();
}