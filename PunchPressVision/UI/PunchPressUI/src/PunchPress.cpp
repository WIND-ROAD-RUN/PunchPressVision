// 必须最先包含：rqwu_MessageBox.h 要在 windows.h(经 Halcon 引入) 定义
// MessageBox 宏之前解析，否则类名会被宏改写为 MessageBoxW。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

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

		setupImageView();
	}

	PunchPress::~PunchPress()
	{
		delete ui;
	}

	void PunchPress::build()
	{
		// 连接信号、读取已加载配置、执行启动检查。
		// 必须在 infrastructure/business/app build 之后调用。
		buildConnections();
		loadConfigs();
		updateCameraParamButtons();

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

	void PunchPress::destroy()
	{
		// 断开与 App 层的所有信号连接，避免 destroy 阶段收到回调
		QObject::disconnect(&app_, nullptr, this, nullptr);
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
		connect(ui->pbtn_exposure1, &QPushButton::clicked, this, &PunchPress::onExposure1Clicked);
		connect(ui->pbtn_gain1, &QPushButton::clicked, this, &PunchPress::onGain1Clicked);
		connect(ui->pbtn_exposure2, &QPushButton::clicked, this, &PunchPress::onExposure2Clicked);
		connect(ui->pbtn_gain2, &QPushButton::clicked, this, &PunchPress::onGain2Clicked);
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

	void PunchPress::loadConfigs()
	{
		// UI 层配置读取入口：后续可在此扩展光源、PLC 地址、模型参数等配置的读取
		loadCameraConfig();
	}

	void PunchPress::loadCameraConfig()
	{
		const auto& inf = app_.business().infrastructure();
		if (!inf.camera_module_)
			return;

		cameraCfg_ = inf.camera_module_->cameraCfg;
	}

	void PunchPress::updateCameraParamButtons()
	{
		// 用已加载的相机配置刷新主界面曝光/增益显示
		ui->pbtn_exposure1->setText(QString::number(cameraCfg_.exposureTime1));
		ui->pbtn_gain1->setText(QString::number(cameraCfg_.gain1));
		ui->pbtn_exposure2->setText(QString::number(cameraCfg_.exposureTime2));
		ui->pbtn_gain2->setText(QString::number(cameraCfg_.gain2));
	}

	bool PunchPress::inputIntegerParam(QPushButton* button, int& value, int min, int max)
	{
		QString input = QString::number(value);

		rw::rqwu::NumberKeyboard::InputDataConfig cfg;
		cfg.isUsingMin = true;
		cfg.isUsingMax = true;
		cfg.min = static_cast<double>(min);
		cfg.max = static_cast<double>(max);

		const auto result = rw::rqwu::NumberKeyboard::inputDataOnQPushButton(button, input, cfg);
		if (result != rw::rqwu::NumberKeyboard::Accept)
			return false;

		bool ok = false;
		const int newValue = input.toInt(&ok);
		if (!ok)
			return false;

		value = newValue;
		button->setText(QString::number(newValue));
		return true;
	}

	void PunchPress::applyExposure(global::CameraIndex idx, int value, int Config::cameraCfg::* member)
	{
		cameraCfg_.*member = value;

		auto& inf = app_.business().infrastructure();
		if (inf.camera_module_)
		{
			inf.camera_module_->cameraCfg.*member = value;
			if (!inf.camera_module_->setExposure(idx, static_cast<double>(value)))
			{
				rw::rqwu::MessageBox::warning(this, QStringLiteral("设置失败"),
					QStringLiteral("无法设置 %1 曝光").arg(idx == global::CameraIndex::Camera1
						? QStringLiteral("相机1") : QStringLiteral("相机2")));
			}
		}

		if (inf.config_module_)
		{
			inf.config_module_->cameraCfg.*member = value;
			inf.config_module_->save();
		}
	}

	void PunchPress::applyGain(global::CameraIndex idx, int value, int Config::cameraCfg::* member)
	{
		cameraCfg_.*member = value;

		auto& inf = app_.business().infrastructure();
		if (inf.camera_module_)
		{
			inf.camera_module_->cameraCfg.*member = value;
			if (!inf.camera_module_->setGain(idx, static_cast<double>(value)))
			{
				rw::rqwu::MessageBox::warning(this, QStringLiteral("设置失败"),
					QStringLiteral("无法设置 %1 增益").arg(idx == global::CameraIndex::Camera1
						? QStringLiteral("相机1") : QStringLiteral("相机2")));
			}
		}

		if (inf.config_module_)
		{
			inf.config_module_->cameraCfg.*member = value;
			inf.config_module_->save();
		}
	}

	void PunchPress::onExposure1Clicked()
	{
		if (!inputIntegerParam(ui->pbtn_exposure1, cameraCfg_.exposureTime1, 1, 10000000))
			return;
		applyExposure(global::CameraIndex::Camera1, cameraCfg_.exposureTime1,
			&Config::cameraCfg::exposureTime1);
	}

	void PunchPress::onGain1Clicked()
	{
		if (!inputIntegerParam(ui->pbtn_gain1, cameraCfg_.gain1, 0, 100))
			return;
		applyGain(global::CameraIndex::Camera1, cameraCfg_.gain1, &Config::cameraCfg::gain1);
	}

	void PunchPress::onExposure2Clicked()
	{
		if (!inputIntegerParam(ui->pbtn_exposure2, cameraCfg_.exposureTime2, 1, 10000000))
			return;
		applyExposure(global::CameraIndex::Camera2, cameraCfg_.exposureTime2,
			&Config::cameraCfg::exposureTime2);
	}

	void PunchPress::onGain2Clicked()
	{
		if (!inputIntegerParam(ui->pbtn_gain2, cameraCfg_.gain2, 0, 100))
			return;
		applyGain(global::CameraIndex::Camera2, cameraCfg_.gain2, &Config::cameraCfg::gain2);
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
