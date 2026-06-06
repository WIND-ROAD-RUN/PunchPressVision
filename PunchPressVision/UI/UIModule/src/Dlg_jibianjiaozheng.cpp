#include "Dlg_jibianjiaozheng.h"
#include "ui_Dlg_jibianjiaozheng.h"
#if 0 // --- 以下项目引用暂时注释 ---
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include "Modules.hpp"
#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"
#include "Utilty.hpp"
#include "rqwu/rqwu_MessageBox.h"
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
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
#include <imgqt/imgqt_core.hpp>
#endif

Dlg_jibianjiaozheng::Dlg_jibianjiaozheng(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_jibianjiaozhengClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	CalibImages.clear();
	build_ui();
	build_connect();
#endif
}

Dlg_jibianjiaozheng::~Dlg_jibianjiaozheng()
{
#if 0 // --- 以下内容暂时注释 ---
	closeHalconWindow();
	clearCalibImages();
	delete nowImage;
	nowImage = nullptr;
#endif
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---
// clearLastMarksOverlay, updateUiFromCalibParam, initial_calibParamUi,
// onCameraDisplay, build_ui, ensureHalconWindow, closeHalconWindow,
// redrawHalconView, resizeEvent, build_connect,
// saveCalibParam, loadCalibParam, showEvent, closeEvent,
// btn_exit_clicked, btn_baoguang1_clicked, btn_zengyi1_clicked,
// btn_baoguang2_clicked, btn_zengyi2_clicked,
// btn_startCamera_clicked, btn_stopCamera_clicked,
// btn_capture_clicked, clearCalibImages, addNewItemToListView,
// btn_kaishibiaoding_clicked, btn_ceshi_clicked,
// btn_remove_clicked, btn_removeAll_clicked,
// btn_setAsReferencePose_clicked, btn_jiaoju_clicked,
// btn_shijiwuliaogaodudaobiaodingbanjuli_clicked,
// onCalibImageTableCellClicked,
// saveNowImageToCalibSlot, drawCalibMarks,
// calibrateFromImages, blendYellowRegion
#endif
