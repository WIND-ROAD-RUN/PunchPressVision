// 必须最先包含：rqwu_MessageBox.h 要在 windows.h(经 Halcon 引入) 定义
// MessageBox 宏之前解析，否则类名会被宏改写为 MessageBoxW。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/PunchPress.h"
#include "ui_PunchPress.h"

#include <QButtonGroup>
#include <QLabel>
#include <QLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QStatusBar>

#include "app/PunchPressApp.hpp"
#include "UI/HalconInteractiveLabel.h"
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

		// 按钮分组：隔离模式和光源两组 RadioButton 的互斥作用域
		modeGroup_ = new QButtonGroup(this);
		modeGroup_->setExclusive(false); // 允许取消选中，进入 Idle/停止模式
		modeGroup_->addButton(ui->rbtn_debug);
		modeGroup_->addButton(ui->rbtn_work);

		// 开机时调试/工作模式均不选中，对应 Idle 状态
		ui->rbtn_debug->setChecked(false);
		ui->rbtn_work->setChecked(false);

		lightGroup_ = new QButtonGroup(this);
		lightGroup_->setExclusive(false); // 上下光源可独立开关
		lightGroup_->addButton(ui->rbtn_upLight);
		lightGroup_->addButton(ui->rbtn_downLight);

		buildConnections();
		setupImageView();

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
		connect(&app_, &app::PunchPressApp::plcConnectionChanged,
			this, &PunchPress::onPlcConnectionChanged, Qt::QueuedConnection);
		connect(&app_, &app::PunchPressApp::startupCheckFailed,
			this, &PunchPress::onStartupCheckFailed, Qt::QueuedConnection);

		// UI 控件 → 槽
		connect(ui->rbtn_debug, &QRadioButton::clicked, this, &PunchPress::onDebugClicked);
		connect(ui->rbtn_work, &QRadioButton::clicked, this, &PunchPress::onWorkClicked);
		connect(ui->rbtn_upLight, &QRadioButton::clicked, this, &PunchPress::onUpperLightClicked);
		connect(ui->rbtn_downLight, &QRadioButton::clicked, this, &PunchPress::onLowerLightClicked);
		connect(ui->pbtn_modelManager, &QPushButton::clicked, this, &PunchPress::onModelManager);
		connect(ui->pbtn_exit, &QPushButton::clicked, this, &PunchPress::onExit);
	}

	void PunchPress::setupImageView()
	{
		// 用 HalconDisplayLabel 替换 ui 中的占位 QLabel
		imageView_ = new HalconInteractiveLabel(this);

		auto* oldLabel = ui->label_imgDisplay;
		if (auto* parentWidget = oldLabel->parentWidget())
		{
			if (auto* layout = parentWidget->layout())
			{
				layout->replaceWidget(oldLabel, imageView_);
			}
		}
		oldLabel->deleteLater();
	}

	void PunchPress::showEvent(QShowEvent* e)
	{
		QMainWindow::showEvent(e);
		// HalconDisplayLabel 在首次 showEvent 时自动懒创建 Halcon 窗口
	}

	void PunchPress::resizeEvent(QResizeEvent* e)
	{
		QMainWindow::resizeEvent(e);
		// HalconDisplayLabel 在自有 resizeEvent 中自动同步尺寸
	}

	// ===== 槽实现 =====

	void PunchPress::onDebugClicked()
	{
		if (ui->rbtn_debug->isChecked())
		{
			ui->rbtn_work->setChecked(false);
			QString err;
			if (!app_.switchToMode(global::RunMode::Debug, &err))
			{
				if (!err.isEmpty())
					rw::rqwu::MessageBox::warning(this, QStringLiteral("调试模式"), err);
				// 切换失败时回到 Idle/停止模式，停止取流
				app_.switchToMode(global::RunMode::Idle);
			}
		}
		else
		{
			// 再次点击取消选中 → 回到 Idle/停止模式
			app_.switchToMode(global::RunMode::Idle);
		}
	}

	void PunchPress::onWorkClicked()
	{
		if (ui->rbtn_work->isChecked())
		{
			ui->rbtn_debug->setChecked(false);
			QString err;
			if (!app_.switchToMode(global::RunMode::Production, &err))
			{
				if (!err.isEmpty())
					rw::rqwu::MessageBox::warning(this, QStringLiteral("工作模式"), err);
				// 切换失败时回到 Idle/停止模式，停止取流
				app_.switchToMode(global::RunMode::Idle);
			}
		}
		else
		{
			// 再次点击取消选中 → 回到 Idle/停止模式
			app_.switchToMode(global::RunMode::Idle);
		}
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

	void PunchPress::onFrameReady(HalconCpp::HImage image)
	{
		if (imageView_)
			imageView_->displayImage(image);
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

	void PunchPress::onModeChanged(global::RunMode mode)
	{
		ui->rbtn_debug->setChecked(mode == global::RunMode::Debug);
		ui->rbtn_work->setChecked(mode == global::RunMode::Production);
	}

	void PunchPress::onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
	{
		QLabel* label = nullptr;
		QString camName;
		if (idx == global::CameraIndex::Camera1)
		{
			label = ui->label_camera1State;
			camName = QStringLiteral("相机1");
		}
		else
		{
			label = ui->label_camera2State;
			camName = QStringLiteral("相机2");
		}

		if (!label)
			return;

		if (connected)
		{
			label->setStyleSheet(QStringLiteral(
				"color: #4CAF50;"
				"font-size: 14px;"
				"font-weight: bold;"));
			label->setText(camName + QStringLiteral(" ● 已连接"));
		}
		else
		{
			label->setStyleSheet(QStringLiteral(
				"color: #F44336;"
				"font-size: 14px;"
				"font-weight: bold;"));
			label->setText(camName + QStringLiteral(" ● 断开: ") + reason);
		}
	}

	void PunchPress::onPlcConnectionChanged(bool connected)
	{
		if (connected)
		{
			ui->label_PlcState->setStyleSheet(QStringLiteral(
				"color: #4CAF50;"
				"font-size: 14px;"
				"font-weight: bold;"));
			ui->label_PlcState->setText(QStringLiteral("PLC ● 已连接"));
		}
		else
		{
			ui->label_PlcState->setStyleSheet(QStringLiteral(
				"color: #F44336;"
				"font-size: 14px;"
				"font-weight: bold;"));
			ui->label_PlcState->setText(QStringLiteral("PLC ● 断开"));
		}
	}

	void PunchPress::onStartupCheckFailed(const QString& reason)
	{
		rw::rqwu::MessageBox::critical(this, QStringLiteral("启动检查失败"), reason);
	}
}
