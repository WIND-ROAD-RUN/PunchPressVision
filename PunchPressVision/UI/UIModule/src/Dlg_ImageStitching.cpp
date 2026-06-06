#include "Dlg_ImageStitching.h"
#include "ui_Dlg_ImageStitching.h"
#if 0 // --- 以下项目引用暂时注释 ---
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
#endif

Dlg_ImageStitching::Dlg_ImageStitching(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_ImageStitchingClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	build_ui();
	build_connect();
#endif
}

Dlg_ImageStitching::~Dlg_ImageStitching()
{
#if 0 // --- 以下内容暂时注释 ---
	closeAllHalconWindows();
	delete _camera1Image;
	delete _camera2Image;
#endif
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---
// build_ui, build_connect, ensureHalconWindow, closeHalconWindow,
// closeAllHalconWindows, redrawHalconView, ensureHalconViewPart,
// resetHalconViewPartToFullImage, zoomHalconViewAt, panHalconViewFromDrag,
// setDisplayImage, eventFilter, showEvent, resizeEvent, closeEvent,
// btn_exit_clicked, btn_takePicture_clicked, onCameraDisplay,
// btn_zengyi1_clicked, btn_baoguang1_clicked, btn_zengyi2_clicked,
// btn_baoguang2_clicked, btn_xiangjiwuliaohebiaodingbangaoduchazhi_clicked,
// btn_caiqiebili_clicked, btn_buchang1_clicked, btn_buchang2_clicked,
// btn_yongshi_clicked, btn_pinjieguocheng_clicked,
// btn_xinjianpinjieliaohao_clicked, btn_liaohaomingcheng_clicked,
// btn_xiugaimingcheng_clicked, btn_delete_clicked, btn_deleteAll_clicked,
// btn_dangqianxuanzedepinjieliaohao_clicked, btn_test_clicked, getWindowData
#endif
