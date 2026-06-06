#pragma once

#include <QDialog>
#include <QListView>
#include <QSize>
#if 0 // --- 以下项目引用暂时注释 ---
#include "CalibParam.hpp"
#include "func/ProcessModule.hpp"
#include "ui_Dlg_jibianjiaozheng.h"
#include <opencv2/opencv.hpp>
#include <Utilty.hpp>
#include <rqwccore/rqwc_types.hpp>

class QStringListModel;

namespace HalconCpp
{
	class HTuple;
	class HObject;
	class HImage;
}
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_jibianjiaozhengClass; };
QT_END_NAMESPACE

class Dlg_jibianjiaozheng : public QDialog
{
	Q_OBJECT
public:
	Dlg_jibianjiaozheng(QWidget* parent = nullptr);
	~Dlg_jibianjiaozheng();
#if 0 // --- 以下内容暂时注释 ---

public:
	void build_ui();
	void build_connect();

	void saveCalibParam();
	void loadCalibParam();
protected:
	void resizeEvent(QResizeEvent*) override;
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent*) override;

private slots:
	void btn_exit_clicked();
	void btn_baoguang1_clicked();
	void btn_zengyi1_clicked();
	void btn_baoguang2_clicked();
	void btn_zengyi2_clicked();
	void btn_startCamera_clicked();
	void btn_stopCamera_clicked();
	void btn_capture_clicked();
	void btn_kaishibiaoding_clicked();
	void btn_ceshi_clicked();
	void btn_remove_clicked();
	void btn_removeAll_clicked();
	void btn_setAsReferencePose_clicked();
	void btn_jiaoju_clicked();
	void btn_shijiwuliaogaodudaobiaodingbanjuli_clicked();

	void onCalibImageTableCellClicked(int row, int column);
private:
	bool lastIsCamera1SoftTrigger{ false };
	bool lastIsCamera2SoftTrigger{ false };
private:
	double jiaoju{ 8 };
	double shijiwuliaogaodudaobiaodingbanjuli{ 1 };
private:
	QVector<HalconCpp::HImage*> CalibImages{ nullptr };
	QVector<HalconCpp::HObject*> CalibmarksXlds{ nullptr };
	QVector<HalconCpp::HObject*> CalibmarksRegions{ nullptr };

	QString _marksXldColor{ "cyan" };
	QString _marksRegionColor{ "red" };
	QString _marksRegionDrawMode{ "margin" };
	double _marksRegionAlpha{ 1.0 };

	void clearCalibImages();
	bool isok = false;
	void addNewItemToListView(const QString& statusText, bool checked);
	int listViewCount{ 0 };

	void clearLastMarksOverlay();

	bool _captureDetectBusy{ false };
	quint64 _captureDetectToken{ 0 };
public:
	bool saveNowImageToCalibSlot(QVector<cv::Mat>& calibImages, const cv::Mat& nowImage, int slotIndex);
	bool drawCalibMarks(const HalconCpp::HImage& src, bool& isOk, HalconCpp::HObject& outMarksXld, HalconCpp::HObject& outMarksRegion);
	CalibParam calibrateFromImages(const QVector<HalconCpp::HImage*>& images, const std::string& descrPath, int referenceIndex);
	bool blendYellowRegion(const HalconCpp::HImage& src,const HalconCpp::HObject& region,double alpha01,HalconCpp::HImage& out);

	HalconCpp::HImage* nowImage = nullptr;
	CalibParam _calibParam;

	void updateUiFromCalibParam();
	void initial_calibParamUi();
public slots:
	void onCameraDisplay(rw::rqwc::MatInfo matInfo, size_t index);
private:
	int _referenceIndex{ 0 };
private:
	QWidget* _halconHost = nullptr;
	QSize _labelImgDisplaySize;
	HalconCpp::HTuple* _halconWindowHandle = nullptr;
	HalconCpp::HImage* _halconLastImage = nullptr;

	HalconCpp::HObject* _lastMarksXld = nullptr;
	HalconCpp::HObject* _lastMarksRegion = nullptr;

	ProcessModule _ProcessModule;

	bool ensureHalconWindow();
	void closeHalconWindow();
	void redrawHalconView(bool clearWindow);
#endif
private:
	Ui::Dlg_jibianjiaozhengClass* ui;
};
