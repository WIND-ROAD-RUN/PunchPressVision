// 必须最先包含：rqwu_MessageBox.h 要在 windows.h(经 Halcon 引入) 定义
// MessageBox 宏之前解析，否则类名会被宏改写为 MessageBoxW。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/PunchPress.h"
#include "ui_PunchPress.h"

#include <QShowEvent>
#include <QResizeEvent>
#include <QStatusBar>

#include "app/PunchPressApp.hpp"
#include "UI/ModelManagerDialog.h"

// 取消 Win32 MessageBox 宏，使用 rw::rqwu::MessageBox。
#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	PunchPress::PunchPress(app::PunchPressApp& app, QWidget* parent)
		: QMainWindow(parent)
		, ui(new Ui::PunchPressClass())
		, app_(app)
	{
		ui->setupUi(this);
		buildConnections();
		ensureHalconWindow();

		// 启动检查（FR-001 ~ FR-004）
		QString startupError;
		if (!app_.performStartupCheck(&startupError))
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("启动检查"), startupError);
		}
		else if (!startupError.isEmpty())
		{
			// 非阻断性提示（标定参数未就绪）
			rw::rqwu::MessageBox::information(this, QStringLiteral("提示"), startupError);
		}
	}

	PunchPress::~PunchPress()
	{
		closeHalconWindow();
		delete ui;
	}

	void PunchPress::buildConnections()
	{
		// App → UI（跨线程 QueuedConnection）
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &PunchPress::onFrameReady, Qt::QueuedConnection);
		connect(&app_, &app::PunchPressApp::positionResultReady,
			this, &PunchPress::onPositionResult, Qt::QueuedConnection);
		connect(&app_, &app::PunchPressApp::modeChanged,
			this, &PunchPress::onModeChanged, Qt::QueuedConnection);
		connect(&app_, &app::PunchPressApp::cameraConnectionChanged,
			this, &PunchPress::onCameraConnectionChanged, Qt::QueuedConnection);
		connect(&app_, &app::PunchPressApp::startupCheckFailed,
			this, &PunchPress::onStartupCheckFailed, Qt::QueuedConnection);

		// UI 控件 → 槽
		connect(ui->rbtn_debug, &QRadioButton::toggled, this, &PunchPress::onDebugToggled);
		connect(ui->rbtn_work, &QRadioButton::toggled, this, &PunchPress::onProductionToggled);
		connect(ui->rbtn_upLight, &QRadioButton::clicked, this, &PunchPress::onUpperLightClicked);
		connect(ui->rbtn_downLight, &QRadioButton::clicked, this, &PunchPress::onLowerLightClicked);
		connect(ui->pbtn_modelManager, &QPushButton::clicked, this, &PunchPress::onModelManager);
		connect(ui->pbtn_exit, &QPushButton::clicked, this, &PunchPress::onExit);
	}

	bool PunchPress::ensureHalconWindow()
	{
		using namespace HalconCpp;
		if (halconWindowHandle_.Length() > 0)
			return true;
		try
		{
			// 将 Halcon 窗口嵌入图像显示标签
			QWidget* host = ui->label_imgDisplay;
			if (!host)
				return false;
			halconHost_ = host;
			const Hlong winId = static_cast<Hlong>(host->winId());
			OpenWindow(0, 0, host->width(), host->height(), winId, "visible", "", &halconWindowHandle_);
			SetWindowAttr("background_color", "gray");
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void PunchPress::closeHalconWindow()
	{
		using namespace HalconCpp;
		if (halconWindowHandle_.Length() == 0)
			return;
		try { CloseWindow(halconWindowHandle_); }
		catch (...) {}
		halconWindowHandle_ = HTuple();
	}

	void PunchPress::displayImage(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;
		if (!image.IsInitialized() || halconWindowHandle_.Length() == 0)
			return;
		try
		{
			HTuple w, h;
			image.GetImageSize(&w, &h);
			SetPart(halconWindowHandle_, 0, 0, h[0].I() - 1, w[0].I() - 1);
			ClearWindow(halconWindowHandle_);
			DispObj(image, halconWindowHandle_);
			lastImage_ = image;
		}
		catch (...)
		{
		}
	}

	void PunchPress::showEvent(QShowEvent* e)
	{
		QMainWindow::showEvent(e);
		ensureHalconWindow();
	}

	void PunchPress::resizeEvent(QResizeEvent* e)
	{
		QMainWindow::resizeEvent(e);
		if (halconWindowHandle_.Length() > 0 && halconHost_)
		{
			try
			{
				HalconCpp::SetWindowExtents(halconWindowHandle_, 0, 0,
					halconHost_->width(), halconHost_->height());
				if (lastImage_.IsInitialized())
					displayImage(lastImage_);
			}
			catch (...) {}
		}
	}

	// ===== 槽实现 =====

	void PunchPress::onDebugToggled(bool checked)
	{
		if (!checked) return;
		QString err;
		if (!app_.switchToMode(global::RunMode::Debug, &err) && !err.isEmpty())
			rw::rqwu::MessageBox::warning(this, QStringLiteral("调试模式"), err);
	}

	void PunchPress::onProductionToggled(bool checked)
	{
		if (!checked) return;
		QString err;
		if (!app_.switchToMode(global::RunMode::Production, &err))
			rw::rqwu::MessageBox::warning(this, QStringLiteral("工作模式"), err);
	}

	void PunchPress::onUpperLightClicked()
	{
		auto& biz = app_.business();
		if (biz.light_control_bun)
			biz.light_control_bun->setUpperLight(ui->rbtn_upLight->isChecked());
	}

	void PunchPress::onLowerLightClicked()
	{
		auto& biz = app_.business();
		if (biz.light_control_bun)
			biz.light_control_bun->setLowerLight(ui->rbtn_downLight->isChecked());
	}

	void PunchPress::onModelManager()
	{
		ModelManagerDialog dlg(app_, this);
		dlg.exec();
	}

	void PunchPress::onExit()
	{
		close();
	}

	void PunchPress::onFrameReady(HalconCpp::HImage image, global::CameraIndex /*camIdx*/)
	{
		displayImage(image);
	}

	void PunchPress::onPositionResult(global::PositionResult result)
	{
		const QString text = result.valid
			? QStringLiteral("X=%1 Y=%2 A=%3 score=%4")
				.arg(result.offsetX, 0, 'f', 3).arg(result.offsetY, 0, 'f', 3)
				.arg(result.angle, 0, 'f', 3).arg(result.score, 0, 'f', 3)
			: QStringLiteral("未匹配");
		statusBar()->showMessage(text);
	}

	void PunchPress::onModeChanged(global::RunMode /*mode*/)
	{
	}

	void PunchPress::onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
	{
		const QString cam = (idx == global::CameraIndex::Camera1) ? QStringLiteral("相机1") : QStringLiteral("相机2");
		statusBar()->showMessage(QStringLiteral("%1 %2 %3")
			.arg(cam, connected ? QStringLiteral("已连接") : QStringLiteral("断开"), reason));
	}

	void PunchPress::onStartupCheckFailed(const QString& reason)
	{
		rw::rqwu::MessageBox::critical(this, QStringLiteral("启动检查失败"), reason);
	}
}
