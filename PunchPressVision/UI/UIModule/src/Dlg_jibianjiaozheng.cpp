#include "Dlg_jibianjiaozheng.h"

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

#pragma region fuc

void Dlg_jibianjiaozheng::clearLastMarksOverlay()
{
	// 预览阶段不显示叠加：用 empty object 清空，避免频繁 delete/new
	isok = false;

	try
	{
		if (_lastMarksXld)
			HalconCpp::GenEmptyObj(_lastMarksXld);
		if (_lastMarksRegion)
			HalconCpp::GenEmptyObj(_lastMarksRegion);
	}
	catch (...)
	{
		delete _lastMarksXld; _lastMarksXld = nullptr;
		delete _lastMarksRegion; _lastMarksRegion = nullptr;
	}
}


void Dlg_jibianjiaozheng::updateUiFromCalibParam()
{
	if (!ui)
		return;

	if (!_calibParam.cameraParameters)
		return;

	const auto& camPar = *_calibParam.cameraParameters;
	if (camPar.TupleLength() < 13)
		return;

	// area_scan_polynomial:
	// [0]=camera_type, [1]=focus(m), [2]=k1, [3]=k2, [4]=k3, [5]=p1, [6]=p2,
	// [7]=sx(m/px), [8]=sy(m/px), [9]=cx(px), [10]=cy(px), [11]=width(px), [12]=height(px)
	const double focus_m = camPar[1].D();
	const double k1 = camPar[2].D();
	const double k2 = camPar[3].D();
	const double k3 = camPar[4].D();
	const double p1 = camPar[5].D();
	const double p2 = camPar[6].D();
	const double sx_m = camPar[7].D();
	const double sy_m = camPar[8].D();
	const double cx = camPar[9].D();
	const double cy = camPar[10].D();
	const int imgW = camPar[11].I();
	const int imgH = camPar[12].I();

	// 单位转换：m -> mm / um
	const double focus_mm = focus_m * 1000.0;
	const double sx_um = sx_m * 1e6;
	const double sy_um = sy_m * 1e6;

	ui->btn_dangexiangyuandekuan->setText(QString::number(sx_um, 'f', 3));
	ui->btn_dangexiangyuandegao->setText(QString::number(sy_um, 'f', 3));
	ui->btn_jiaojuResult->setText(QString::number(focus_mm, 'f', 3));
	ui->btn_Kappa->setText(QString::number(k1, 'g', 10));
	ui->btn_centerX->setText(QString::number(cx, 'f', 2));
	ui->btn_centerY->setText(QString::number(cy, 'f', 2));
	ui->btn_imgWidth->setText(QString::number(imgW));
	ui->btn_imgHeight->setText(QString::number(imgH));

	qDebug().noquote()
		<< QString("[CalibUI] Apply UI values : Sx=%1 um, Sy=%2 um, Focus=%3 mm, K1=%4, K2=%5, K3=%6, P1=%7, P2=%8, Cx=%9 px, Cy=%10 px, W=%11 px, H=%12 px")
		.arg(sx_um, 0, 'f', 3)
		.arg(sy_um, 0, 'f', 3)
		.arg(focus_mm, 0, 'f', 3)
		.arg(k1, 0, 'g', 10)
		.arg(k2, 0, 'g', 10)
		.arg(k3, 0, 'g', 10)
		.arg(p1, 0, 'g', 10)
		.arg(p2, 0, 'g', 10)
		.arg(cx, 0, 'f', 2)
		.arg(cy, 0, 'f', 2)
		.arg(imgW)
		.arg(imgH);
}

void Dlg_jibianjiaozheng::initial_calibParamUi()
{
	ui->btn_dangexiangyuandekuan->setText("Value");
	ui->btn_dangexiangyuandegao->setText("Value");
	ui->btn_jiaojuResult->setText("Value");
	ui->btn_Kappa->setText("Value");
	ui->btn_centerX->setText("Value");
	ui->btn_centerY->setText("Value");
	ui->btn_imgWidth->setText("Value");
	ui->btn_imgHeight->setText("Value");
}

void Dlg_jibianjiaozheng::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index)
{
	// 如果index与当前选择的相机不匹配,则不更新显示
	if (ui->comboBox_CameraType->currentIndex() + 1 != index)
	{
		return;
	}

	// 避免跨线程直接操作 UI/Halcon Window：切到 UI 线程
	const cv::Mat frame = matInfo.mat.clone();
	QMetaObject::invokeMethod(this,
		[this, frame]() mutable
		{
			if (frame.empty())
				return;

			if (!ensureHalconWindow())
				return;

			// 1) 更新 nowImage（用于“拍照”按钮保存）
			try
			{
				const HalconCpp::HImage hRaw = rw::img::cvMatToHImage(frame);



				delete nowImage;
				nowImage = new HalconCpp::HImage(hRaw);

				// 2) 更新显示用的 lastImage（避免与 nowImage 指针共享导致重复释放）
				if (!_halconLastImage)
					_halconLastImage = new HalconCpp::HImage(hRaw);
				else
					*_halconLastImage = hRaw;
			}
			catch (...)
			{
				return;
			}

			// 3) 预览阶段不做耗时识别：只清空叠加，避免旧结果跟随新画面
			clearLastMarksOverlay();

			// 4) 刷新显示（仅图像）
			redrawHalconView(true);

			//HalconCpp::WriteImage(*nowImage, "jpeg", 0, "C:/Users/zzw/Desktop/temp/222");
		},
		Qt::QueuedConnection);
}
//写一个函数显示找到的halocn标定板的每个原点
#pragma endregion

Dlg_jibianjiaozheng::Dlg_jibianjiaozheng(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_jibianjiaozhengClass())
{
	ui->setupUi(this);

	// 预分配/初始化为 9 个元素（每个元素是空的 cv::Mat）
	CalibImages.clear();

	build_ui();
	build_connect();
}

Dlg_jibianjiaozheng::~Dlg_jibianjiaozheng()
{
	closeHalconWindow();

	clearCalibImages();
	delete nowImage;
	nowImage = nullptr;

	delete ui;
	ui = nullptr;
}

void Dlg_jibianjiaozheng::build_ui()
{
	auto* old = ui->label_imgDisplay;
	_labelImgDisplaySize = old ? old->size() : QSize();

	_halconHost = new QWidget(old->parentWidget());
	_halconHost->setObjectName(old->objectName());
	_halconHost->setStyleSheet(old->styleSheet());
	_halconHost->setSizePolicy(old->sizePolicy());
	_halconHost->setMinimumSize(old->minimumSize());
	_halconHost->setMaximumSize(old->maximumSize());

	_halconHost->setAttribute(Qt::WA_NativeWindow, true);
	_halconHost->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

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

	// 设置tableWidget
	auto& t = ui->tabWidget_calibImages;
	t->setColumnCount(2);
	t->setHorizontalHeaderLabels({ "图像", "状态" });
	t->setSelectionBehavior(QAbstractItemView::SelectRows);
	t->setSelectionMode(QAbstractItemView::SingleSelection);
	t->setEditTriggers(QAbstractItemView::NoEditTriggers);
	t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	ui->btn_jiaoju->setText(QString::number(jiaoju));
	ui->btn_shijiwuliaogaodudaobiaodingbanjuli->setText(QString::number(shijiwuliaogaodudaobiaodingbanjuli));
}
bool Dlg_jibianjiaozheng::ensureHalconWindow()
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
		0, 0, hostW, hostH,
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
void Dlg_jibianjiaozheng::closeHalconWindow()
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

	delete _lastMarksXld;
	_lastMarksXld = nullptr;

	delete _lastMarksRegion;
	_lastMarksRegion = nullptr;
}
void Dlg_jibianjiaozheng::redrawHalconView(bool clearWindow)
{
	if (!ensureHalconWindow())
		return;
	if (!_halconLastImage)
		return;

	bool usedBlend = false;
	HalconCpp::HImage dispImg = *_halconLastImage;
	const HalconCpp::HImage* refImg = _halconLastImage;

	auto hasObj = [](const HalconCpp::HObject* obj) -> bool
		{
			if (!obj)
				return false;
			try
			{
				HalconCpp::HTuple n;
				HalconCpp::CountObj(*obj, &n);
				return (n.TupleLength() > 0 && n[0].I() > 0);
			}
			catch (...)
			{
				return false;
			}
		};

	if (hasObj(_lastMarksRegion) &&
		_marksRegionDrawMode == "fill" &&
		_marksRegionColor == "yellow" &&
		_marksRegionAlpha > 0.0 && _marksRegionAlpha < 1.0)
	{
		HalconCpp::HImage blended;
		if (blendYellowRegion(dispImg, *_lastMarksRegion, _marksRegionAlpha, blended))
		{
			dispImg = blended;
			refImg = &dispImg;
			usedBlend = true;
		}
	}

	QString err;
	if (!_ProcessModule.DispHalconImage(dispImg, *_halconWindowHandle, clearWindow, &err))
	{
		qWarning() << "DispHalconImage failed:" << err;
		return;
	}

	if (hasObj(_lastMarksXld))
		(void)_ProcessModule.DispHalconXld(*_lastMarksXld, *_halconWindowHandle, refImg, _marksXldColor, 2, false, &err);

	if (hasObj(_lastMarksRegion))
	{
		if (!usedBlend)
		{
			(void)_ProcessModule.DispHalconRegion(*_lastMarksRegion, *_halconWindowHandle, refImg,
				_marksRegionColor, _marksRegionDrawMode, 2, false, &err);
		}
		else
		{
			(void)_ProcessModule.DispHalconRegion(*_lastMarksRegion, *_halconWindowHandle, refImg,
				"yellow", "margin", 2, false, &err);
		}
	}
}
void Dlg_jibianjiaozheng::resizeEvent(QResizeEvent* e)
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
void Dlg_jibianjiaozheng::build_connect()
{
	QObject::connect(ui->btn_exit, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_exit_clicked);
	QObject::connect(ui->btn_baoguang1, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_baoguang1_clicked);
	QObject::connect(ui->btn_zengyi1, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang2, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_baoguang2_clicked);
	QObject::connect(ui->btn_zengyi2, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_zengyi2_clicked);

	QObject::connect(ui->btn_startCamera, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_startCamera_clicked);
	QObject::connect(ui->btn_stopCamera, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_stopCamera_clicked);

	QObject::connect(ui->btn_capture, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_capture_clicked);
	QObject::connect(ui->btn_kaishibiaoding, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_kaishibiaoding_clicked);

	QObject::connect(ui->btn_remove, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_remove_clicked);
	QObject::connect(ui->btn_removeAll, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_removeAll_clicked);
	QObject::connect(ui->btn_setAsReferencePose, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_setAsReferencePose_clicked);

	QObject::connect(ui->btn_jiaoju, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_jiaoju_clicked);
	QObject::connect(ui->btn_shijiwuliaogaodudaobiaodingbanjuli, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_shijiwuliaogaodudaobiaodingbanjuli_clicked);
	QObject::connect(ui->btn_ceshi, &QPushButton::clicked, this, &Dlg_jibianjiaozheng::btn_ceshi_clicked);

	// 行点击：拿到 row，用于回显 CalibImages[row]
	QObject::connect(ui->tabWidget_calibImages, &QTableWidget::cellClicked,
		this, &Dlg_jibianjiaozheng::onCalibImageTableCellClicked);

}

void Dlg_jibianjiaozheng::saveCalibParam()
{
	// 同步 UI -> 参数（曝光/增益）
	if (ui)
	{
		_calibParam.baoguangshijian1 = ui->btn_baoguang1->text().toInt();
		_calibParam.gain1 = ui->btn_zengyi1->text().toDouble();
		_calibParam.baoguangshijian2 = ui->btn_baoguang2->text().toInt();
		_calibParam.gain2 = ui->btn_zengyi2->text().toDouble();
	}

	try
	{
		// 同步到全局配置（与 btn_exit_clicked 行为一致）
		if (0 == ui->comboBox_CameraType->currentIndex())
		{
			_calibParam.save(globalPath.calibParamPath1.toStdString());
			Modules::getInstance().configManagerModule.calibParam1 = _calibParam;
		}
		else if (1 == ui->comboBox_CameraType->currentIndex())
		{
			_calibParam.save(globalPath.calibParamPath2.toStdString());
			Modules::getInstance().configManagerModule.calibParam2 = _calibParam;
		}

		Modules::getInstance().configManagerModule.isLoadCalibParam1 = true;
	}
	catch (const std::exception& e)
	{
		rw::rqwu::MessageBox::warning(this, "保存失败", QString("保存畸变矫正参数失败：%1").arg(e.what()));
	}
	catch (...)
	{
		rw::rqwu::MessageBox::warning(this, "保存失败", "保存畸变矫正参数失败：未知异常");
	}
}

void Dlg_jibianjiaozheng::loadCalibParam()
{
	ui->btn_baoguang1->setText(QString::number(50000));
	ui->btn_zengyi1->setText(QString::number(0));
	ui->btn_baoguang2->setText(QString::number(50000));
	ui->btn_zengyi2->setText(QString::number(0));

	updateUiFromCalibParam();

	auto& camera1 = Modules::getInstance().cameraModule.camera1;
	auto& camera2 = Modules::getInstance().cameraModule.camera2;
	if (camera1)
	{
		camera1->setExposureTime(50000);
		camera1->setGain(0);
	}
	if (camera2)
	{
		camera2->setExposureTime(50000);
		camera2->setGain(0);
	}
}


void Dlg_jibianjiaozheng::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);

	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = true;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = false;
	lastIsCamera1SoftTrigger = Modules::getInstance().cameraModule.isCamera1SoftTrigger.load();
	lastIsCamera2SoftTrigger = Modules::getInstance().cameraModule.isCamera2SoftTrigger.load();
	Modules::getInstance().cameraModule.setCamera1TriggerOff();
	Modules::getInstance().cameraModule.setCamera2TriggerOff();

	loadCalibParam();

	clearCalibImages();

	jiaoju = 8;
	shijiwuliaogaodudaobiaodingbanjuli = 1;
	ui->btn_jiaoju->setText(QString::number(jiaoju));
	ui->btn_shijiwuliaogaodudaobiaodingbanjuli->setText(QString::number(shijiwuliaogaodudaobiaodingbanjuli));

	initial_calibParamUi();

	// 延迟到布局完成后再创建/调整 Halcon 窗口
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

void Dlg_jibianjiaozheng::closeEvent(QCloseEvent* close_event)
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
	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = true;

	// 恢复主窗体的相机配置
	auto& cfg = Modules::getInstance().configManagerModule.runningInfo;
	auto& cameraModule = Modules::getInstance().cameraModule;
	cameraModule.setCamera1ExposureTime(cfg.cameraSet.exposureTime1);
	cameraModule.setCamera2ExposureTime(cfg.cameraSet.exposureTime2);
	cameraModule.setCamera1Gain(cfg.cameraSet.gain1);
	cameraModule.setCamera2Gain(cfg.cameraSet.gain2);

	closeHalconWindow();
}

void Dlg_jibianjiaozheng::btn_exit_clicked()
{
	auto isAccepty = rw::rqwu::MessageBox::question(this, "询问", "是否要保存标定矫正参数");
	if (isAccepty != rw::rqwu::MessageBox::StandardButton::Yes)
	{
		this->close();
		return;
	}

	saveCalibParam();

	this->close();
}

void Dlg_jibianjiaozheng::btn_baoguang1_clicked()
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
	}
}

void Dlg_jibianjiaozheng::btn_zengyi1_clicked()
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
	}
}

void Dlg_jibianjiaozheng::btn_baoguang2_clicked()
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
	}
}

void Dlg_jibianjiaozheng::btn_zengyi2_clicked()
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
	}
}

void Dlg_jibianjiaozheng::btn_startCamera_clicked()
{
	if (0 == ui->comboBox_CameraType->currentIndex())
	{
		auto& camera = Modules::getInstance().cameraModule.camera1;
		if (camera)
			camera->startMonitor();
	}
	else if (1 == ui->comboBox_CameraType->currentIndex())
	{
		auto& camera = Modules::getInstance().cameraModule.camera2;
		if (camera)
			camera->startMonitor();
	}
}

void Dlg_jibianjiaozheng::btn_stopCamera_clicked()
{
	if (0 == ui->comboBox_CameraType->currentIndex())
	{
		auto& camera = Modules::getInstance().cameraModule.camera1;
		if (camera)
			camera->stopMonitor();
	}
	else if (1 == ui->comboBox_CameraType->currentIndex())
	{
		auto& camera = Modules::getInstance().cameraModule.camera2;
		if (camera)
			camera->stopMonitor();
	}
}

void Dlg_jibianjiaozheng::btn_capture_clicked()
{
	if (_captureDetectBusy)
		return;

	if (nowImage == nullptr || !nowImage->IsInitialized())
		return;

	if (!ensureHalconWindow())
		return;

	// 对当前帧做快照（避免后台识别过程中 nowImage 被下一帧覆盖/释放）
	HalconCpp::HImage snap;
	try
	{
		snap = *nowImage;
	}
	catch (...)
	{
		return;
	}

	_captureDetectBusy = true;
	if (ui)
		ui->btn_capture->setEnabled(false);

	const quint64 token = ++_captureDetectToken;

	auto* progress = new QProgressDialog("正在识别标定板原点...", QString(), 0, 0, this);
	progress->setWindowModality(Qt::WindowModal);
	progress->setCancelButton(nullptr);
	progress->setMinimumDuration(0);
	progress->show();

	struct DetectResult
	{
		bool ok{ false };
		HalconCpp::HObject xld;
		HalconCpp::HObject reg;
	};

	auto* watcher = new QFutureWatcher<DetectResult>(this);
	QObject::connect(watcher, &QFutureWatcher<DetectResult>::finished, this,
		[this, watcher, progress, snap, token]()
		{
			const DetectResult r = watcher->result();
			watcher->deleteLater();

			progress->close();
			progress->deleteLater();

			_captureDetectBusy = false;
			if (ui)
				ui->btn_capture->setEnabled(true);

			// 如果期间又触发了新的识别任务，忽略旧结果
			if (token != _captureDetectToken)
				return;

			if (!r.ok)
			{
				isok = false;
				rw::rqwu::MessageBox::warning(this, "提示", "未找到标定板原点，无法保存该张标定图。请调整标定板位置/光照后重试。");
				return;
			}

			isok = true;

			// 保存图片 + 保存 marks 缓存
			CalibImages.append(new HalconCpp::HImage(snap));

			{



				auto* pxld = new HalconCpp::HObject();
				auto* preg = new HalconCpp::HObject();
				*pxld = r.xld;
				*preg = r.reg;
				CalibmarksXlds.append(pxld);
				CalibmarksRegions.append(preg);
			}

			addNewItemToListView("NO", false);

			// 可选：把识别结果叠加在当前显示上（下一帧预览会清掉）
			try
			{
				if (!_halconLastImage)
					_halconLastImage = new HalconCpp::HImage(snap);
				else
					*_halconLastImage = snap;

				if (!_lastMarksXld) _lastMarksXld = new HalconCpp::HObject();
				if (!_lastMarksRegion) _lastMarksRegion = new HalconCpp::HObject();
				*_lastMarksXld = r.xld;
				*_lastMarksRegion = r.reg;

				_marksXldColor = "cyan";
				_marksRegionColor = "yellow";
				_marksRegionDrawMode = "fill";
				_marksRegionAlpha = 0.35;

				redrawHalconView(true);
			}
			catch (...)
			{
			}

			rw::rqwu::MessageBox::information(this, "提示", "图像拍照成功");
		});

	watcher->setFuture(QtConcurrent::run([this, snap]() -> DetectResult
		{
			DetectResult r;
			bool ok = false;
			HalconCpp::HObject xld, reg;
			(void)drawCalibMarks(snap, ok, xld, reg);
			r.ok = ok;
			r.xld = xld;
			r.reg = reg;
			return r;
		}));
}

void Dlg_jibianjiaozheng::clearCalibImages()
{
	for (auto*& p : CalibImages)
	{
		delete p;
		p = nullptr;
	}
	CalibImages.clear();

	for (auto*& p : CalibmarksXlds)
	{
		delete p;
		p = nullptr;
	}
	CalibmarksXlds.clear();

	for (auto*& p : CalibmarksRegions)
	{
		delete p;
		p = nullptr;
	}
	CalibmarksRegions.clear();

	listViewCount = 0;
	if (ui->tabWidget_calibImages)
		ui->tabWidget_calibImages->setRowCount(0);
}

void Dlg_jibianjiaozheng::addNewItemToListView(const QString& statusText, bool checked)
{
	auto& t = ui->tabWidget_calibImages;
	const int row = t->rowCount();
	t->insertRow(row);

	// 第0列：图像（带勾选框）
	auto* imgItem = new QTableWidgetItem(QString::number(listViewCount));
	imgItem->setFlags((imgItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
	imgItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	t->setItem(row, 0, imgItem);

	// 第1列：状态
	auto* stItem = new QTableWidgetItem(statusText);
	stItem->setFlags((stItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
	t->setItem(row, 1, stItem);

	listViewCount = t->rowCount();
}

void Dlg_jibianjiaozheng::btn_kaishibiaoding_clicked()
{
	// 先记住“参考图指针”，过滤删图后用它找回新的下标（避免行号错位）
	HalconCpp::HImage* refPtr = nullptr;
	if (_referenceIndex >= 0 && _referenceIndex < CalibImages.size())
		refPtr = CalibImages[_referenceIndex];

	// 先过滤：遍历 CalibImages，用 drawCalibMarks 检测；失败就删掉该图（并同步删掉缓存的 marks）
	for (int i = CalibImages.size() - 1; i >= 0; --i) // 倒序删除避免下标错乱
	{
		if (!CalibImages[i] || !CalibImages[i]->IsInitialized())
		{
			delete CalibImages[i];
			CalibImages.removeAt(i);

			if (i < CalibmarksXlds.size())
			{
				delete CalibmarksXlds[i];
				CalibmarksXlds.removeAt(i);
			}
			if (i < CalibmarksRegions.size())
			{
				delete CalibmarksRegions[i];
				CalibmarksRegions.removeAt(i);
			}
			continue;
		}

		bool isOk = false;
		HalconCpp::HObject marksXld;
		HalconCpp::HObject marksRegion;
		(void)drawCalibMarks(*CalibImages[i], isOk, marksXld, marksRegion);

		if (!isOk)
		{
			delete CalibImages[i];
			CalibImages.removeAt(i);

			if (i < CalibmarksXlds.size())
			{
				delete CalibmarksXlds[i];
				CalibmarksXlds.removeAt(i);
			}
			if (i < CalibmarksRegions.size())
			{
				delete CalibmarksRegions[i];
				CalibmarksRegions.removeAt(i);
			}
		}
		else
		{
			// 可选：把重新检测到的 marks 更新回缓存（保证缓存与当前检测一致）
			if (i < CalibmarksXlds.size() && CalibmarksXlds[i])
				*CalibmarksXlds[i] = marksXld;
			if (i < CalibmarksRegions.size() && CalibmarksRegions[i])
				*CalibmarksRegions[i] = marksRegion;
		}
	}

	// 过滤后：修正参考索引（如果参考图被删，则回退到 0）
	if (refPtr)
	{
		const int newIdx = CalibImages.indexOf(refPtr);
		_referenceIndex = (newIdx >= 0) ? newIdx : 0;
	}
	else
	{
		_referenceIndex = 0;
	}

	// 过滤后：重建表格（保证表格行号与 CalibImages 下标一致）
	if (ui->tabWidget_calibImages)
	{
		ui->tabWidget_calibImages->setRowCount(0);
		listViewCount = 0;
		for (int i = 0; i < CalibImages.size(); ++i)
			addNewItemToListView(i == _referenceIndex ? "REF" : "NO", false);
	}

	// 用指定参考图做参考位姿
	_calibParam = calibrateFromImages(CalibImages, globalPath.describeBoardFile.toStdString(), _referenceIndex);

	updateUiFromCalibParam();

	rw::rqwu::MessageBox::information(this, "提示", "标定完成");
}

void Dlg_jibianjiaozheng::btn_ceshi_clicked()
{
	// 1) 标定结果检查
	if (!_calibParam.cameraParameters || !_calibParam.cameraPose)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先完成标定（获得相机参数/参考位姿）后再测试畸变矫正。");
		return;
	}
	if (_calibParam.cameraParameters->TupleLength() <= 0 || _calibParam.cameraPose->TupleLength() <= 0)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "标定参数为空，请先标定成功后再测试。");
		return;
	}

	// 2) 参考位姿图片检查
	if (_referenceIndex < 0 || _referenceIndex >= CalibImages.size())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "参考位姿索引无效，请先设置参考位姿。");
		return;
	}
	if (!CalibImages[_referenceIndex] || !CalibImages[_referenceIndex]->IsInitialized())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "参考位姿图片无效，请先拍照并设置参考位姿。");
		return;
	}

	if (!ensureHalconWindow())
		return;

	// 3) 调用 CalibParam 内的矫正函数
	// rectWidthMm <= 0：自动按原图视野计算（推荐，用于“大小一致”）
	const double rectWidthMm = _calibParam.rectWidthMm; // 默认 0.0 走自动模式

	HalconCpp::HObject rectMap;
	HalconCpp::HImage rectified;
	const bool ok = _calibParam.rectifyImageToWorldPlane(
		*CalibImages[_referenceIndex],
		*_calibParam.cameraParameters,
		*_calibParam.cameraPose,
		rectWidthMm,
		rectMap,
		rectified);

	if (!ok || !rectified.IsInitialized())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "畸变矫正/平面整形失败。");
		return;
	}

	// 4) 显示矫正后的图（清空叠加，避免干扰）
	delete _lastMarksXld; _lastMarksXld = nullptr;
	delete _lastMarksRegion; _lastMarksRegion = nullptr;

	try
	{
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(rectified);
		else
			*_halconLastImage = rectified;
	}
	catch (...)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "显示矫正图失败。");
		return;
	}

	redrawHalconView(true);
}

void Dlg_jibianjiaozheng::btn_remove_clicked()
{
	auto* t = ui->tabWidget_calibImages;
	if (!t)
		return;

	bool removedAny = false;

	// 倒序删除，避免 removeAt 后下标错乱
	for (int row = t->rowCount() - 1; row >= 0; --row)
	{
		auto* item0 = t->item(row, 0);
		if (!item0)
			continue;

		if (item0->checkState() != Qt::Checked)
			continue;

		removedAny = true;

		if (row >= 0 && row < CalibImages.size())
		{
			delete CalibImages[row];
			CalibImages.removeAt(row);
		}
		if (row >= 0 && row < CalibmarksXlds.size())
		{
			delete CalibmarksXlds[row];
			CalibmarksXlds.removeAt(row);
		}
		if (row >= 0 && row < CalibmarksRegions.size())
		{
			delete CalibmarksRegions[row];
			CalibmarksRegions.removeAt(row);
		}

		t->removeRow(row);
	}

	if (!removedAny)
	{
		rw::rqwu::MessageBox::information(this, "提示", "请先勾选要删除的行。");
		return;
	}

	// 重新刷新第0列编号
	for (int r = 0; r < t->rowCount(); ++r)
	{
		if (auto* item0 = t->item(r, 0))
			item0->setText(QString::number(r));
	}

	listViewCount = t->rowCount();
}

void Dlg_jibianjiaozheng::btn_removeAll_clicked()
{
	const auto ret = rw::rqwu::MessageBox::question(
		this,
		"警告",
		"是否确认要删除所有的保存图片。");

	if (ret != rw::rqwu::MessageBox::StandardButton::Yes)
		return;

	clearCalibImages();
}

void Dlg_jibianjiaozheng::btn_setAsReferencePose_clicked()
{
	auto* t = ui->tabWidget_calibImages;
	if (!t)
		return;

	const int row = t->currentRow();
	if (row < 0)
	{
		rw::rqwu::MessageBox::information(this, "提示", "请先在列表中选中一张图片，再设置参考位姿。");
		return;
	}

	_referenceIndex = row;

	// 可选：把状态列更新一下，直观看到参考图
	for (int r = 0; r < t->rowCount(); ++r)
	{
		if (auto* st = t->item(r, 1))
			st->setText(r == _referenceIndex ? "REF" : "NO");
	}

	rw::rqwu::MessageBox::information(this, "提示", QString("已将第 %1 张图设为参考位姿。").arg(_referenceIndex));
}
void Dlg_jibianjiaozheng::btn_jiaoju_clicked()
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
		jiaoju = value.toDouble();
		ui->btn_jiaoju->setText(value);
	}
}

void Dlg_jibianjiaozheng::btn_shijiwuliaogaodudaobiaodingbanjuli_clicked()
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
		shijiwuliaogaodudaobiaodingbanjuli = value.toDouble();
		ui->btn_shijiwuliaogaodudaobiaodingbanjuli->setText(value);
	}
}

void Dlg_jibianjiaozheng::onCalibImageTableCellClicked(int row, int column)
{
	(void)column;

	if (row < 0 || row >= CalibImages.size())
		return;

	auto* img = CalibImages[row];
	if (!img || !img->IsInitialized())
		return;

	if (!ensureHalconWindow())
		return;

	// 1) 显示选中的标定图
	try
	{
		if (!_halconLastImage)
			_halconLastImage = new HalconCpp::HImage(*img);
		else
			*_halconLastImage = *img;
	}
	catch (...)
	{
		return;
	}

	// 2) 叠加原点（优先用缓存；没有缓存就重新检测一次）
	bool ok = false;
	HalconCpp::HObject xld, reg;

	const bool hasCache =
		(row < CalibmarksXlds.size() && CalibmarksXlds[row]) &&
		(row < CalibmarksRegions.size() && CalibmarksRegions[row]);

	if (hasCache)
	{
		xld = *CalibmarksXlds[row];
		reg = *CalibmarksRegions[row];
		ok = true;
	}
	else
	{
		(void)drawCalibMarks(*_halconLastImage, ok, xld, reg);
	}

	if (ok)
	{
		if (!_lastMarksXld) _lastMarksXld = new HalconCpp::HObject();
		if (!_lastMarksRegion) _lastMarksRegion = new HalconCpp::HObject();
		*_lastMarksXld = xld;
		*_lastMarksRegion = reg;
	}
	else
	{
		delete _lastMarksXld; _lastMarksXld = nullptr;
		delete _lastMarksRegion; _lastMarksRegion = nullptr;
	}

	// 3) 刷新显示
	redrawHalconView(true);
}

bool Dlg_jibianjiaozheng::saveNowImageToCalibSlot(QVector<cv::Mat>& calibImages, const cv::Mat& nowImage, int slotIndex)
{
	if (nowImage.empty())
		return false;

	const int needSize = std::max(9, slotIndex + 1);
	if (calibImages.size() < needSize)
		calibImages.resize(needSize);

	calibImages[slotIndex] = nowImage.clone();

	return true;
}
// 新增实现：在图像上检测并绘制标定板原点（使用 Halcon API，基于 .descr）
// 返回绘制好的图像（不修改原图）
// --- 修改：drawCalibMarks 输入 HImage，输出 XLD + Region（用于显示叠加） ---
bool Dlg_jibianjiaozheng::drawCalibMarks(const HalconCpp::HImage& src, bool& isOk, HalconCpp::HObject& outMarksXld, HalconCpp::HObject& outMarksRegion)
{
	using namespace HalconCpp;

	isOk = false;

	GenEmptyObj(&outMarksXld);
	GenEmptyObj(&outMarksRegion);

	if (!src.IsInitialized())
		return false;

	HTuple hv_W, hv_H;
	try
	{
		src.GetImageSize(&hv_W, &hv_H);
	}
	catch (const HException&)
	{
		return false;
	}

	HTuple hv_CalibHandle;

	HTuple StartParameters;
	StartParameters[0] = "area_scan_polynomial";
	StartParameters[1] = 0.008;
	StartParameters[2] = 0;
	StartParameters[3] = 0;
	StartParameters[4] = 0;
	StartParameters[5] = 0;
	StartParameters[6] = 0;
	StartParameters[7] = 2.4e-06;
	StartParameters[8] = 2.4e-06;
	StartParameters[9] = hv_W[0].D() / 2.0;
	StartParameters[10] = hv_H[0].D() / 2.0;
	StartParameters[11] = hv_W;
	StartParameters[12] = hv_H;

	HTuple hv_FindCalObjParNames, hv_FindCalObjParValues;
	hv_FindCalObjParNames[0] = "gap_tolerance";
	hv_FindCalObjParValues[0] = 1;
	hv_FindCalObjParNames[1] = "alpha";
	hv_FindCalObjParValues[1] = 1;
	hv_FindCalObjParNames[2] = "skip_find_caltab";
	hv_FindCalObjParValues[2] = "false";

	try
	{
		CreateCalibData("calibration_object", 1, 1, &hv_CalibHandle);
		SetCalibDataCamParam(hv_CalibHandle, 0, HTuple(), StartParameters);

		const std::string descrPath = globalPath.describeBoardFile.toStdString();
		SetCalibDataCalibObject(hv_CalibHandle, 0, descrPath.c_str());

		// 转灰度再找板（更稳）
		HImage hGray;
		HTuple hv_Channels;
		CountChannels(src, &hv_Channels);
		if (hv_Channels.TupleLength() > 0 && hv_Channels[0].I() == 3)
			Rgb1ToGray(src, &hGray);
		else
			hGray = src;

		FindCalibObject(hGray, hv_CalibHandle, 0, 0, 0, hv_FindCalObjParNames, hv_FindCalObjParValues);

		HTuple hv_MarkRows, hv_MarkColumns, hv_Ind, hv_CameraPose;
		GetCalibDataObservPoints(hv_CalibHandle, 0, 0, 0, &hv_MarkRows, &hv_MarkColumns, &hv_Ind, &hv_CameraPose);

		if (hv_MarkRows.TupleLength() == 0 || hv_MarkColumns.TupleLength() == 0)
		{
			isOk = false;
			ClearCalibData(hv_CalibHandle);
			return false;
		}

		// 输出 XLD：每个点十字（保留，便于看到原点位置）
		GenCrossContourXld(&outMarksXld, hv_MarkRows, hv_MarkColumns, 12, 0.785398);

		// 注意：这里不再生成整体外接矩形区域（避免在显示时叠加一个大矩形）。
		// outMarksRegion 保持为空对象即可。

		isOk = true;
		ClearCalibData(hv_CalibHandle);
		return true;
	}
	catch (const HException&)
	{
		try { ClearCalibData(hv_CalibHandle); }
		catch (...) {}
		isOk = false;
		return false;
	}
}



CalibParam Dlg_jibianjiaozheng::calibrateFromImages(const QVector<HalconCpp::HImage*>& images, const std::string& descrPath, int referenceIndex)
{
	using namespace HalconCpp;
	CalibParam calibResult;

	// 基础校验
	if (images.empty() || descrPath.empty())
		return calibResult;

	// 找第一张有效图，用于取图像尺寸
	const HImage* first = nullptr;
	for (const auto* img : images)
	{
		if (img != nullptr && img->IsInitialized())
		{
			first = img;
			break;
		}
	}
	if (first == nullptr)
		return calibResult;

	HTuple hv_W, hv_H;
	try
	{
		first->GetImageSize(&hv_W, &hv_H);
	}
	catch (const HException&)
	{
		return calibResult;
	}

	// StartParameters：与 HDevelop 脚本一致
	double jiaoju = ui->btn_jiaoju->text().toDouble();
	double  houdu = ui->btn_shijiwuliaogaodudaobiaodingbanjuli->text().toDouble();

	HTuple hv_StartParameters;
	hv_StartParameters[0] = "area_scan_polynomial";
	hv_StartParameters[1] = jiaoju / 1000;
	hv_StartParameters[2] = 0;
	hv_StartParameters[3] = 0;
	hv_StartParameters[4] = 0;
	hv_StartParameters[5] = 0;
	hv_StartParameters[6] = 0;
	hv_StartParameters[7] = 2.4e-06;
	hv_StartParameters[8] = 2.4e-06;
	hv_StartParameters[9] = hv_W[0].D() / 2.0;
	hv_StartParameters[10] = hv_H[0].D() / 2.0;
	hv_StartParameters[11] = hv_W;
	hv_StartParameters[12] = hv_H;

	HTuple hv_FindCalObjParNames, hv_FindCalObjParValues;
	hv_FindCalObjParNames[0] = "gap_tolerance";
	hv_FindCalObjParValues[0] = 1;
	hv_FindCalObjParNames[1] = "alpha";
	hv_FindCalObjParValues[1] = 1;
	hv_FindCalObjParNames[2] = "skip_find_caltab";
	hv_FindCalObjParValues[2] = "false";

	HTuple hv_CalibHandle;
	try
	{
		// 创建 CalibData 并设置相机初始参数与标定对象描述
		CreateCalibData("calibration_object", 1, 1, &hv_CalibHandle);
		SetCalibDataCamParam(hv_CalibHandle, 0, HTuple(), hv_StartParameters);
		const std::string descrPath = globalPath.describeBoardFile.toStdString();

		SetCalibDataCalibObject(hv_CalibHandle, 0, descrPath.c_str());

		// 为每张有效图像执行 FindCalibObject（观测 index 连续递增）
		int usedIndex = 0;
		for (int i = 0; i < images.size(); ++i)
		{
			const auto* img = images[i];
			if (img == nullptr || !img->IsInitialized())
				continue;

			// 灰度化（如果是彩色）
			HImage hGray;
			HTuple hv_Channels;
			CountChannels(*img, &hv_Channels);
			if (hv_Channels.TupleLength() > 0 && hv_Channels[0].I() == 3)
				Rgb1ToGray(*img, &hGray);
			else
				hGray = *img;

			FindCalibObject(hGray, hv_CalibHandle, 0, 0, usedIndex, hv_FindCalObjParNames, hv_FindCalObjParValues);
			++usedIndex;
		}

		if (usedIndex <= 0)
		{
			ClearCalibData(hv_CalibHandle);
			return calibResult;
		}

		// 执行实际标定
		HTuple hv_Errors;
		CalibrateCameras(hv_CalibHandle, &hv_Errors);

		// 获取标定后的相机参数和参考位姿
		HTuple hv_CameraParameters;
		GetCalibData(hv_CalibHandle, "camera", 0, "params", &hv_CameraParameters);

		referenceIndex = std::clamp(referenceIndex, 0, usedIndex - 1);

		HTuple hv_CameraPose;
		GetCalibData(hv_CalibHandle, "calib_obj_pose", HTuple(0).TupleConcat(referenceIndex), "pose", &hv_CameraPose);


		// 根据板厚调整原点（与 HDevelop 一致）
		SetOriginPose(hv_CameraPose, 0.0, 0.0, houdu / 1000, &hv_CameraPose);

		// 写入 CalibParam
		*calibResult.cameraParameters = hv_CameraParameters;
		*calibResult.cameraPose = hv_CameraPose;
		*calibResult.errors = hv_Errors;
		calibResult.referenceIndex = _referenceIndex;


		ClearCalibData(hv_CalibHandle);
	}
	catch (const HalconCpp::HException&)
	{
		try { ClearCalibData(hv_CalibHandle); }
		catch (...) {}
	}

	return calibResult;
}

bool Dlg_jibianjiaozheng::blendYellowRegion(const HalconCpp::HImage& src, const HalconCpp::HObject& region,
	double alpha01, HalconCpp::HImage& out)
{
	using namespace HalconCpp;

	alpha01 = std::clamp(alpha01, 0.0, 1.0);
	if (alpha01 <= 0.0)
	{
		out = src;
		return true;
	}

	try
	{
		// 1) 保证 RGB
		HImage rgb;
		HTuple ch;
		CountChannels(src, &ch);
		if (ch.TupleLength() > 0 && ch[0].I() == 3)
			rgb = src;
		else
			Compose3(src, src, src, &rgb);

		// 2) 拆通道
		HImage r, g, b;
		Decompose3(rgb, &r, &g, &b);

		// 3) 复制一份用于 overpaint（OverpaintRegion 是原地修改）
		HImage rPaint, gPaint, bPaint;
		CopyObj(r, &rPaint, 1, -1);
		CopyObj(g, &gPaint, 1, -1);
		CopyObj(b, &bPaint, 1, -1);

		// 4) 把 region 涂成黄色：R=255, G=255, B=0
		OverpaintRegion(rPaint, region, 255, "fill");
		OverpaintRegion(gPaint, region, 255, "fill");
		OverpaintRegion(bPaint, region, 0, "fill");

		// 5) alpha 混合：out = base*(1-a) + overlay*a
		HImage rBase, gBase, bBase;
		HImage rOver, gOver, bOver;
		HImage rOut, gOut, bOut;

		ScaleImage(r, &rBase, 1.0 - alpha01, 0.0);
		ScaleImage(g, &gBase, 1.0 - alpha01, 0.0);
		ScaleImage(b, &bBase, 1.0 - alpha01, 0.0);

		ScaleImage(rPaint, &rOver, alpha01, 0.0);
		ScaleImage(gPaint, &gOver, alpha01, 0.0);
		ScaleImage(bPaint, &bOver, alpha01, 0.0);

		AddImage(rBase, rOver, &rOut, 1.0, 0.0);
		AddImage(gBase, gOver, &gOut, 1.0, 0.0);
		AddImage(bBase, bOver, &bOut, 1.0, 0.0);

		Compose3(rOut, gOut, bOut, &out);
		return true;
	}
	catch (...)
	{
		return false;
	}
}