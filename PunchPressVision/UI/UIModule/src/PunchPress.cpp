#include "PunchPress.h"
#include "ui_PunchPress.h"
#if 0 // --- 以下项目引用暂时注释 ---
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
#endif

PunchPress::PunchPress(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::PunchPressClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	initializeComponents();
#endif
}

PunchPress::~PunchPress()
{
#if 0 // --- 以下内容暂时注释 ---
	destroyComponents();
	Modules::getInstance().stop();
	Modules::getInstance().destroy();
#endif
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---

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

	ini_clickableTitle();
	rbtn_removeFunc_checked(true);

	ui->ckb_jioucaiqie->setChecked(false);
}

void PunchPress::ini_clickableTitle()
{
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

// === 其余所有方法实现省略，均在 #if 0 内 ===
// ensureHalconWindow, closeHalconWindow, redrawHalconView,
// onImageProcessingFrameReady, onUpdateTabWidgetResult,
// ensureHalconViewPart, resetHalconViewPartToFullImage,
// zoomHalconViewAt, panHalconViewFromDrag,
// resizeEvent, eventFilter,
// build_DlgModelManager, destroy_DlgModelManager,
// updateCameraLabelState, onUpdateStatisticalInfoUI,
// onUpdateDlgModelManager, onDeleteModelModelManager,
// onLoadModelModelManager, onModelSelectChangeModelManager,
// onReNameModel, lb_title_clicked, pbtn_exit_clicked,
// rbtn_debug_checked, rbtn_removeFunc_checked,
// btn_saveProcessParam_clicked, btn_setOffSet_clicked,
// btn_save_clicked, btn_baoguang1_clicked, btn_zengyi1_clicked,
// btn_baoguang2_clicked, btn_zengyi2_clicked,
// btn_createModel_clicked, btn_changeModel_clicked,
// btn_loadModel_clicked, btn_jiudianbiaoding_clicked,
// btn_jibianjiaozheng_clicked, btn_imageStitching_clicked,
// ckb_jioucaiqie_checked, rbtn_upLight_clicked,
// rbtn_downLight_clicked, loadCompanyTXT

#endif
