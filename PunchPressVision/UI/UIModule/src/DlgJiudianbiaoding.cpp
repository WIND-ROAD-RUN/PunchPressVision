#include "DlgJiudianbiaoding.h"
#include "ui_DlgJiudianbiaoding.h"
#if 0 // --- 以下项目引用暂时注释 ---
#include <QFileDialog>
#include <QInputDialog>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>
#include <opencv2/imgcodecs.hpp>

#include "ProcessModule.hpp"
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include "rqwu/rqwu_MessageBox.h"
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

#include <imgqt/imgqt_core.hpp>

#include "Modules.hpp"
#include "ProcessParam.hpp"
#include "Utilty.hpp"
#include "rqwu/rqwu_MessageBox.h"

#include "rqwu/Keyboard/rqwu_NumberKeyboard.h"

#include "imginterop/imginterop_core.hpp"
#include "imghalcon/imghalcon_core.hpp"
#endif

DlgJiudianbiaoding::DlgJiudianbiaoding(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::DlgJiudianbiaodingClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	build_ui();
	build_connect();
#endif
}

DlgJiudianbiaoding::~DlgJiudianbiaoding()
{
#if 0 // --- 以下内容暂时注释 ---
	closeHalconWindow();

	delete _cam1Buffer;
	delete _cam2Buffer;
	_cam1Buffer = nullptr;
	_cam2Buffer = nullptr;
#endif
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---

static int halconCountObj(const HalconCpp::HObject* obj) { /* ... */ }
static QVector<QRectF> exportRectObjToRects(const HalconCpp::HObject* obj) { /* ... */ }

void DlgJiudianbiaoding::build_ui() { /* ... */ }
void DlgJiudianbiaoding::build_connect() { /* ... */ }
bool DlgJiudianbiaoding::ensureHalconWindow() { /* ... */ }
void DlgJiudianbiaoding::closeHalconWindow() { /* ... */ }
void DlgJiudianbiaoding::redrawHalconView(bool clearWindow) { /* ... */ }
void DlgJiudianbiaoding::readImage() { /* ... */ }
void DlgJiudianbiaoding::load_jiuDianParam() { /* ... */ }
void DlgJiudianbiaoding::btn_readImage_clicked() { /* ... */ }
void DlgJiudianbiaoding::setRoiEditingUiEnabled(bool enabled) { /* ... */ }
void DlgJiudianbiaoding::paintCreateRegion() { /* ... */ }
void DlgJiudianbiaoding::btn_paintRegion_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_findRectangle_clicked() { /* ... */ }
void DlgJiudianbiaoding::clearJiudianResults() { /* ... */ }
void DlgJiudianbiaoding::btn_jiudianbiaoding_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_exit() { /* ... */ }
void DlgJiudianbiaoding::btn_wanchengjiudianbiaoding_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_baoguang1_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_zengyi1_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_baoguang2_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_zengyi2_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_remove_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_removeAll_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_MeasureLength1_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_MeasureLength2_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_Threshold_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_num_measures_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_startCamera1_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_stopCamera1_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_startCamera2_clicked() { /* ... */ }
void DlgJiudianbiaoding::btn_stopCamera2_clicked() { /* ... */ }
void DlgJiudianbiaoding::onJiuDianImageTableCellClicked(int row, int column) { /* ... */ }
bool DlgJiudianbiaoding::createShapeModelFromRois() { /* ... */ }
void DlgJiudianbiaoding::createShapeModel() { /* ... */ }
bool DlgJiudianbiaoding::findRectangle(const HalconCpp::HImage& hImg, QVector<RectangleResult>& results, FindRectangleDisplay display, bool showMessage, int maxCount) { /* ... */ }
void DlgJiudianbiaoding::showEvent(QShowEvent* show_event) { /* ... */ }
void DlgJiudianbiaoding::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index) { /* ... */ }
void DlgJiudianbiaoding::handleDualCameraStitch(const HalconCpp::HImage& src, size_t cameraIndex) { /* ... */ }
void DlgJiudianbiaoding::processStitchedImage(HalconCpp::HImage img1, HalconCpp::HImage img2) { /* ... */ }
void DlgJiudianbiaoding::handleSingleCamera(const HalconCpp::HImage& src) { /* ... */ }
void DlgJiudianbiaoding::closeEvent(QCloseEvent* close_event) { /* ... */ }
void DlgJiudianbiaoding::resizeEvent(QResizeEvent* e) { /* ... */ }
void DlgJiudianbiaoding::addNewItemToListView(double pixX, double pixY, double realX, double realY, bool checked) { /* ... */ }
bool DlgJiudianbiaoding::ensureHalconViewPart() { /* ... */ }
void DlgJiudianbiaoding::resetHalconViewPartToFullImage() { /* ... */ }
void DlgJiudianbiaoding::zoomHalconViewAt(const QPoint& hostPos, int steps) { /* ... */ }
void DlgJiudianbiaoding::panHalconViewFromDrag(const QPoint& dragDelta) { /* ... */ }
bool DlgJiudianbiaoding::eventFilter(QObject* watched, QEvent* event) { /* ... */ }
void DlgJiudianbiaoding::clearJiudianImages() { /* ... */ }

#endif
