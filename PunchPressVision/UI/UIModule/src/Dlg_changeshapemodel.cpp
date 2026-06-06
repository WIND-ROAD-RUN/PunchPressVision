#include "Dlg_changeshapemodel.h"

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

// ROI 校验：返回空字符串表示 OK；否则返回错误原因
static QString validateRois(const QVector<QRectF>& rois, int imgW, int imgH)
{
	if (imgW <= 0 || imgH <= 0)		
		return "当前图像尺寸无效。";

	if (rois.isEmpty())
		return "ROI 列表为空。";

	const QRectF bounds(0.0, 0.0, static_cast<double>(imgW), static_cast<double>(imgH));

	for (int i = 0; i < rois.size(); ++i)
	{
		QRectF r = rois[i];

		// QRectF 允许负宽高，统一规范化再判断
		r = r.normalized();

		auto finite = [](double v) { return std::isfinite(v); };
		if (!finite(r.x()) || !finite(r.y()) || !finite(r.width()) || !finite(r.height()))
			return QString("ROI[%1] 含有非有限数(NaN/Inf)。").arg(i);

		if (r.width() <= 1.0 || r.height() <= 1.0)
			return QString("ROI[%1] 尺寸过小（w/h <= 1）。").arg(i);

		if (r.left() < bounds.left() || r.top() < bounds.top() ||
			r.right() > bounds.right() || r.bottom() > bounds.bottom())
		{
			return QString("ROI[%1] 越界：[ %2,%3,%4,%5 ]，图像范围 [0,0,%6,%7]")
				.arg(i)
				.arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
				.arg(imgW).arg(imgH);
		}
	}

	return QString(); // OK
}

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
			rois.push_back(QRectF(c1.D(), r1.D(), c2.D() - c1.D(), r2.D() - r1.D()).normalized());
		}
	}
	catch (...)
	{
		rois.clear();
	}
	return rois;
}

static void setRoiEditingUiEnabled(Ui::Dlg_changeshapemodelClass* ui, bool enabled)
{
	if (!ui)
		return;

	// 需要禁用/恢复的按钮集中在这里，避免散落各处
	if (ui->btn_exit) ui->btn_exit->setEnabled(enabled);
	if (ui->btn_readImage) ui->btn_readImage->setEnabled(enabled);
	if (ui->btn_paintRegion) ui->btn_paintRegion->setEnabled(enabled);
	if (ui->btn_shiledRegion) ui->btn_shiledRegion->setEnabled(enabled);
	if (ui->btn_changeShapeModel) ui->btn_changeShapeModel->setEnabled(enabled);

	// if (ui->comboBox_ImageType) ui->comboBox_ImageType->setEnabled(enabled);
}

static QString processParamToText(const ProcessParam& p)
{
	QStringList lines;

	lines << "ProcessParam";
	lines << "--------------------------";

	lines << QString("SingleChannelType: %1").arg(static_cast<int>(p._SingleChannelType));

	auto hImageInfo = [](const HalconCpp::HImage* img) -> QString
		{
			if (!img)
				return "null";
			try
			{
				HalconCpp::HTuple w, h;
				HalconCpp::GetImageSize(*img, &w, &h);
				const bool empty = (w.I() <= 0 || h.I() <= 0);
				return QString("%1 x %2  empty=%3").arg(w.I()).arg(h.I()).arg(empty ? "true" : "false");
			}
			catch (...)
			{
				return "invalid";
			}
		};

	lines << QString("_originalImage: %1").arg(hImageInfo(p._originalImage));
	lines << QString("_templateMatImage: %1").arg(hImageInfo(p._templateMatImage));
	lines << QString("hv_ModelID: %1").arg(p.hv_ModelID ? "OK" : "null");
	lines << QString("hv_MetrologyHandle: %1").arg(p.hv_MetrologyHandle ? "OK" : "null");

	lines << QString("CreateRoiObj: %1").arg(halconCountObj(p._paintCreateRoiObj));
	lines << QString("ShieldRoiObj: %1").arg(halconCountObj(p._paintShieldRoiObj));
	lines << QString("FindCreateXldObj: %1").arg(halconCountObj(p._findCreateXldObj));

	return lines.join('\n');
}

static QPixmap hImageToPixmapForDisplay(const HalconCpp::HImage* img)
{
	if (!img)
		return {};
	try
	{
		QImage qimg;
		HalconCpp::HImage tmp = *img;
		rw::img::HImageToQImage(tmp, qimg);
		return QPixmap::fromImage(qimg);
	}
	catch (...)
	{
		return {};
	}
}

static QPixmap renderTextToPixmap(const QString& text, const QSize& targetSize)
{
	const int w = std::max(320, targetSize.width());
	const int h = std::max(180, targetSize.height());

	QPixmap pix(w, h);
	pix.fill(QColor(20, 20, 20));

	QPainter painter(&pix);
	painter.setRenderHint(QPainter::TextAntialiasing, true);

	QFont font("Consolas");
	font.setPointSize(10);
	painter.setFont(font);

	painter.setPen(QColor(220, 220, 220));
	const QRect rect(10, 10, w - 20, h - 20);
	painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);

	return pix;
}

// 新增：方便显示的 cv::Mat -> QPixmap 转换（自动处理常见图像格式）
static QPixmap cvMatToPixmapForDisplay(const cv::Mat& mat)
{
	if (mat.empty())
		return {};

	// 仅做常见类型处理：Gray8 / BGR / BGRA
	switch (mat.type())
	{
	case CV_8UC1:
	{
		QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
		return QPixmap::fromImage(img.copy());
	}
	case CV_8UC3:
	{
		cv::Mat rgb;
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
		QImage img(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
		return QPixmap::fromImage(img.copy());
	}
	case CV_8UC4:
	{
		QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32);
		return QPixmap::fromImage(img.copy());
	}
	default:
		break;
	}

	// 兜底：转 8UC1 显示
	cv::Mat gray;
	if (mat.channels() == 1)
	{
		mat.convertTo(gray, CV_8UC1);
	}
	else
	{
		cv::Mat bgr8;
		mat.convertTo(bgr8, CV_8UC3);
		cv::cvtColor(bgr8, gray, cv::COLOR_BGR2GRAY);
	}

	QImage img(gray.data, gray.cols, gray.rows, static_cast<int>(gray.step), QImage::Format_Grayscale8);
	return QPixmap::fromImage(img.copy());
}

Dlg_changeshapemodel::Dlg_changeshapemodel(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_changeshapemodelClass())
{
	ui->setupUi(this);

	build_ui();
	build_connect();

	isShowImg = true;
}

Dlg_changeshapemodel::~Dlg_changeshapemodel()
{
	delete ui;
}

void Dlg_changeshapemodel::build_ui()
{
	auto* old = ui->label_imgDisplay;

	// 关键：父对象用 old 的 parent（被 layout 管理时通常就是 groupbox/容器）
	panZoomLabel = new PanZoomLabel(old->parentWidget());
	panZoomLabel->setObjectName(old->objectName());
	panZoomLabel->setStyleSheet(old->styleSheet());
	panZoomLabel->setSizePolicy(old->sizePolicy());
	panZoomLabel->setMinimumSize(old->minimumSize());
	panZoomLabel->setMaximumSize(old->maximumSize());

	// 关键：让 layout 接管新的控件尺寸/位置
	if (auto* lay = old->parentWidget() ? old->parentWidget()->layout() : nullptr)
	{
		lay->replaceWidget(old, panZoomLabel);
	}
	else
	{
		// 兜底：如果不在 layout 里，才用几何信息
		panZoomLabel->setGeometry(old->geometry());
	}

	// 可选：保证可见
	panZoomLabel->show();

	// 移除旧控件
	old->hide();
	old->deleteLater();
}

void Dlg_changeshapemodel::setRoiEditingUiEnabled(bool enabled)
{
	if (!ui)
		return;

	// 需要禁用/恢复的按钮集中在这里，避免散落各处
	if (ui->btn_exit) ui->btn_exit->setEnabled(enabled);
	if (ui->btn_readImage) ui->btn_readImage->setEnabled(enabled);
	if (ui->btn_paintRegion) ui->btn_paintRegion->setEnabled(enabled);
	if (ui->btn_shiledRegion) ui->btn_shiledRegion->setEnabled(enabled);
	if (ui->btn_changeShapeModel) ui->btn_changeShapeModel->setEnabled(enabled);

	// 如需要：在画 ROI 时也禁用下拉框/其他控件，在这里继续加
	// if (ui->comboBox_ImageType) ui->comboBox_ImageType->setEnabled(enabled);
}

void Dlg_changeshapemodel::build_connect()
{
	QObject::connect(ui->btn_exit, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_exit_clicked);

	QObject::connect(ui->btn_readImage, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_readImage_clicked);
	QObject::connect(ui->btn_paintRegion, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_paintRegion_clicked);
	QObject::connect(ui->btn_shiledRegion, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_shiledRegion_clicked);
	QObject::connect(ui->btn_changeShapeModel, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_changeShapeModel_clicked);
	QObject::connect(ui->btn_clearRegion, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_clearRegion_clicked);

	QObject::connect(ui->btn_zengyi1, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang1, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_baoguang1_clicked);
	QObject::connect(ui->btn_zengyi2, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_zengyi1_clicked);
	QObject::connect(ui->btn_baoguang2, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_baoguang1_clicked);
	QObject::connect(ui->btn_angle, &QPushButton::clicked, this, &Dlg_changeshapemodel::btn_angle_clicked);

	if (panZoomLabel != nullptr)
	{
		connect(panZoomLabel, &PanZoomLabel::roiRectsConfirmedChanged, this,
			[this](const QVector<QRectF>& rois)
			{
				HalconCpp::HTuple w, h;
				try
				{
					if (_processParam._originalImage)
						HalconCpp::GetImageSize(*_processParam._originalImage, &w, &h);
				}
				catch (...)
				{
					w = 0;
					h = 0;
				}
				const QString err = ProcessModule::validateRois(rois, w.I(), h.I());
				if (!err.isEmpty())
				{
					qWarning() << "[ROI Invalid]" << err << "rois=" << rois;
					rw::rqwu::MessageBox::warning(this, "ROI错误", err);
					return;
				}

				if (!rois.isEmpty())
				{
					const QRectF last = rois.back().normalized();
					_findCreatePaths.clear();
					if (panZoomLabel)
						panZoomLabel->clearOverlayPaths();

					switch (_roiPurpose)
					{
					case ProcessModule::RoiPurpose::Create:
						_paintCreateRois.push_back(last);
						break;
					case ProcessModule::RoiPurpose::Shield:
						_paintShieldRois.push_back(last);
						break;
					default:
						break;
					}
					// 同步分组 ROI 到控件，保证颜色固定
					if (panZoomLabel)
					{
						panZoomLabel->setCreateRois(_paintCreateRois);
						panZoomLabel->setShieldRois(_paintShieldRois);
					}
				}

				_roiPurpose = ProcessModule::RoiPurpose::None;
				setRoiEditingUiEnabled(true);
			});
	}
}

void Dlg_changeshapemodel::onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index)
{
	// 实时显示开关
	if (!isShowImg)
		return;

	HalconCpp::HImage himage;
	QString err{};

	auto isok = ProcessModule::genImageToWorldPlaneMap(
		Modules::getInstance().configManagerModule.calibParam1,
		matInfo.mat, himage, 1, &err);

	// 保存原图（Halcon 格式）
	if (!_processParam._originalImage)
		_processParam._originalImage = new HalconCpp::HImage();
	*_processParam._originalImage = himage;

	// 2) 根据下拉框选择单通道
	auto typeFromIndex = [](int idx) -> ProcessModule::SingleChannelType
		{
			using T = ProcessModule::SingleChannelType;
			switch (idx)
			{
			case 0: return T::Gray;
			case 1: return T::R;
			case 2: return T::G;
			case 3: return T::B;
			case 4: return T::H;
			case 5: return T::S;
			case 6: return T::V;
			default: return T::Gray;
			}
		};

	const int idx = (ui && ui->comboBox_ImageType) ? ui->comboBox_ImageType->currentIndex() : 0;

	// --- 改动点：ExtractSingleChannel 改为 Halcon 输入/输出 ---
	HalconCpp::HImage hCh;
	if (!ProcessModule::ExtractSingleChannel(*_processParam._originalImage, typeFromIndex(idx), hCh, &err))
	{
		qWarning() << "ExtractSingleChannel failed:" << err;
		return;
	}

	// 用于 ROI/建模/识别的图像（单通道）
	if (!_processParam._templateMatImage)
		_processParam._templateMatImage = new HalconCpp::HImage();
	*_processParam._templateMatImage = hCh;

	// 3) 单通道(HImage) -> QPixmap 显示
	QImage q;
	rw::img::HImageToQImage(hCh, q);
	const QPixmap pix = QPixmap::fromImage(q.copy()); // copy 确保生命周期安全

	if (panZoomLabel)
	{
		panZoomLabel->setPixmap(pix);
		if (!_findCreatePaths.isEmpty())
			panZoomLabel->setOverlayPaths(_findCreatePaths);
	}
	else if (ui && ui->label_imgDisplay)
	{
		ui->label_imgDisplay->setPixmap(pix);
		ui->label_imgDisplay->setScaledContents(true);
	}
}

void Dlg_changeshapemodel::btn_exit_clicked()
{
	if (panZoomLabel)
	{
		panZoomLabel->cancelActiveRoi();
	}

	close();
}

void Dlg_changeshapemodel::btn_readImage_clicked()
{
	readImage();
}



void Dlg_changeshapemodel::btn_changeShapeModel_clicked()
{
	auto isCreate = createShapeModel();

	if (isCreate)
	{
		saveModelData();
	}
}
void Dlg_changeshapemodel::btn_paintRegion_clicked()
{
	paintCreateRegion();

}
void Dlg_changeshapemodel::btn_shiledRegion_clicked()
{
	paintShieldRegion();

}

void Dlg_changeshapemodel::btn_clearRegion_clicked()
{
	// 1) 清空对话框侧保存的 ROI 数据
	_paintCreateRois.clear();
	_paintShieldRois.clear();
	_roiPurpose = ProcessModule::RoiPurpose::None;

	// 2) 清空找到的轮廓/区域（叠加显示）
	_findCreatePaths.clear();
	ProcessParam::clearCreateRegionAndXld(_processParam);

	// 3) 同步清空到 PanZoomLabel（显示层 + 内部状态）
	if (panZoomLabel)
	{
		panZoomLabel->cancelActiveRoi();     // 取消当前未确认的绘制
		panZoomLabel->clearAllRois();        // 清空 PanZoomLabel 内部的已确认 ROI/状态
		panZoomLabel->setCreateRois({});     // 清空“建模ROI”显示
		panZoomLabel->setShieldRois({});     // 清空“屏蔽ROI”显示
		panZoomLabel->clearOverlayPaths();   // 清空“找到的区域/轮廓”显示
		panZoomLabel->setRoiMode(false);     // 退出绘制模式
	}

	// 4) UI 恢复（按你现有的交互：开始画 ROI 时会 setRoiEditingUiEnabled(false)）
	//setRoiEditingUiEnabled(true);

	// 5) 可选：恢复实时显示
	isShowImg = true;
}

void Dlg_changeshapemodel::btn_zengyi1_clicked()
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

void Dlg_changeshapemodel::btn_baoguang1_clicked()
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

void Dlg_changeshapemodel::btn_zengyi2_clicked()
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

void Dlg_changeshapemodel::btn_baoguang2_clicked()
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

void Dlg_changeshapemodel::btn_angle_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < -360 || value.toDouble() > 360)
		{
			rw::rqwu::MessageBox::warning(this, "提示", "请输入-360~360的数值");
			return;
		}
		ui->btn_angle->setText(value);
	}
}

bool Dlg_changeshapemodel::saveModelData()
{
	const QString modelPath = QString::fromStdString(
		Modules::getInstance().configManagerModule.runningInfo.filePath.modelPath);

	const QString currentRootPath = QFileInfo(modelPath).absolutePath();
	_processParam.save(currentRootPath.toStdString());

	rw::rqwu::MessageBox::information(this, "成功", "修改成功");

	return true;
}

void Dlg_changeshapemodel::showEvent(QShowEvent* show_event)
{
	isShowImg = true;
	Modules::getInstance().imageTransceiverModule.isOpenCalibParamImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenJiudianbiaodingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenImageStitchingImgEmit = false;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = true;
	lastIsCamera1SoftTrigger = Modules::getInstance().cameraModule.isCamera1SoftTrigger.load();
	lastIsCamera2SoftTrigger = Modules::getInstance().cameraModule.isCamera2SoftTrigger.load();
	Modules::getInstance().cameraModule.setCamera1TriggerOff();
	Modules::getInstance().cameraModule.setCamera2TriggerOff();

	ui->btn_angle->setText(QString::number(0));
	auto& runningInfo = Modules::getInstance().configManagerModule.runningInfo;
	ui->btn_baoguang1->setText(QString::number(runningInfo.cameraSet.exposureTime1));
	ui->btn_zengyi1->setText(QString::number(runningInfo.cameraSet.gain1));
	ui->btn_baoguang2->setText(QString::number(runningInfo.cameraSet.exposureTime2));
	ui->btn_zengyi2->setText(QString::number(runningInfo.cameraSet.gain2));

	ProcessParam::clearCreateRegionAndXld(_processParam);
	_roiPurpose = ProcessModule::RoiPurpose::None;

	// 2) 清空控件叠加显示（ROI/轮廓）
	if (panZoomLabel)
	{
		panZoomLabel->cancelActiveRoi();     // 取消正在编辑的框（预览框/活动框）
		panZoomLabel->clearAllRois();        // 清空旧的“全部 ROI”缓存（_roiImageRects）
		panZoomLabel->setCreateRois({});     // 清空 Create 组 ROI
		panZoomLabel->setShieldRois({});     // 清空 Shield 组 ROI
		panZoomLabel->clearOverlayPaths();   // 清空轮廓叠加
		panZoomLabel->setRoiMode(false);     // 退出 ROI 绘制模式，避免残留状态
		// 不要 panZoomLabel->clear(); 否则会把图片也清空
	}
	_processParam = Modules::getInstance().configManagerModule.processParam1;
	_paintCreateRois = exportRectObjToRects(_processParam._paintCreateRoiObj);
	_paintShieldRois = exportRectObjToRects(_processParam._paintShieldRoiObj);
	_findCreatePaths.clear();

	// === 1) 显示 _templateMatImage ===
	const QPixmap pix = hImageToPixmapForDisplay(_processParam._templateMatImage);

	if (panZoomLabel)
	{
		panZoomLabel->setPixmap(pix);

		// === 2) 叠加显示 ROI（图像坐标：像素）===
		panZoomLabel->setCreateRois(_paintCreateRois);
		panZoomLabel->setShieldRois(_paintShieldRois);

		// === 3) 叠加显示轮廓/路径 ===
		panZoomLabel->setOverlayPaths(_findCreatePaths);
	}
	else if (ui && ui->label_imgDisplay)
	{
		// 兜底（理论上你已经 replaceWidget 了，不会走到这里）
		ui->label_imgDisplay->setPixmap(pix);
		ui->label_imgDisplay->setScaledContents(true);
	}

	QDialog::showEvent(show_event);
}

void Dlg_changeshapemodel::closeEvent(QCloseEvent* close_event)
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
	Modules::getInstance().imageTransceiverModule.isOpenMainWindowImgEmit = true;
	Modules::getInstance().imageTransceiverModule.isOpenCreateModelImgEmit = false;
}

void Dlg_changeshapemodel::readImage()
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

	QString err;
	bool iswrong;
	cv::Mat realimage = bgr;

	if (!_processParam._originalImage)
		_processParam._originalImage = new HalconCpp::HImage();
	*_processParam._originalImage = rw::img::cvMatToHImage(realimage);

	// 根据下拉框选择的通道进行转换（输出单通道图）
	auto typeFromIndex = [](int idx) -> ProcessModule::SingleChannelType
		{
			using T = ProcessModule::SingleChannelType;
			switch (idx)
			{
			case 0: return T::Gray;
			case 1: return T::R;
			case 2: return T::G;
			case 3: return T::B;
			case 4: return T::H;
			case 5: return T::S;
			case 6: return T::V;
			default: return T::Gray;
			}
		};

	const int idx = (ui && ui->comboBox_ImageType) ? ui->comboBox_ImageType->currentIndex() : 0;

	// --- 改动点：ExtractSingleChannel 改为 Halcon 输入/输出 ---
	HalconCpp::HImage hCh;
	if (!ProcessModule::ExtractSingleChannel(*_processParam._originalImage, typeFromIndex(idx), hCh, &err))
	{
		rw::rqwu::MessageBox::warning(this, "提示", QString("通道转换失败：%1").arg(err));
		return;
	}

	// 让 img 与显示内容一致（用于ROI校验等）
	if (!_processParam._templateMatImage)
		_processParam._templateMatImage = new HalconCpp::HImage();
	*_processParam._templateMatImage = hCh;

	// 单通道(HImage) -> QPixmap 显示
	QImage q;
	rw::img::HImageToQImage(hCh, q);
	const QPixmap pix = QPixmap::fromImage(q.copy()); // copy确保数据生命周期安全

	ProcessParam::clearCreateRegionAndXld(_processParam);
	_paintCreateRois.clear();
	_paintShieldRois.clear();
	_findCreatePaths.clear();
	_roiPurpose = ProcessModule::RoiPurpose::None;
	if (panZoomLabel)
	{
		panZoomLabel->clearAllRois();
		panZoomLabel->setCreateRois({});
		panZoomLabel->setShieldRois({});
		panZoomLabel->clearOverlayPaths();   // 关键：清掉旧建模轮廓
		panZoomLabel->setRoiMode(false);
		panZoomLabel->setPixmap(pix);
	}
	else if (ui && ui->label_imgDisplay)
	{
		ui->label_imgDisplay->setPixmap(pix);
		ui->label_imgDisplay->setScaledContents(true);
	}
}



void Dlg_changeshapemodel::paintCreateRegion()
{
	isShowImg = false;
	if (!panZoomLabel)
		return;


	// 进入 ROI 模式
	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}
	// 显示所有已绘制框（Create + Shield）
	panZoomLabel->setCreateRois(_paintCreateRois);
	panZoomLabel->setShieldRois(_paintShieldRois);
	panZoomLabel->cancelActiveRoi();
	panZoomLabel->setRoiMode(true);
	_roiPurpose = ProcessModule::RoiPurpose::Create;
	//setRoiEditingUiEnabled(false);
}

void Dlg_changeshapemodel::paintShieldRegion()
{
	isShowImg = false;
	if (!panZoomLabel)
		return;


	// 进入 ROI 模式
	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return;
	}
	// 显示所有已绘制框（Create + Shield）
	panZoomLabel->setCreateRois(_paintCreateRois);
	panZoomLabel->setShieldRois(_paintShieldRois);
	panZoomLabel->cancelActiveRoi();
	panZoomLabel->setRoiMode(true);
	_roiPurpose = ProcessModule::RoiPurpose::Shield;
	//setRoiEditingUiEnabled(false);
}
bool Dlg_changeshapemodel::createShapeModelFromRois()
{
	try
	{
		if (!_processParam._templateMatImage)
			return false;

		// 1) Create ROI -> HObject(Union)
		HalconCpp::HObject hoCreate;
		HalconCpp::GenEmptyObj(&hoCreate);
		{
			bool hasAny = false;
			for (const QRectF& r0 : _paintCreateRois)
			{
				const QRectF r = r0.normalized();

				const double row1 = r.top();
				const double col1 = r.left();
				const double row2 = r.bottom();
				const double col2 = r.right();

				HalconCpp::HObject hoOne;
				HalconCpp::GenRectangle1(&hoOne, row1, col1, row2, col2);

				if (!hasAny)
				{
					hoCreate = hoOne;
					hasAny = true;
				}
				else
				{
					HalconCpp::HObject hoUnion;
					HalconCpp::Union2(hoCreate, hoOne, &hoUnion);
					hoCreate = hoUnion;
				}
			}
		}
		if (!_processParam._paintCreateRoiObj)
			_processParam._paintCreateRoiObj = new HalconCpp::HObject();
		*_processParam._paintCreateRoiObj = hoCreate;

		// 2) Shield ROI -> HObject(Union) + Difference(Create - Shield)
		HalconCpp::HObject hoFinal = hoCreate;
		HalconCpp::HObject hoShield;
		HalconCpp::GenEmptyObj(&hoShield);
		if (!_paintShieldRois.isEmpty())
		{
			bool hasAny = false;

			for (const QRectF& r0 : _paintShieldRois)
			{
				const QRectF r = r0.normalized();

				const double row1 = r.top();
				const double col1 = r.left();
				const double row2 = r.bottom();
				const double col2 = r.right();

				HalconCpp::HObject hoOne;
				HalconCpp::GenRectangle1(&hoOne, row1, col1, row2, col2);

				if (!hasAny)
				{
					hoShield = hoOne;
					hasAny = true;
				}
				else
				{
					HalconCpp::HObject hoUnion;
					HalconCpp::Union2(hoShield, hoOne, &hoUnion);
					hoShield = hoUnion;
				}
			}

			HalconCpp::HObject hoDiff;
			HalconCpp::Difference(hoCreate, hoShield, &hoDiff);
			hoFinal = hoDiff;
		}

		if (!_processParam._paintShieldRoiObj)
			_processParam._paintShieldRoiObj = new HalconCpp::HObject();
		if (!_paintShieldRois.isEmpty())
			*_processParam._paintShieldRoiObj = hoShield;
		else
			HalconCpp::GenEmptyObj(_processParam._paintShieldRoiObj);

		// 3) ReduceDomain + CreateShapeModel
		HalconCpp::HImage imgReduced;
		HalconCpp::ReduceDomain(*_processParam._templateMatImage, hoFinal, &imgReduced);

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

		// 保存到成员（你.h里是 HalconCpp::HTuple* hv_ModelID;）

// 保存到 processParam1 中的 hv_ModelID
		delete _processParam.hv_ModelID;
		_processParam.hv_ModelID = new HalconCpp::HTuple(modelIdLocal);
		HalconCpp::HTuple hv_Row;
		HalconCpp::HTuple hv_Column;
		HalconCpp::HTuple hv_Angle;
		HalconCpp::HTuple hv_Score;

		// ====== 你原来的 FindShapeModel + 轮廓显示代码保持不变 ======
		HalconCpp::FindShapeModel(
			*_processParam._templateMatImage,
			*_processParam.hv_ModelID,
			0, HalconCpp::HTuple(360).TupleRad(),
			0.5,
			1,
			0.5,
			"least_squares",
			0,
			0.9,
			&hv_Row, &hv_Column, &hv_Angle, &hv_Score);

		if (hv_Row.TupleLength() >= 1)
		{
			// 轮廓 + 仿射变换（便于显示/调试）
			HalconCpp::HObject ho_ModelContours;
			HalconCpp::HObject ho_ContoursAffineTrans;
			HalconCpp::HTuple hv_HomMat2D;

			HalconCpp::GetShapeModelContours(&ho_ModelContours, *_processParam.hv_ModelID, 1);
			HalconCpp::VectorAngleToRigid(0, 0, 0, hv_Row[0], hv_Column[0], hv_Angle[0], &hv_HomMat2D);
			HalconCpp::AffineTransContourXld(ho_ModelContours, &ho_ContoursAffineTrans, hv_HomMat2D);
			// 显示轮廓：Halcon XLD -> QPainterPath（图像坐标）
			// 显示轮廓：按对象遍历（CountObj + SelectObj + GetContourXld）
			if (!_processParam._findCreateXldObj)
				_processParam._findCreateXldObj = new HalconCpp::HObject();
			*_processParam._findCreateXldObj = ho_ContoursAffineTrans;

			_findCreatePaths.clear();
			QPainterPath allPath;
			HalconCpp::HTuple hv_Number;
			HalconCpp::CountObj(ho_ContoursAffineTrans, &hv_Number);
			const Hlong number = hv_Number.TupleLength() > 0 ? hv_Number[0].L() : 0;

			for (Hlong index = 1; index <= number; ++index)
			{
				HalconCpp::HObject objSelected;
				HalconCpp::SelectObj(ho_ContoursAffineTrans, &objSelected, index);

				HalconCpp::HTuple rows, cols;
				HalconCpp::GetContourXld(objSelected, &rows, &cols);

				const Hlong n = rows.TupleLength();
				if (n <= 0)
					continue;

				bool first = true;
				for (Hlong i = 0; i < n; ++i)
				{
					const double r = rows[i].D();
					const double c = cols[i].D();
					if (!std::isfinite(r) || !std::isfinite(c))
						continue;

					const QPointF pt(c, r);
					if (first)
					{
						allPath.moveTo(pt);
						first = false;
					}
					else
					{
						allPath.lineTo(pt);
					}
				}
			}
			if (!allPath.isEmpty())
				_findCreatePaths.push_back(allPath);



			// hv_Score 可能为空，防护读取
			const double score = (hv_Score.TupleLength() > 0) ? hv_Score[0].D() : 0.0;
			rw::rqwu::MessageBox::information(
				this,
				"提示",
				QString("建模成功，初次匹配score=%1").arg(score));


			return true;
		}
		else
		{
			return false;
		}
	}
	catch (const HalconCpp::HException& e)
	{
		rw::rqwu::MessageBox::critical(this, "Halcon异常", QString::fromLocal8Bit(e.ErrorMessage().Text()));
	}
	return false;



}

bool Dlg_changeshapemodel::createShapeModel()
{
	// 先删除原来绘制/找到的轮廓显示（避免残留）
	_findCreatePaths.clear();
	if (panZoomLabel)
		panZoomLabel->clearOverlayPaths();

	// 创建模板时不显示绘制框（Create/Shield）
	if (panZoomLabel)
	{
		panZoomLabel->setCreateRois({});
		panZoomLabel->setShieldRois({});
		panZoomLabel->cancelActiveRoi();
		panZoomLabel->setRoiMode(false);
	}


	if (_paintCreateRois.isEmpty())
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先绘制ROI。");
		return false;
	}

	if (!_processParam._templateMatImage)
	{
		rw::rqwu::MessageBox::warning(this, "提示", "请先加载图片。");
		return false;
	}

	// 防御：避免异常ROI进入Halcon
	HalconCpp::HTuple w, h;
	HalconCpp::GetImageSize(*_processParam._templateMatImage, &w, &h);
	const QString roiErr = ProcessModule::validateRois(_paintCreateRois, w.I(), h.I());
	if (!roiErr.isEmpty())
	{
		rw::rqwu::MessageBox::warning(this, "ROI错误", roiErr);
		return false;
	}

	// Shield ROI 允许为空；不为空则也校验一次
	if (!_paintShieldRois.isEmpty())
	{
		const QString shieldErr = ProcessModule::validateRois(_paintShieldRois, w.I(), h.I());
		if (!shieldErr.isEmpty())
		{
			rw::rqwu::MessageBox::warning(this, "ROI错误", QString("屏蔽ROI错误：%1").arg(shieldErr));
			return false;
		}
	}


	if (!createShapeModelFromRois())
	{
		return false;
	}
	else
	{

		if (panZoomLabel)
			panZoomLabel->setOverlayPaths(_findCreatePaths);
		return true;
	}

}



