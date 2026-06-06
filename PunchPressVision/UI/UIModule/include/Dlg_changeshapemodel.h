#pragma once

#include <QDialog>
#include <QFutureWatcher>
#if 0 // --- 以下项目引用暂时注释 ---
#include <opencv2/core/mat.hpp>
#include <rqwccore/rqwc_types.hpp>

#include "PanZoomLabel.h"
#include "ProcessModule.hpp"
#include "ProcessParam.hpp"
#include "ui_Dlg_changeshapemodel.h"

namespace HalconCpp
{
	class HTuple;
	class HImage;
}
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_changeshapemodelClass; };
QT_END_NAMESPACE

class Dlg_changeshapemodel : public QDialog
{
	Q_OBJECT
public:
	Dlg_changeshapemodel(QWidget *parent = nullptr);
	~Dlg_changeshapemodel();
#if 0 // --- 以下内容暂时注释 ---
private:
	QFutureWatcher<std::pair<double, cv::Mat>> _matchWatcher;
	bool _matchRunning = false;
public:
	void build_ui();
	void setRoiEditingUiEnabled(bool enabled);
	void build_connect();
public slots:
	void onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index);
private slots:
	void btn_exit_clicked();

	void btn_readImage_clicked();
	void btn_paintRegion_clicked();
	void btn_changeShapeModel_clicked();
	void btn_shiledRegion_clicked();
	void btn_clearRegion_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang1_clicked();
	void btn_zengyi2_clicked();
	void btn_baoguang2_clicked();
	void btn_angle_clicked();
private:
	bool isShowImg{ false };

	bool lastIsCamera1SoftTrigger{ false };
	bool lastIsCamera2SoftTrigger{ false };
private:
	PanZoomLabel* panZoomLabel = nullptr;

	bool saveModelData();

protected:
	void showEvent(QShowEvent*) override;
	void closeEvent(QCloseEvent*) override;
private:
	QImage qimg;
	ProcessParam _processParam;
	ProcessModule::RoiPurpose _roiPurpose{ ProcessModule::RoiPurpose::None };

	QVector<QRectF> _paintCreateRois;
	QVector<QRectF> _paintShieldRois;
	QVector<QPainterPath> _findCreatePaths;
private:
	void readImage();
	void paintCreateRegion();
	void paintShieldRegion();
	bool createShapeModelFromRois();
	bool createShapeModel();
#endif
private:
	Ui::Dlg_changeshapemodelClass *ui;
};
