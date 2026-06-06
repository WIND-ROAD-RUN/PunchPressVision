#include "Dlg_changeshapemodel.h"
#include "ui_Dlg_changeshapemodel.h"
#if 0 // --- 以下项目引用暂时注释 ---
#include "Modules.hpp"
#include "ProcessModule.hpp"
#include "Utilty.hpp"
#include "rqwu/rqwu_MessageBox.h"
#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"
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

#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QPainter>

#include "ProcessParam.hpp"
#include "rqwu/rqwu_MessageBox.h"
#include "imginterop/imginterop_core.hpp"
#include "imgqt/imgqt_core.hpp"
#include "imghalcon/imghalcon_core.hpp"
#endif

Dlg_changeshapemodel::Dlg_changeshapemodel(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_changeshapemodelClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	build_ui();
	build_connect();
	isShowImg = true;
#endif
}

Dlg_changeshapemodel::~Dlg_changeshapemodel()
{
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---
// validateRois, halconCountObj, exportRectObjToRects,
// setRoiEditingUiEnabled, processParamToText, hImageToPixmapForDisplay,
// renderTextToPixmap, cvMatToPixmapForDisplay,
// build_ui, setRoiEditingUiEnabled, build_connect,
// onCameraDisplay, btn_exit_clicked, btn_readImage_clicked,
// btn_changeShapeModel_clicked, btn_paintRegion_clicked,
// btn_shiledRegion_clicked, btn_clearRegion_clicked,
// btn_zengyi1_clicked, btn_baoguang1_clicked,
// btn_zengyi2_clicked, btn_baoguang2_clicked, btn_angle_clicked,
// saveModelData, showEvent, closeEvent,
// readImage, paintCreateRegion, paintShieldRegion,
// createShapeModelFromRois, createShapeModel
#endif
