// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/DistortionCalibDialog.h"
#include "ui_Dlg_jibianjiaozheng.h"

#include "app/PunchPressApp.hpp"
#include "Business/CalibBun/CalibBun.hpp"

// 取消 Win32 MessageBox 宏
#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	DistortionCalibDialog::DistortionCalibDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_jibianjiaozhengClass())
		, app_(app)
	{
		ui->setupUi(this);
		buildConnections();
		view_.ensure(ui->label_imgDisplay);
	}

	DistortionCalibDialog::~DistortionCalibDialog()
	{
		delete ui;
	}

	void DistortionCalibDialog::buildConnections()
	{
		// 实时帧来自 App 层
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &DistortionCalibDialog::onFrameReady, Qt::QueuedConnection);

		// 复用旧 .ui 的按钮（objectName 见 Dlg_jibianjiaozheng.ui）
		connect(ui->btn_capture, &QPushButton::clicked, this, &DistortionCalibDialog::onCapture);
		connect(ui->btn_remove, &QPushButton::clicked, this, &DistortionCalibDialog::onRemoveLast);
		connect(ui->btn_removeAll, &QPushButton::clicked, this, &DistortionCalibDialog::onRemoveAll);
		connect(ui->btn_kaishibiaoding, &QPushButton::clicked, this, &DistortionCalibDialog::onCalibrate);
		connect(ui->btn_exit, &QPushButton::clicked, this, &QDialog::accept);
	}

	void DistortionCalibDialog::onFrameReady(HalconCpp::HImage image, global::CameraIndex /*camIdx*/)
	{
		lastFrame_ = image;
		view_.ensure(ui->label_imgDisplay);
		view_.display(image);
	}

	void DistortionCalibDialog::onCapture()
	{
		if (lastFrame_.IsInitialized())
			capturedImages_.push_back(lastFrame_);
		setWindowTitle(QStringLiteral("畸变矫正 - 已采集 %1 张").arg(capturedImages_.size()));
	}

	void DistortionCalibDialog::onRemoveLast()
	{
		if (!capturedImages_.empty())
			capturedImages_.pop_back();
		setWindowTitle(QStringLiteral("畸变矫正 - 已采集 %1 张").arg(capturedImages_.size()));
	}

	void DistortionCalibDialog::onRemoveAll()
	{
		capturedImages_.clear();
		setWindowTitle(QStringLiteral("畸变矫正 - 已采集 0 张"));
	}

	void DistortionCalibDialog::onCalibrate()
	{
		auto& biz = app_.business();
		if (!biz.calib_bun)
			return;
		if (capturedImages_.empty())
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("畸变矫正"),
				QStringLiteral("请先采集标定图像"));
			return;
		}

		// TODO(UI): 焦距/板厚/参考序号应从对话框输入控件读取，这里用默认值占位。
		const double focalLengthMm = 8.0;
		const double plateThicknessMm = 0.0;
		const int referenceIndex = 0;

		std::string err;
		const bool ok = biz.calib_bun->calibrateFromImages(
			capturedImages_, focalLengthMm, plateThicknessMm, referenceIndex, &err);
		if (ok)
		{
			// 标定结果已写入 CalibConfig，持久化保存（FR-014）
			auto& inf = biz.infrastructure();
			if (inf.calib_config_module_)
				inf.calib_config_module_->save();
			rw::rqwu::MessageBox::information(this, QStringLiteral("畸变矫正"),
				QStringLiteral("标定完成并已保存"));
		}
		else
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("畸变矫正"),
				QString::fromStdString(err));
		}
	}
}
