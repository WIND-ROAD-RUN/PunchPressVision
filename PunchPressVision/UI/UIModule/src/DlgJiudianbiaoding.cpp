#include "DlgJiudianbiaoding.h"

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
			double aa = c1.D();

			rois.push_back(QRectF(c1.D(), r1.D(), c2.D() - c1.D(), r2.D() - r1.D()).normalized());
		}
	}
	catch (...)
	{
		rois.clear();
	}
	return rois;
}

DlgJiudianbiaoding::DlgJiudianbiaoding(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::DlgJiudianbiaodingClass())
{
	ui->setupUi(this);
	build_ui();
	build_connect();
}

DlgJiudianbiaoding::~DlgJiudianbiaoding()
{
	closeHalconWindow();
	
	// 清理双相机缓存
	delete _cam1Buffer;
	delete _cam2Buffer;
	_cam1Buffer = nullptr;
	_cam2Buffer = nullptr;
	
	delete ui;
}

void DlgJiudianbiaoding::build_ui()
{
	auto* old = ui->label_imgDisplay;
	_labelImgDisplaySize = old ? old->size() : QSize();

	// Halcon OpenWindow 的承载窗口（替换原 label_imgDisplay）
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
	// ===== 九点标定表格：5列 =====
	if (ui && ui->tabWidget_jiudianImages)
	{
		auto& t = ui->tabWidget_jiudianImages;
		t->setColumnCount(5);
		t->setHorizontalHeaderLabels({ "图像Index", "像素X", "像素Y", "实际X", "实际Y" });
		t->setSelectionBehavior(QAbstractItemView::SelectRows);
		t->setSelectionMode(QAbstractItemView::SingleSelection);
		t->setEditTriggers(QAbstractItemView::NoEditTriggers);
		t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	}
}

void DlgJiudianbiaoding::build_connect()
{

	QObject::connect(ui->btn_readImage, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_readImage_clicked);
	QObject::connect(ui->btn_paintRegion, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_paintRegion_clicked);
	QObject::connect(ui->btn_findRectangle, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_findRectangle_clicked);
	QObject::connect(ui->btn_jiudianbiaoding, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_jiudianbiaoding_clicked);
	QObject::connect(ui->btn_exit, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_exit);
	QObject::connect(ui->btn_baoguang1, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_baoguang1_clicked);
	QObject::connect(ui->btn_zengyi1, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang2, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_baoguang2_clicked);
	QObject::connect(ui->btn_zengyi2, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_zengyi2_clicked);
	
	QObject::connect(ui->btn_MeasureLength1, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_MeasureLength1_clicked);
	QObject::connect(ui->btn_MeasureLength2, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_MeasureLength2_clicked);
	QObject::connect(ui->btn_Threshold, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_Threshold_clicked);
	QObject::connect(ui->btn_num_measures, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_num_measures_clicked);

	QObject::connect(ui->btn_stopCamera1, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_stopCamera1_clicked);
	QObject::connect(ui->btn_startCamera1, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_startCamera1_clicked);
	QObject::connect(ui->btn_stopCamera2, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_stopCamera2_clicked);
	QObject::connect(ui->btn_startCamera2, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_startCamera2_clicked);

	QObject::connect(ui->btn_remove, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_remove_clicked);
	QObject::connect(ui->btn_removeAll, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_removeAll_clicked);
	QObject::connect(ui->btn_wanchengjiudianbiaoding, &QPushButton::clicked, this, &DlgJiudianbiaoding::btn_wanchengjiudianbiaoding_clicked);

	QObject::connect(ui->tabWidget_jiudianImages, &QTableWidget::cellClicked,
		this, &DlgJiudianbiaoding::onJiuDianImageTableCellClicked);

	QObject::connect(&_cameraDisplayWatcher, &QFutureWatcher<void>::finished, this,
		[this]()
		{
			_genMapRunning.clear(std::memory_order_release);
			_stitchRunning.clear(std::memory_order_release);
		});


}

bool DlgJiudianbiaoding::ensureHalconWindow()
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

void DlgJiudianbiaoding::closeHalconWindow()
{
	try
	{
		if (_halconWindowHandle && _halconWindowHandle->TupleLength() > 0)
			HalconCpp::CloseWindow(*_halconWindowHandle);
	}
	catch (...)
	{
	}

	delete _halconWindowHandle;
	_halconWindowHandle = nullptr;

	delete _halconLastImage;
	_halconLastImage = nullptr;

	_viewPartValid = false;
	_viewImgW = 0;
	_viewImgH = 0;
	_isPanning = false;
}

void DlgJiudianbiaoding::redrawHalconView(bool clearWindow)
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
					DispObj(one, *_halconWindowHandle);
				}
			};
		if (_showPaintCreateRoiObj)
			dispRois("green", _jiudianParam._paintCreateRoiObj);

		if (_jiudianParam._findCreateXldObj)
		{
			HTuple n;
			CountObj(*_jiudianParam._findCreateXldObj, &n);
			if (n.I() > 0)
			{
				SetColor(*_halconWindowHandle, "cyan");
				SetLineWidth(*_halconWindowHandle, 2);
				DispObj(*_jiudianParam._findCreateXldObj, *_halconWindowHandle);
			}
		}
	}
	catch (...)
	{
	}
}
void DlgJiudianbiaoding::readImage()
{
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"选择图片",
		QString(),
		"Image Files (*.jpg *.jpeg *.png *.bmp)");

	if (filePath.isEmpty())
		return;

	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly))
	{
		rw::rqwu::MessageBox::warning(this, "提示", "图片打开失败！");
		return;
	}

	const QByteArray bytes = f.readAll();
	f.close();

	cv::Mat buf(1, bytes.size(), CV_8U, const_cast<char*>(bytes.data()));
	cv::Mat bgr = cv::imdecode(buf, cv::IMREAD_COLOR);

	if (bgr.empty())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "图片解码失败！");
		return;
	}

	if (!_jiudianParam._originalImage)
		_jiudianParam._originalImage = new HalconCpp::HImage();
	*_jiudianParam._originalImage = rw::img::cvMatToHImage(bgr);

	// --- 改动点：ExtractSingleChannel 改为 Halcon 输入/输出 ---
	HalconCpp::HImage hGray;
	QString err;
	if (!ProcessModule::ExtractSingleChannel(*_jiudianParam._originalImage, ProcessModule::SingleChannelType::Gray, hGray, &err))
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString("通道转换失败：%1").arg(err));
		return;
	}

	// 让 img 与显示内容一致（用于ROI校验等）
	if (!_jiudianParam._templateMatImage)
		_jiudianParam._templateMatImage = new HalconCpp::HImage();
	*_jiudianParam._templateMatImage = hGray;

	// 单通道(HImage) -> QPixmap 显示
	JiudianParam::clearCreateRegionAndXld(_jiudianParam);
	_roiPurpose = ProcessModule::RoiPurpose::None;

	if (!_halconLastImage)
		_halconLastImage = new HalconCpp::HImage(hGray);
	else
		*_halconLastImage = hGray;

	redrawHalconView(true);
}

void DlgJiudianbiaoding::load_jiuDianParam()
{
	const std::string dirPath = globalPath.jiuDianParamPath.toStdString();

	const bool ok = _jiudianParam.load(dirPath);
	if (!ok)
	{
		qWarning() << "JiudianParam load failed, path =" << QString::fromStdString(dirPath);

		// 加载失败时：用构造默认值刷新一下UI，避免按钮仍显示旧内容
		if (ui)
		{
			if (ui->btn_MeasureLength1) ui->btn_MeasureLength1->setText(QString::number(_jiudianParam.MeasureLength1));
			if (ui->btn_MeasureLength2) ui->btn_MeasureLength2->setText(QString::number(_jiudianParam.MeasureLength2));
			if (ui->btn_Threshold) ui->btn_Threshold->setText(QString::number(_jiudianParam.MeasureThreshold));
			if (ui->btn_num_measures) ui->btn_num_measures->setText(QString::number(_jiudianParam.num_Measure));

			if (ui->btn_baoguang1) ui->btn_baoguang1->setText(QString::number(_jiudianParam.baoguangshijian1));
			if (ui->btn_zengyi1) ui->btn_zengyi1->setText(QString::number(_jiudianParam.gain1));
			if (ui->btn_baoguang2) ui->btn_baoguang2->setText(QString::number(_jiudianParam.baoguangshijian2));
			if (ui->btn_zengyi2) ui->btn_zengyi2->setText(QString::number(_jiudianParam.gain2));
		}
		return;
	}

	// 1) 同步UI
	if (ui)
	{
		if (ui->btn_MeasureLength1) ui->btn_MeasureLength1->setText(QString::number(_jiudianParam.MeasureLength1));
		if (ui->btn_MeasureLength2) ui->btn_MeasureLength2->setText(QString::number(_jiudianParam.MeasureLength2));
		if (ui->btn_Threshold) ui->btn_Threshold->setText(QString::number(_jiudianParam.MeasureThreshold));
		if (ui->btn_num_measures) ui->btn_num_measures->setText(QString::number(_jiudianParam.num_Measure));

		if (ui->btn_baoguang1) ui->btn_baoguang1->setText(QString::number(_jiudianParam.baoguangshijian1));
		if (ui->btn_zengyi1) ui->btn_zengyi1->setText(QString::number(_jiudianParam.gain1));
		if (ui->btn_baoguang2) ui->btn_baoguang2->setText(QString::number(_jiudianParam.baoguangshijian2));
		if (ui->btn_zengyi2) ui->btn_zengyi2->setText(QString::number(_jiudianParam.gain2));
	}

	// 2) 同步相机（按你当前代码的接口：Exposure int / Gain int）
	auto& camera1 = Modules::getInstance().cameraModule.camera1;
	if (camera1)
	{
		camera1->setExposureTime(_jiudianParam.baoguangshijian1);
		camera1->setGain(static_cast<int>(_jiudianParam.gain1));
	}
	auto& camera2 = Modules::getInstance().cameraModule.camera2;
	if (camera2)
	{
		camera2->setExposureTime(_jiudianParam.baoguangshijian2);
		camera2->setGain(static_cast<int>(_jiudianParam.gain2));
	}
	

	// 3) 同步显示图像：优先 template（灰度），否则 original
	auto isValidImage = [](const HalconCpp::HImage* img) -> bool
		{
			if (!img)
				return false;
			try
			{
				HalconCpp::HTuple w, h;
				HalconCpp::GetImageSize(*img, &w, &h);
				return w.I() > 0 && h.I() > 0;
			}
			catch (...)
			{
				return false;
			}
		};

	const HalconCpp::HImage* showImg = nullptr;
	if (isValidImage(_jiudianParam._templateMatImage))
		showImg = _jiudianParam._templateMatImage;
	else if (isValidImage(_jiudianParam._originalImage))
		showImg = _jiudianParam._originalImage;

	if (showImg)
	{
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(*showImg);
		else
			*_halconLastImage = *showImg;

		redrawHalconView(true); // 会显示加载出来的ROI/XLD叠加
	}
}

void DlgJiudianbiaoding::btn_readImage_clicked()
{
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"选择图片",
		QString(),
		"Image Files (*.jpg *.jpeg *.png *.bmp)");

	if (filePath.isEmpty())
		return;

	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly))
	{
		rw::rqwu::MessageBox::warning(this, "提示", "图片打开失败！");
		return;
	}

	const QByteArray bytes = f.readAll();
	f.close();

	cv::Mat buf(1, bytes.size(), CV_8U, const_cast<char*>(bytes.data()));
	cv::Mat bgr = cv::imdecode(buf, cv::IMREAD_COLOR);

	if (bgr.empty())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "图片解码失败！");
		return;
	}

	QString err{};

	if (!_jiudianParam._originalImage)
		_jiudianParam._originalImage = new HalconCpp::HImage();
	*_jiudianParam._originalImage = rw::img::cvMatToHImage(bgr);

	// --- 改动点：ExtractSingleChannel 改为 Halcon 输入/输出 ---
	HalconCpp::HImage hGray;
	if (!ProcessModule::ExtractSingleChannel(*_jiudianParam._originalImage, ProcessModule::SingleChannelType::Gray, hGray, &err))
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString("通道转换失败：%1").arg(err));
		return;
	}

	if (!_jiudianParam._templateMatImage)
		_jiudianParam._templateMatImage = new HalconCpp::HImage();
	*_jiudianParam._templateMatImage = hGray;

	JiudianParam::clearCreateRegionAndXld(_jiudianParam);
	_roiPurpose = ProcessModule::RoiPurpose::None;

	if (!_halconLastImage)
		_halconLastImage = new HalconCpp::HImage(hGray);
	else
		*_halconLastImage = hGray;

	redrawHalconView(true);
}
void DlgJiudianbiaoding::setRoiEditingUiEnabled(bool enabled)
{
	if (!ui)
		return;

	// 需要禁用/恢复的按钮集中在这里，避免散落各处

	if (ui->btn_readImage) ui->btn_readImage->setEnabled(enabled);
	if (ui->btn_paintRegion) ui->btn_paintRegion->setEnabled(enabled);
	if (ui->btn_findRectangle) ui->btn_findRectangle->setEnabled(enabled);
	if (ui->btn_jiudianbiaoding) ui->btn_jiudianbiaoding->setEnabled(enabled);
	if (ui->btn_exit) ui->btn_exit->setEnabled(enabled);


	// 如需要：在画 ROI 时也禁用下拉框/其他控件，在这里继续加
	// if (ui->comboBox_ImageType) ui->comboBox_ImageType->setEnabled(enabled);
}
void DlgJiudianbiaoding::paintCreateRegion()
{
	isShowImg = false;

	if (!_jiudianParam._templateMatImage)
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
	_roiPurpose = ProcessModule::RoiPurpose::Create;

	try
	{
		using namespace HalconCpp;

		HTuple w, h;
		GetImageSize(*_jiudianParam._templateMatImage, &w, &h);

		HTuple row1, col1, row2, col2;
		DrawRectangle1(*_halconWindowHandle, &row1, &col1, &row2, &col2);

		const QRectF roi(col1.D(), row1.D(), col2.D() - col1.D(), row2.D() - row1.D());
		const QString vErr = ProcessModule::validateRois(QVector<QRectF>{ roi }, w.I(), h.I());
		if (!vErr.isEmpty())
		{
			rw::rqwu::MessageBox::warning(this, "ROI错误", vErr);
			_roiPurpose = ProcessModule::RoiPurpose::None;
			setRoiEditingUiEnabled(true);
			isShowImg = true;
			return;
		}

		HObject roiObj;
		GenRectangle1(&roiObj, roi.top(), roi.left(), roi.bottom(), roi.right());

		// 只允许存在一个 ROI：直接覆盖旧的 ROI
		if (!_jiudianParam._paintCreateRoiObj)
			_jiudianParam._paintCreateRoiObj = new HObject();
		*_jiudianParam._paintCreateRoiObj = roiObj;

		// ROI 变更后，清空“找到的轮廓”显示，避免残留（可选但建议）
		if (!_jiudianParam._findCreateXldObj)
			_jiudianParam._findCreateXldObj = new HObject();
		GenEmptyObj(_jiudianParam._findCreateXldObj);

		redrawHalconView(true);
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::warning(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	catch (...)
	{
	}

	_roiPurpose = ProcessModule::RoiPurpose::None;
	setRoiEditingUiEnabled(true);
	isShowImg = true;
}


void DlgJiudianbiaoding::btn_paintRegion_clicked()
{
	_showPaintCreateRoiObj = true;

	// 进入重新绘制前：清空上一次绘制的矩形ROI + 轮廓显示
	try
	{
		if (!_jiudianParam._paintCreateRoiObj)
			_jiudianParam._paintCreateRoiObj = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(_jiudianParam._paintCreateRoiObj);

		if (!_jiudianParam._findCreateXldObj)
			_jiudianParam._findCreateXldObj = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(_jiudianParam._findCreateXldObj);
	}
	catch (...)
	{
	}

	// 可选：立刻刷新一下，把旧的框从窗口上擦掉
	redrawHalconView(true);

	ui->btn_readImage->setEnabled(false);
	ui->btn_findRectangle->setEnabled(false);
	ui->btn_exit->setEnabled(false);

	paintCreateRegion();
}


//查找模板

void DlgJiudianbiaoding::btn_findRectangle_clicked()
{
	_showPaintCreateRoiObj = true;




	createShapeModel();

	QVector<RectangleResult> results;
	bool isfind = false;

	if (_jiudianParam._templateMatImage)
	{
		FindRectangleDisplay display = FindRectangleDisplay::FoundRectangle;
		if (ui && ui->rad_showrectangle && ui->rad_showrectangle->isChecked())
			display = FindRectangleDisplay::SearchRectangleBox;
		else if (ui && ui->rad_showfindrectangle && ui->rad_showfindrectangle->isChecked())
			display = FindRectangleDisplay::FoundRectangle;

		isfind = findRectangle(*_jiudianParam._templateMatImage, results, display, true, 3);

		if (isfind && !results.isEmpty())
		{
			// 显示找到的所有矩形
			QString msg = QString("找到 %1 个矩形轮廓。\n").arg(results.size());
			for (int i = 0; i < results.size(); ++i)
			{
				msg += QString("矩形%1: (Col=X)=%2, (Row=Y)=%3\n")
					.arg(i + 1)
					.arg(results[i].x, 0, 'f', 2)
					.arg(results[i].y, 0, 'f', 2);
			}
			rw::rqwu::MessageBox::information(this, "提示", msg);

			// 在图上绘制找到的所有点
			try
			{
				if (!_jiudianParam._findCreateXldObj)
					_jiudianParam._findCreateXldObj = new HalconCpp::HObject();

				HalconCpp::HObject allCrosses;
				HalconCpp::GenEmptyObj(&allCrosses);

				for (const auto& result : results)
				{
					HalconCpp::HObject hoCross;
					HalconCpp::GenCrossContourXld(&hoCross, result.y, result.x, 30.0, 0.0);
					
					HalconCpp::HObject temp;
					HalconCpp::ConcatObj(allCrosses, hoCross, &temp);
					allCrosses = temp;
				}

				if (halconCountObj(_jiudianParam._findCreateXldObj) > 0)
				{
					HalconCpp::HObject out;
					HalconCpp::ConcatObj(*_jiudianParam._findCreateXldObj, allCrosses, &out);
					*_jiudianParam._findCreateXldObj = out;
				}
				else
				{
					*_jiudianParam._findCreateXldObj = allCrosses;
				}

				redrawHalconView(false);
			}
			catch (...)
			{
				// 绘制失败不影响主流程
			}

			// 清理 contour 指针
			for (auto& result : results)
			{
				if (result.contour)
				{
					delete result.contour;
					result.contour = nullptr;
				}
			}
		}
		else
		{
			rw::rqwu::MessageBox::warning(this, "提示", "未找到矩形轮廓。");
		}
	}
	else
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
	}
}

void DlgJiudianbiaoding::clearJiudianResults()
{
	// 1) 释放九点结果里保存的 Halcon 对象指针（使用 unloadImage 保留文件路径）
	for (auto& it : _jiudianParam.JiudianResultInfos)
	{
		it.unloadImage();
	}
	_jiudianParam.JiudianResultInfos.clear();

	

	// 3) 清空表格
	clearJiudianImages();

	// 4) 相关状态复位
	isJiuDianBiaoDing = false;
	if (ui && ui->btn_jiudianbiaoding)
		ui->btn_jiudianbiaoding->setEnabled(false);
}
void DlgJiudianbiaoding::btn_jiudianbiaoding_clicked()
{
	// 清空双相机缓存（硬触发模式每次只拍一张，需要确保没有残留缓存）
	{
		std::lock_guard<std::mutex> lock(_stitchMutex);
		delete _cam1Buffer;
		delete _cam2Buffer;
		_cam1Buffer = nullptr;
		_cam2Buffer = nullptr;
		_cam1Ready = false;
		_cam2Ready = false;
	}

	// 开始新一轮采集前：如果已有历史点，先确认是否清空
	if (!_jiudianParam.JiudianResultInfos.isEmpty())
	{
		const auto ret = rw::rqwu::MessageBox::question(
			this,
			"确认",
			"检测到已有历史九点标定数据，是否清空后重新采集？");

		if (ret != rw::rqwu::MessageBox::StandardButton::Yes)
			return;

		clearJiudianResults();
	}
	else
	{
		
		clearJiudianImages();
		if (ui && ui->btn_jiudianbiaoding)
			ui->btn_jiudianbiaoding->setEnabled(false);
	}

	Modules::getInstance().cameraModule.setCamera1HardwareTrigger();
	Modules::getInstance().cameraModule.setCamera2HardwareTrigger();


	// 3s 后再进入九点标定状态（异步，不阻塞UI）

	QTimer::singleShot(3000, this, [this]()
		{
			isJiuDianBiaoDing = true;
			rw::rqwu::MessageBox::information(this, "提示", "准备完成");
		});

}

void DlgJiudianbiaoding::btn_exit()
{
	auto isAccept = rw::rqwu::MessageBox::question(this, "询问", "是否保存");
	if (isAccept == rw::rqwu::MessageBox::StandardButton::Yes)
	{
		_jiudianParam.save(globalPath.jiuDianParamPath.toStdString());

		auto& configManagerModule = Modules::getInstance().configManagerModule;
		const bool ok = configManagerModule._jiudianParam.load(globalPath.jiuDianParamPath.toStdString());
		if (!ok)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "保存成功，但全局九点参数重载失败（请检查路径/文件权限）。");
		}
	}

	this->close();
}

void DlgJiudianbiaoding::btn_wanchengjiudianbiaoding_clicked()
{
	// 完成九点标定：用当前采集到的点位，计算像素->世界 的 Halcon 2D 仿射矩阵
	if (_jiudianParam.JiudianResultInfos.size() < 3)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "九点标定点位不足（至少需要3个点）。");
		return;
	}

	HalconCpp::HTuple homMat2D;
	const bool ok = _jiudianParam.calcPixToWorldHomMat2D(_jiudianParam.JiudianResultInfos, homMat2D);
	if (!ok)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "计算像素->世界变换矩阵失败，请检查点位数据。");
		return;
	}

	// 1) 写入对话框内的参数（用于保存到文件）
	if (!_jiudianParam.outHomMat2D)
		_jiudianParam.outHomMat2D = new HalconCpp::HTuple();
	*_jiudianParam.outHomMat2D = homMat2D;

	// 2) 同步写入全局配置模块（用于运行期直接使用）
	auto& globalParam = Modules::getInstance().configManagerModule._jiudianParam;
	if (!globalParam.outHomMat2D)
		globalParam.outHomMat2D = new HalconCpp::HTuple();
	*globalParam.outHomMat2D = homMat2D;

	rw::rqwu::MessageBox::information(this, "提示", "九点标定完成：已计算并保存像素->世界 HomMat2D。\n请点击“退出”进行保存。");
}

void DlgJiudianbiaoding::btn_baoguang1_clicked()
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
		_jiudianParam.baoguangshijian1 = value.toInt();
	}
}

void DlgJiudianbiaoding::btn_zengyi1_clicked()
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
		_jiudianParam.gain1 = value.toInt();
	}
}

void DlgJiudianbiaoding::btn_baoguang2_clicked()
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
		_jiudianParam.baoguangshijian2 = value.toInt();
	}
}

void DlgJiudianbiaoding::btn_zengyi2_clicked()
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
		_jiudianParam.gain2 = value.toInt();
	}
}

//在这里临时写一个程序点击以后,软触发相机拍一张照片
void DlgJiudianbiaoding::btn_remove_clicked()
{
	auto& camera = Modules::getInstance().cameraModule.camera1;
	if (camera)
	{
		camera->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::ON);
		camera->setTriggerSource(rw::rqwc::TriggerSource::Software);
		camera->executeSoftwareTrigger();
	}
}

void DlgJiudianbiaoding::btn_removeAll_clicked()
{
	const auto ret = rw::rqwu::MessageBox::question(
		this,
		"警告",
		"是否确认要清空所有九点标定数据？");

	if (ret != rw::rqwu::MessageBox::StandardButton::Yes)
		return;

	// 清空九点结果（释放 jiudianImage + 清空表格 + 复位状态）
	clearJiudianResults();

	

	// 可选：清空当前显示叠加（避免残留）
	JiudianParam::clearCreateRegionAndXld(_jiudianParam);
	redrawHalconView(true);
}
void DlgJiudianbiaoding::btn_MeasureLength1_clicked()
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
		ui->btn_MeasureLength1->setText(value);
		
	}
}

void DlgJiudianbiaoding::btn_MeasureLength2_clicked()
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
		ui->btn_MeasureLength2->setText(value);
	}
}

void DlgJiudianbiaoding::btn_Threshold_clicked()
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
		ui->btn_Threshold->setText(value);
	}
}

void DlgJiudianbiaoding::btn_num_measures_clicked()
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
		ui->btn_num_measures->setText(value);
	}
}

void DlgJiudianbiaoding::btn_startCamera1_clicked()
{
	isJiuDianBiaoDing = false;
	auto& camera = Modules::getInstance().cameraModule.camera1;
	if (camera)
	{
		camera->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::OFF);
	}
}

void DlgJiudianbiaoding::btn_stopCamera1_clicked()
{
	auto& camera = Modules::getInstance().cameraModule.camera1;
	if (camera)
	{
		camera->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::ON);
		camera->setTriggerSource(rw::rqwc::TriggerSource::Line0);
	}
}

void DlgJiudianbiaoding::btn_startCamera2_clicked()
{
	isJiuDianBiaoDing = false;
	auto& camera = Modules::getInstance().cameraModule.camera2;
	if (camera)
	{
		camera->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::OFF);
	}
}

void DlgJiudianbiaoding::btn_stopCamera2_clicked()
{
	auto& camera = Modules::getInstance().cameraModule.camera2;
	if (camera)
	{
		camera->setTriggerModeStatus(rw::rqwc::TriggerModeStatus::ON);
		camera->setTriggerSource(rw::rqwc::TriggerSource::Line0);
	}
}

void DlgJiudianbiaoding::onJiuDianImageTableCellClicked(int row, int column)
{
	(void)column;

	if (row < 0 || row >= _jiudianParam.JiudianResultInfos.size())
		return;

	auto& info = _jiudianParam.JiudianResultInfos[row];
	
	// ===== 优化：延迟加载 - 只有用户点击查看时才加载图片 =====
	HalconCpp::HObject* imgObj = info.getImage();
	if (!imgObj)
		return;

	if (!ensureHalconWindow())
		return;
	
	try
	{
		// 浏览采集图片时：不显示创建模板的ROI框
		_showPaintCreateRoiObj = false;

		// 选中图片时：清空“建模时的模板线”等历史叠加，只保留本张图的识别结果
		if (!_jiudianParam._findCreateXldObj)
			_jiudianParam._findCreateXldObj = new HalconCpp::HObject();
		HalconCpp::GenEmptyObj(_jiudianParam._findCreateXldObj);

		// JiudianResult 里存的是 HObject*（实际存入的是 HImage），这里直接转回 HImage 用于显示
		const HalconCpp::HImage img(*imgObj);
		if (!img.IsInitialized())
			return;

		// 同步当前“工作图像”（后续 ROI/找矩形等直接用这张）
		if (!_jiudianParam._templateMatImage)
			_jiudianParam._templateMatImage = new HalconCpp::HImage();
		*_jiudianParam._templateMatImage = img;

		if (!_jiudianParam._originalImage)
			_jiudianParam._originalImage = new HalconCpp::HImage();
		*_jiudianParam._originalImage = img;

		// 同步显示图像
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(img);
		else
			*_halconLastImage = img;

		QVector<RectangleResult> results;
		const bool found = findRectangle(img, results, FindRectangleDisplay::FoundRectangle, false, 3);
		
		// 清理 contour 指针
		for (auto& result : results)
		{
			if (result.contour)
			{
				delete result.contour;
				result.contour = nullptr;
			}
		}

		if (!found || results.isEmpty())
			redrawHalconView(true);
	}
	catch (...)
	{
		// 转换/显示失败直接忽略
	}
}

bool DlgJiudianbiaoding::createShapeModelFromRois()
{
	try
	{
		if (!_jiudianParam._templateMatImage)
			return false;

		if (!_jiudianParam._paintCreateRoiObj || halconCountObj(_jiudianParam._paintCreateRoiObj) <= 0)
			return false;

		HalconCpp::HObject hoCreate = *_jiudianParam._paintCreateRoiObj;
		HalconCpp::HObject hoCreateUnion;
		HalconCpp::Union1(hoCreate, &hoCreateUnion);

		HalconCpp::HObject hoFinal = hoCreateUnion;
	

		// 3) ReduceDomain + CreateShapeModel
		HalconCpp::HImage imgReduced;
		HalconCpp::ReduceDomain(*_jiudianParam._templateMatImage, hoFinal, &imgReduced);

		HalconCpp::HTuple modelIdLocal;
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

		delete _jiudianParam.hv_ModelID;
		_jiudianParam.hv_ModelID = new HalconCpp::HTuple(modelIdLocal);

		HalconCpp::HTuple hv_Row;
		HalconCpp::HTuple hv_Column;
		HalconCpp::HTuple hv_Angle;
		HalconCpp::HTuple hv_Score;

		HalconCpp::FindShapeModel(
			*_jiudianParam._templateMatImage,
			*_jiudianParam.hv_ModelID,
			0, HalconCpp::HTuple(360).TupleRad(),
			0.5,
			1,
			0.5,
			"least_squares",
			0,
			0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		if (hv_Row.TupleLength() < 1)
			return false;

		HalconCpp::HObject ho_ModelContours;
		HalconCpp::HObject ho_ContoursAffineTrans;
		HalconCpp::HTuple hv_HomMat2D;

		HalconCpp::GetShapeModelContours(&ho_ModelContours, *_jiudianParam.hv_ModelID, 1);
		HalconCpp::VectorAngleToRigid(0, 0, 0, hv_Row[0], hv_Column[0], hv_Angle[0], &hv_HomMat2D);
		HalconCpp::AffineTransContourXld(ho_ModelContours, &ho_ContoursAffineTrans, hv_HomMat2D);

		if (!_jiudianParam._findCreateXldObj)
			_jiudianParam._findCreateXldObj = new HalconCpp::HObject();
		*_jiudianParam._findCreateXldObj = ho_ContoursAffineTrans;

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

void DlgJiudianbiaoding::createShapeModel()
{
	if (!_jiudianParam._paintCreateRoiObj || halconCountObj(_jiudianParam._paintCreateRoiObj) <= 0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先绘制ROI。");
		return;
	}

	if (!_jiudianParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}

	// 不再 validateRois：直接使用 _paintCreateRoiObj 建模
	if (!createShapeModelFromRois())
	{
		// 错误/异常信息已在内部弹窗显示，这里直接返回
		return;
	}

	redrawHalconView(true);
}

// 查找多个矩形（最多3个）
bool DlgJiudianbiaoding::findRectangle(const HalconCpp::HImage& hImg, QVector<RectangleResult>& results, FindRectangleDisplay display, bool showMessage, int maxCount)
{
	results.clear();

	// 预检：避免未初始化的 HImage 直接进入后续 Halcon 算子
	try
	{
		HalconCpp::HTuple w, h;
		HalconCpp::GetImageSize(hImg, &w, &h);
		if (w.I() <= 0 || h.I() <= 0)
		{
			if (showMessage)
				rw::rqwu::MessageBox::warning(this, "提示", "传入图像为空，无法识别。");
			return false;
		}
	}
	catch (...)
	{
		if (showMessage)
			rw::rqwu::MessageBox::warning(this, "提示", "传入图像为空，无法识别。");
		return false;
	}

	if (!_jiudianParam.hv_ModelID)
	{
		if (showMessage)
			rw::rqwu::MessageBox::warning(this, "提示", "请先创建模板（CreateShapeModel）。");
		return false;
	}

	if (!_jiudianParam._paintCreateRoiObj || halconCountObj(_jiudianParam._paintCreateRoiObj) <= 0)
	{
		if (showMessage)
			rw::rqwu::MessageBox::warning(this, "提示", "没有可用的绘制矩形（Create ROI）。");
		return false;
	}

	try
	{
		// 1) shape model 匹配得到多个位姿
		HalconCpp::HTuple hv_Row, hv_Column, hv_Angle, hv_Score;
		HalconCpp::FindShapeModel(
			hImg,
			*_jiudianParam.hv_ModelID,
			0,
			HalconCpp::HTuple(360).TupleRad(),
			0.2,
			maxCount,  // 最多匹配个数
			0.5,
			"least_squares",
			0,
			0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		const int matchCount = hv_Row.TupleLength();
		if (matchCount < 1)
		{
			if (showMessage)
				rw::rqwu::MessageBox::warning(this, "提示", "没有找到模板匹配结果。");
			return false;
		}

		// 图像尺寸
		HalconCpp::HTuple hv_Width, hv_Height;
		HalconCpp::GetImageSize(hImg, &hv_Width, &hv_Height);

		// 从按钮读取测量参数
		auto readBtnDouble = [](const QPushButton* btn, double fallback) -> double
			{
				if (!btn)
					return fallback;
				bool ok = false;
				const double v = btn->text().trimmed().toDouble(&ok);
				return ok ? v : fallback;
			};

		double measureLength1 = readBtnDouble(ui ? ui->btn_MeasureLength1 : nullptr, 100.0);
		double measureLength2 = readBtnDouble(ui ? ui->btn_MeasureLength2 : nullptr, 50.0);
		double measureSigma = readBtnDouble(ui ? ui->btn_num_measures : nullptr, 1.0);
		double measureThreshold = readBtnDouble(ui ? ui->btn_Threshold : nullptr, 5.0);

		measureLength1 = std::max(1.0, measureLength1);
		measureLength2 = std::max(1.0, measureLength2);
		measureSigma = std::max(0.1, measureSigma);
		measureThreshold = std::max(0.0, measureThreshold);

		// 直接从 _paintCreateRoiObj 获取 ROI 尺寸
		double roiW = 0.0;
		double roiH = 0.0;
		{
			HalconCpp::HObject one;
			HalconCpp::SelectObj(*_jiudianParam._paintCreateRoiObj, &one, 1);
			HalconCpp::HTuple r1, c1, r2, c2;
			HalconCpp::SmallestRectangle1(one, &r1, &c1, &r2, &c2);
			roiW = std::abs(c2.D() - c1.D());
			roiH = std::abs(r2.D() - r1.D());
		}

		if (roiW <= 0.0 || roiH <= 0.0)
		{
			if (showMessage)
				rw::rqwu::MessageBox::warning(this, "提示", "ROI尺寸无效，无法进行测量。");
			return false;
		}

		// 为每个匹配创建 Metrology 并测量
		HalconCpp::HObject allContours;
		HalconCpp::GenEmptyObj(&allContours);

		for (int i = 0; i < matchCount; ++i)
		{
			const double foundRow = hv_Row[i].D();
			const double foundCol = hv_Column[i].D();
			const double foundAngle = hv_Angle[i].D();

			// 创建 Metrology
			HalconCpp::HTuple hv_MetrologyLocal;
			HalconCpp::CreateMetrologyModel(&hv_MetrologyLocal);
			HalconCpp::SetMetrologyModelImageSize(hv_MetrologyLocal, hv_Width, hv_Height);

			// Add Rectangle2 measure object
			HalconCpp::HTuple hv_Index;
			HalconCpp::AddMetrologyObjectRectangle2Measure(
				hv_MetrologyLocal,
				foundRow, foundCol, foundAngle,
				roiW / 2.0, roiH / 2.0,
				measureLength1, measureLength2, measureSigma, measureThreshold,
				HalconCpp::HTuple(), HalconCpp::HTuple(),
				&hv_Index);

			// Apply
			bool applied = true;
			try
			{
				HalconCpp::ApplyMetrologyModel(hImg, hv_MetrologyLocal);
			}
			catch (...)
			{
				applied = false;
			}

			RectangleResult result;

			if (applied)
			{
				try
				{
					HalconCpp::HObject* hoResultContour = new HalconCpp::HObject();
					HalconCpp::GetMetrologyObjectResultContour(hoResultContour, hv_MetrologyLocal, 0, "all", 1.5);
					result.contour = hoResultContour;
					result.valid = (halconCountObj(result.contour) > 0);

					// 获取轮廓中心点坐标
					if (result.valid)
					{
						HalconCpp::HTuple row, col, phi, length1, length2;
						HalconCpp::SmallestRectangle2Xld(*result.contour, &row, &col, &phi, &length1, &length2);
						if (row.TupleLength() > 0)
						{
							result.x = col[0].D();
							result.y = row[0].D();
						}
						else
						{
							// 备用：使用模板匹配中心
							result.x = foundCol;
							result.y = foundRow;
						}
					}

					// 合并所有轮廓用于显示
					if (result.valid)
					{
						HalconCpp::HObject temp;
						HalconCpp::ConcatObj(allContours, *result.contour, &temp);
						allContours = temp;
					}
				}
				catch (...)
				{
					result.valid = false;
					result.x = foundCol;
					result.y = foundRow;
					if (result.contour)
					{
						delete result.contour;
						result.contour = nullptr;
					}
				}
			}
			else
			{
				// 测量失败，使用模板匹配中心
				result.x = foundCol;
				result.y = foundRow;
			}

			results.append(result);

			// 保存第一个的 MetrologyHandle
			if (i == 0)
			{
				if (_jiudianParam.hv_MetrologyHandle)
				{
					delete _jiudianParam.hv_MetrologyHandle;
					_jiudianParam.hv_MetrologyHandle = nullptr;
				}
				_jiudianParam.hv_MetrologyHandle = new HalconCpp::HTuple(hv_MetrologyLocal);
			}
		}

		// 更新显示
		if (!_jiudianParam._findCreateXldObj)
			_jiudianParam._findCreateXldObj = new HalconCpp::HObject();
		*_jiudianParam._findCreateXldObj = allContours;

		redrawHalconView(true);

		return !results.isEmpty();
	}
	catch (const HalconCpp::HException& e)
	{
		if (showMessage)
			rw::rqwu::MessageBox::critical(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
		return false;
	}
}

void DlgJiudianbiaoding::showEvent(QShowEvent* show_event)
{
	QDialog::showEvent(show_event);

	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = true;
	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = false;
	lastIsCamera1SoftTrigger = Modules::getInstance().cameraModule.isCamera1SoftTrigger.load();
	lastIsCamera2SoftTrigger = Modules::getInstance().cameraModule.isCamera2SoftTrigger.load();
	Modules::getInstance().cameraModule.setCamera1TriggerOff();
	Modules::getInstance().cameraModule.setCamera2TriggerOff();


	_stitchParam=Modules::getInstance().configManagerModule.imageStitchingParam;

	
	ui->btn_jiudianbiaoding->setEnabled(false);

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
			load_jiuDianParam();
		});
}

void DlgJiudianbiaoding::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index)
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

void DlgJiudianbiaoding::handleDualCameraStitch(const HalconCpp::HImage& src, size_t cameraIndex)
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

void DlgJiudianbiaoding::processStitchedImage(HalconCpp::HImage img1, HalconCpp::HImage img2)
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

	
	// 提取灰度通道
	QString err;
	HalconCpp::HImage hGray;
	if (!ProcessModule::ExtractSingleChannel(himage, ProcessModule::SingleChannelType::Gray, hGray, &err))
	{
		QMetaObject::invokeMethod(this, [err]()
		{
			qWarning() << "ExtractSingleChannel failed:" << err;
		}, Qt::QueuedConnection);
		_stitchRunning.clear(std::memory_order_release);
		return;
	}

	// 九点标定模式处理
	if (isJiuDianBiaoDing)
	{
		QVector<RectangleResult> results;

		// 查找矩形（最多3个）
		const bool found = findRectangle(hGray, results, FindRectangleDisplay::FoundRectangle, true, 3);
		
		// 按 x 坐标从小到大排序
		std::sort(results.begin(), results.end(), [](const RectangleResult& a, const RectangleResult& b) {
			return a.x < b.x;
		});
		
		const Position position = PLCControl::getPosition();

		if (found && !results.isEmpty())
		{
			// 保存所有找到的矩形
			QMetaObject::invokeMethod(this,
				[this, hGray, results, position]() mutable
			{
				for (const auto& rect : results)
				{
					 double realX = static_cast<double>(position.x);
					 double realY = static_cast<double>(position.y);
					if (_jiudianParam.JiudianResultInfos.size()==0)
					{
						realX = 400;
						realY = 0;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 1)
					{
						realX = 900;
						realY = 0;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 2)
					{
						realX = 1400;
						realY = 0;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 3)
					{
						realX = 400;
						realY = 100;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 4)
					{
						realX = 900;
						realY = 100;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 5)
					{
						realX = 1400;
						realY = 100;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 6)
					{
						realX = 400;
						realY = 200;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 7)
					{
						realX = 900;
						realY = 200;

					}
					if (_jiudianParam.JiudianResultInfos.size() == 8)
					{
						realX = 1400;
						realY = 200;

					}
					// 保存到九点标定结果
					JiudianResult result;
					result.jiudianImage = new HalconCpp::HObject();
					*result.jiudianImage = hGray;
					result.imageLoaded = true;
					result.pixPosition.x = rect.x;
					result.pixPosition.y = rect.y;
					result.realPosition.x = realX;
					result.realPosition.y = realY;
					
					_jiudianParam.JiudianResultInfos.append(result);

					// 添加到列表显示
					addNewItemToListView(rect.x, rect.y, realX, realY, true);
				}

				const int currentCount = _jiudianParam.JiudianResultInfos.size();

				// 采集够9个点
				if (currentCount >= 9)
				{
					ui->btn_jiudianbiaoding->setEnabled(true);
					isJiuDianBiaoDing = false;
					rw::rqwu::MessageBox::information(
						this,
						"提示",
						"已完成9个标定点采集，可以进行九点标定了！");
				}

				// 清理 contour 指针
				for (auto& result : results)
				{
					if (result.contour)
					{
						delete result.contour;
						result.contour = nullptr;
					}
				}
			}, Qt::QueuedConnection);
		}
		else
		{
			QMetaObject::invokeMethod(this, []()
			{
				rw::rqwu::MessageBox::warning(nullptr, "提示", "未找到矩形，请重新采集！");
			}, Qt::QueuedConnection);
		}
	

	
	
	}

	// 更新显示
	QMetaObject::invokeMethod(this,
		[this, himage, hGray]() mutable
	{
		if (!_jiudianParam._originalImage)
			_jiudianParam._originalImage = new HalconCpp::HImage();
		if (!_jiudianParam._templateMatImage)
			_jiudianParam._templateMatImage = new HalconCpp::HImage();

		*_jiudianParam._originalImage = himage;
		*_jiudianParam._templateMatImage = hGray;

		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(hGray);
		else
			*_halconLastImage = hGray;

		redrawHalconView(true);

		// 释放处理标志
		_stitchRunning.clear(std::memory_order_release);
	}, Qt::QueuedConnection);
}

void DlgJiudianbiaoding::handleSingleCamera(const HalconCpp::HImage& src)
{
	if (_genMapRunning.test_and_set(std::memory_order_acquire))
		return;

	_cameraDisplayWatcher.setFuture(QtConcurrent::run(
		[this, src]() mutable
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

		// 提取灰度
		HalconCpp::HImage hGray;
		if (!ProcessModule::ExtractSingleChannel(himage, ProcessModule::SingleChannelType::Gray, hGray, &err))
		{
			QMetaObject::invokeMethod(this, [err]()
			{
				qWarning() << "ExtractSingleChannel failed:" << err;
			}, Qt::QueuedConnection);
			return;
		}

		// 九点标定模式
		if (isJiuDianBiaoDing)
		{
			QVector<RectangleResult> results;
			const bool found = findRectangle(hGray, results, FindRectangleDisplay::FoundRectangle, true, 3);
			const Position position = PLCControl::getPosition();

			if (found && !results.isEmpty())
			{
				QMetaObject::invokeMethod(this,
					[this, hGray, results, position]() mutable
				{
					for (const auto& rect : results)
					{
						JiudianResult result;
						result.jiudianImage = new HalconCpp::HObject(hGray);
						result.imageLoaded = true;
						result.pixPosition.x = rect.x;
						result.pixPosition.y = rect.y;
						result.realPosition.x = position.x;
						result.realPosition.y = position.y;

						_jiudianParam.JiudianResultInfos.append(result);
						addNewItemToListView(rect.x, rect.y, position.x, position.y, true);
					}

					if (_jiudianParam.JiudianResultInfos.size() >= 9)
					{
						ui->btn_jiudianbiaoding->setEnabled(true);
						isJiuDianBiaoDing = false;
						rw::rqwu::MessageBox::information(this, "提示", "已完成9个标定点采集！");
					}

					// 清理 contour 指针
					for (auto& result : results)
					{
						if (result.contour)
						{
							delete result.contour;
							result.contour = nullptr;
						}
					}
				}, Qt::QueuedConnection);
			}
			else
			{
				// 未找到矩形，也需要清理 contour 指针
				for (auto& result : results)
				{
					if (result.contour)
					{
						delete result.contour;
						result.contour = nullptr;
					}
				}

				QMetaObject::invokeMethod(this, []()
				{
					rw::rqwu::MessageBox::warning(nullptr, "提示", "未找到矩形，请重新采集！");
				}, Qt::QueuedConnection);
			}
		}

		// 更新显示
		QMetaObject::invokeMethod(this,
			[this, himage, hGray]() mutable
		{
			if (!_jiudianParam._originalImage)
				_jiudianParam._originalImage = new HalconCpp::HImage();
			if (!_jiudianParam._templateMatImage)
				_jiudianParam._templateMatImage = new HalconCpp::HImage();

			*_jiudianParam._originalImage = himage;
			*_jiudianParam._templateMatImage = hGray;

			if (!_halconLastImage)
				_halconLastImage = new HalconCpp::HImage(hGray);
			else
				*_halconLastImage = hGray;

			redrawHalconView(true);
		}, Qt::QueuedConnection);
	}));
}

void DlgJiudianbiaoding::closeEvent(QCloseEvent* close_event)
{
	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = false;
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

	isJiuDianBiaoDing = false;

	// 清理双相机缓存
	{
		std::lock_guard<std::mutex> lock(_stitchMutex);
		delete _cam1Buffer;
		delete _cam2Buffer;
		_cam1Buffer = nullptr;
		_cam2Buffer = nullptr;
		_cam1Ready = false;
		_cam2Ready = false;
	}
	_isDualCameraMode = false;

	closeHalconWindow();
}

void DlgJiudianbiaoding::resizeEvent(QResizeEvent* e)
{
	QDialog::resizeEvent(e);

	if (!ensureHalconWindow())
		return;

	try
	{
		HalconCpp::SetWindowExtents(*_halconWindowHandle, 0, 0,
			std::max(1, _halconHost ? _halconHost->width() : width()),
			std::max(1, _halconHost ? _halconHost->height() : height()));
		redrawHalconView(false);
	}
	catch (...)
	{
	}
}

void DlgJiudianbiaoding::addNewItemToListView(double pixX, double pixY, double realX, double realY, bool checked)
{
	if (!ui || !ui->tabWidget_jiudianImages)
		return;

	auto& t = ui->tabWidget_jiudianImages;
	const int row = t->rowCount();
	t->insertRow(row);

	auto mkNumItem = [](double v) -> QTableWidgetItem*
		{
			auto* it = new QTableWidgetItem(QString::number(v, 'f', 2));
			it->setFlags((it->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
			it->setTextAlignment(Qt::AlignCenter);
			return it;
		};

	// 第0列：图像Index（带勾选框）
	auto* idxItem = new QTableWidgetItem(QString::number(listViewCount));
	idxItem->setFlags((idxItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
	idxItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	idxItem->setTextAlignment(Qt::AlignCenter);
	t->setItem(row, 0, idxItem);

	// 第1~4列：数值列
	t->setItem(row, 1, mkNumItem(pixX));
	t->setItem(row, 2, mkNumItem(pixY));
	t->setItem(row, 3, mkNumItem(realX));
	t->setItem(row, 4, mkNumItem(realY));

	listViewCount = t->rowCount();
}

bool DlgJiudianbiaoding::ensureHalconViewPart()
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

void DlgJiudianbiaoding::resetHalconViewPartToFullImage()
{
	_viewPart.r1 = 0.0;
	_viewPart.c1 = 0.0;
	_viewPart.r2 = std::max(0, _viewImgH - 1);
	_viewPart.c2 = std::max(0, _viewImgW - 1);
	_viewPartValid = true;
}

void DlgJiudianbiaoding::zoomHalconViewAt(const QPoint& hostPos, int steps)
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

void DlgJiudianbiaoding::panHalconViewFromDrag(const QPoint& dragDelta)
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

bool DlgJiudianbiaoding::eventFilter(QObject* watched, QEvent* event)
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
void DlgJiudianbiaoding::clearJiudianImages()
{
	listViewCount = 0;
	if (ui->tabWidget_jiudianImages)
		ui->tabWidget_jiudianImages->setRowCount(0);
}

















