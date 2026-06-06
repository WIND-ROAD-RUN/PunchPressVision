#include "Dlg_createshapemodel.h"
#include "ui_Dlg_createshapemodel.h"
#if 0 // --- 以下项目引用暂时注释 ---
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
#endif

Dlg_createshapemodel::Dlg_createshapemodel(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_createshapemodelClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	build_ui();
	build_connect();
	isShowImg = true;
#endif
}

Dlg_createshapemodel::~Dlg_createshapemodel()
{
#if 0 // --- 以下内容暂时注释 ---
	closeHalconWindow();

	delete _cam1Buffer;
	delete _cam2Buffer;
	_cam1Buffer = nullptr;
	_cam2Buffer = nullptr;
#endif
}

#if 0 // --- 以下方法实现暂时注释 ---
// halconCountObj, exportRectObjToRects,
// build_ui, setRoiEditingUiEnabled, build_connect,
// ensureHalconWindow, closeHalconWindow, redrawHalconView,
// ensureHalconViewPart, resetHalconViewPartToFullImage,
// zoomHalconViewAt, panHalconViewFromDrag, eventFilter,
// onCameraDisplay, handleDualCameraStitch, processStitchedImage,
// handleSingleCamera, clearStitchBuffers,
// btn_exit_clicked, btn_readImage_clicked, btn_paintRegion_clicked,
// btn_shiledRegion_clicked, btn_createShapeModel_clicked,
// btn_clearRegion_clicked, btn_clearRegion2_clicked,
// btn_paintCenterPoint_clicked,
// rad_findshapemodel_toggled, rad_findshapemodelXLD_toggled,
// btn_MinLength_clicked, btn_min_clicked, btn_max_clicked,
// btn_createXLD_clicked,
// rbtn_auto_toggled, rbtn_manual_toggled,
// btn_contrast_clicked, btn_mincontrast_clicked,
// btn_zengyi1_clicked, btn_baoguang1_clicked,
// btn_zengyi2_clicked, btn_baoguang2_clicked, btn_angle_clicked,
// btn_opening_clicked, btn_closing_clicked, btn_mean_clicked,
// saveModelData, refreshTemplateImage,
// showEvent, closeEvent, resizeEvent,
// readImage, paintCreateRegion, paintShieldRegion,
// createShapeModelFromRois, createShapeModel
#endif
