#include "UI/SpliceDialog.h"
#include "ui_Dlg_ImageStitching.h"

#include "app/PunchPressApp.hpp"

namespace ui
{
	SpliceDialog::SpliceDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_ImageStitchingClass())
		, app_(app)
	{
		ui->setupUi(this);
		buildConnections();
		view1_.ensure(ui->label_imgDisplay1);
		view2_.ensure(ui->label_imgDisplay2);
	}

	SpliceDialog::~SpliceDialog()
	{
		delete ui;
	}

	void SpliceDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &SpliceDialog::onFrameReady, Qt::QueuedConnection);
		connect(ui->btn_pinjieguocheng, &QPushButton::clicked, this, &SpliceDialog::onSplice);
		connect(ui->btn_exit, &QPushButton::clicked, this, &QDialog::accept);
	}

	void SpliceDialog::onSplice()
	{
		auto& biz = app_.business();
		if (!biz.two_camera_splice_bun)
			return;
		if (!cam1Frame_.IsInitialized() || !cam2Frame_.IsInitialized())
		{
			setWindowTitle(QStringLiteral("双相机拼接 - 需先获取两路图像"));
			return;
		}
		biz.two_camera_splice_bun->calculateTwoCameraSpliceConfig(cam1Frame_, cam2Frame_);
		setWindowTitle(QStringLiteral("双相机拼接 - 已计算拼接参数"));
	}

	void SpliceDialog::onFrameReady(HalconCpp::HImage image, global::CameraIndex camIdx)
	{
		if (camIdx == global::CameraIndex::Camera1)
		{
			cam1Frame_ = image;
			view1_.ensure(ui->label_imgDisplay1);
			view1_.display(image);
		}
		else
		{
			cam2Frame_ = image;
			view2_.ensure(ui->label_imgDisplay2);
			view2_.display(image);
		}
	}
}
