// 必须最先包含：rqwu_MessageBox.h 要在 windows.h(经 Halcon 引入) 定义
// MessageBox 宏之前解析，否则类名会被宏改写为 MessageBoxW。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

#include "UI/PunchPress.h"
#include "ui_PunchPress.h"

#include <QButtonGroup>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QShowEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "UI/HalconInteractiveLabel.h"
#include "UI/ModelManagerDialog.h"
#include "UI/OffsetEditorDialog.h"

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

		// === 右侧栏：已加载模型表格 ===
		loadedModelsGroup_ = new QGroupBox(QStringLiteral("已加载模型"), this);
		loadedModelsGroup_->setStyleSheet(QStringLiteral(
			"QGroupBox {"
			"  font-size: 20px;"
			"  font-weight: bold;"
			"  background-color: rgb(181, 181, 181);"
			"  border: 1px solid #e0e0e0;"
			"  border-radius: 15px;"
			"  padding-top: 28px;"
			"}"
			"QGroupBox::title {"
			"  subcontrol-origin: margin;"
			"  subcontrol-position: top left;"
			"  left: 10px;"
			"  top: 4px;"
			"  padding: 0 5px;"
			"  color: #2c3e50;"
			"}"));

		auto* groupLayout = new QVBoxLayout(loadedModelsGroup_);

		loadedModelsTable_ = new QTableWidget(0, 8, this);
		loadedModelsTable_->setStyleSheet(QStringLiteral(
			"QTableWidget {"
			"  font-size: 16px;"
			"  color: #444;"
			"  background-color: white;"
			"  border: 1px solid #DDD;"
			"  border-radius: 4px;"
			"  gridline-color: #EEE;"
			"}"
			"QTableWidget::item {"
			"  padding: 6px 10px;"
			"}"
			"QHeaderView::section {"
			"  background-color: #F5F5F5;"
			"  border: 1px solid #DDD;"
			"  padding: 6px 10px;"
			"  font-size: 14px;"
			"  font-weight: bold;"
			"  color: #555;"
			"}"));
		loadedModelsTable_->setMaximumHeight(200);
		loadedModelsTable_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		loadedModelsTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		loadedModelsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
		loadedModelsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
		loadedModelsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
		loadedModelsTable_->setFocusPolicy(Qt::NoFocus);
		loadedModelsTable_->verticalHeader()->setVisible(false);

		// 表头
		QStringList headers;
		headers << QStringLiteral("模型名称")
			<< QStringLiteral("X方向偏移")
			<< QStringLiteral("Y方向偏移")
			<< QStringLiteral("角度偏移")
			<< QStringLiteral("查找数量")
			<< QStringLiteral("最低分数")
			<< QStringLiteral("最低角度°")
			<< QStringLiteral("最高角度°");
		loadedModelsTable_->setHorizontalHeaderLabels(headers);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setMinimumSectionSize(60);  // 防止内容列过窄
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
		loadedModelsTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

		// 双击行 → 编辑参数
		connect(loadedModelsTable_, &QTableWidget::cellDoubleClicked,
			this, &PunchPress::openOffsetEditor);

		// 右键菜单 → 卸载模型
		loadedModelsTable_->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(loadedModelsTable_, &QTableWidget::customContextMenuRequested,
			this, &PunchPress::onTableContextMenu);

		groupLayout->addWidget(loadedModelsTable_);

		// 插入到右侧控制栏：gBox_tools 之后、gBox_table 之前（索引 3）
		ui->vLayout_control->insertWidget(3, loadedModelsGroup_);
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
		setupInfoTable();
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
		connect(&app_, &app::PunchPressApp::allPositionResultsReady,
			this, &PunchPress::onAllPositionResults, Qt::QueuedConnection);
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

		// 模型加载/卸载 → 刷新右侧栏列表
		auto& bun = app_.business().shape_mode_manager_bun;
		if (bun)
		{
			connect(bun.get(), &bun::ShapeModeManagerBun::modelsLoaded,
				this, &PunchPress::refreshLoadedModelsList);
			connect(bun.get(), &bun::ShapeModeManagerBun::modelsUnloaded,
				this, &PunchPress::refreshLoadedModelsList);
			connect(bun.get(), &bun::ShapeModeManagerBun::modelOffsetChanged,
				this, &PunchPress::refreshLoadedModelsList);
			connect(bun.get(), &bun::ShapeModeManagerBun::matchParamsChanged,
				this, &PunchPress::refreshLoadedModelsList);
			connect(bun.get(), &bun::ShapeModeManagerBun::modelListChanged,
				this, &PunchPress::refreshLoadedModelsList);
		}

		// 初始化列表内容（模型可能在 build 之前已加载）
		refreshLoadedModelsList();
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

	void PunchPress::onAllPositionResults(std::vector<global::PositionResult> results)
	{
		auto* table = ui->tableWidget_info;
		if (!table) return;

		table->setUpdatesEnabled(false);

		// 每个有效结果在表格顶部插入一行（最新数据置顶）
		for (auto it = results.rbegin(); it != results.rend(); ++it)
		{
			if (!it->valid) continue;

			table->insertRow(0);

			// 模型名称
			auto* nameItem = new QTableWidgetItem(
				QString::fromStdString(it->modelName));
			nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(0, 0, nameItem);

			// X (mm)
			auto* xItem = new QTableWidgetItem(
				QString::number(it->offsetX, 'f', 3));
			xItem->setFlags(xItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(0, 1, xItem);

			// Y (mm)
			auto* yItem = new QTableWidgetItem(
				QString::number(it->offsetY, 'f', 3));
			yItem->setFlags(yItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(0, 2, yItem);

			// 角度 (°)
			auto* aItem = new QTableWidgetItem(
				QString::number(it->angle, 'f', 3));
			aItem->setFlags(aItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(0, 3, aItem);

			// 分数
			auto* sItem = new QTableWidgetItem(
				QString::number(it->score, 'f', 3));
			sItem->setFlags(sItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(0, 4, sItem);
		}

		// 限制最大行数，超出时删除旧行
		constexpr int kMaxRows = 200;
		while (table->rowCount() > kMaxRows)
			table->removeRow(table->rowCount() - 1);

		table->resizeColumnsToContents();
		table->setUpdatesEnabled(true);
	}

	void PunchPress::setupInfoTable()
	{
		auto* table = ui->tableWidget_info;
		if (!table) return;

		table->setColumnCount(5);
		table->setHorizontalHeaderLabels({
			QStringLiteral("模型"),
			QStringLiteral("X(mm)"),
			QStringLiteral("Y(mm)"),
			QStringLiteral("角度(°)"),
			QStringLiteral("分数")
		});
		table->horizontalHeader()->setVisible(true);
		table->verticalHeader()->setVisible(false);
		table->setEditTriggers(QAbstractItemView::NoEditTriggers);
		table->setSelectionMode(QAbstractItemView::NoSelection);
		table->setFocusPolicy(Qt::NoFocus);
		table->setAlternatingRowColors(true);
		table->horizontalHeader()->setStretchLastSection(true);
	}

	void PunchPress::onModeChanged(global::RunMode mode)
	{
		ui->rbtn_debug->setChecked(mode == global::RunMode::Debug);
		ui->rbtn_work->setChecked(mode == global::RunMode::Production);
		// CreateModel / Idle / 标定模式均不选中任何模式按钮
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

	void PunchPress::refreshLoadedModelsList()
	{
		if (!loadedModelsTable_)
			return;

		// 断开双击信号避免刷新时触发
		disconnect(loadedModelsTable_, &QTableWidget::cellDoubleClicked,
			this, &PunchPress::openOffsetEditor);

		loadedModelsTable_->setRowCount(0);

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun || !bun->isModelLoaded())
		{
			connect(loadedModelsTable_, &QTableWidget::cellDoubleClicked,
				this, &PunchPress::openOffsetEditor);
			return;
		}

		const auto ids = bun->getLoadedModelIds();
		const auto allModels = bun->getAllModels();

		loadedModelsTable_->setRowCount(static_cast<int>(ids.size()));

		for (int row = 0; row < static_cast<int>(ids.size()); ++row)
		{
			const auto& id = ids[row];

			// 查找模型名称
			QString name = QString::fromStdString(id);
			for (const auto& info : allModels)
			{
				if (info.getId() == id)
				{
					name = QString::fromStdString(info.base_info.name);
					break;
				}
			}

			// 读取偏移量
			auto offset = bun->getUserOffset(id);

			// 读取匹配参数
			auto matchParams = bun->getMatchParams(id);
			constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
			const double minAngleDeg = matchParams.angleStart * kRadToDeg;
			const double maxAngleDeg = (matchParams.angleStart + matchParams.angleExtent) * kRadToDeg;

			// 模型名称（蓝色加粗）
			auto* nameItem = new QTableWidgetItem(QStringLiteral("● ") + name);
			nameItem->setForeground(QColor(0x2196F3));
			QFont boldFont;
			boldFont.setBold(true);
			nameItem->setFont(boldFont);
			nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
			// 将模型 ID 存入 UserRole，供编辑对话框使用
			nameItem->setData(Qt::UserRole, QString::fromStdString(id));
			loadedModelsTable_->setItem(row, 0, nameItem);

			// OffsetX
			auto* xItem = new QTableWidgetItem(QString::number(offset.offsetX, 'f', 3));
			xItem->setTextAlignment(Qt::AlignCenter);
			xItem->setFlags(xItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 1, xItem);

			// OffsetY
			auto* yItem = new QTableWidgetItem(QString::number(offset.offsetY, 'f', 3));
			yItem->setTextAlignment(Qt::AlignCenter);
			yItem->setFlags(yItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 2, yItem);

			// OffsetAngle
			auto* aItem = new QTableWidgetItem(QString::number(offset.offsetAngle, 'f', 3));
			aItem->setTextAlignment(Qt::AlignCenter);
			aItem->setFlags(aItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 3, aItem);

			// 查找数量（整数）
			auto* numItem = new QTableWidgetItem(QString::number(matchParams.numMatches));
			numItem->setTextAlignment(Qt::AlignCenter);
			numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 4, numItem);

			// 最低分数（0.00 ~ 1.00）
			auto* scoreItem = new QTableWidgetItem(QString::number(matchParams.minScore, 'f', 2));
			scoreItem->setTextAlignment(Qt::AlignCenter);
			scoreItem->setFlags(scoreItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 5, scoreItem);

			// 最低角度（度）
			auto* minAngleItem = new QTableWidgetItem(QString::number(minAngleDeg, 'f', 1));
			minAngleItem->setTextAlignment(Qt::AlignCenter);
			minAngleItem->setFlags(minAngleItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 6, minAngleItem);

			// 最高角度（度）
			auto* maxAngleItem = new QTableWidgetItem(QString::number(maxAngleDeg, 'f', 1));
			maxAngleItem->setTextAlignment(Qt::AlignCenter);
			maxAngleItem->setFlags(maxAngleItem->flags() & ~Qt::ItemIsEditable);
			loadedModelsTable_->setItem(row, 7, maxAngleItem);
		}

		// 恢复双击连接
		connect(loadedModelsTable_, &QTableWidget::cellDoubleClicked,
			this, &PunchPress::openOffsetEditor);
	}

	void PunchPress::openOffsetEditor(int row, int /*column*/)
	{
		if (!loadedModelsTable_ || row < 0 || row >= loadedModelsTable_->rowCount())
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		const auto ids = bun->getLoadedModelIds();
		if (row >= static_cast<int>(ids.size()))
			return;

		const auto& id = ids[row];

		// 从表格读取模型名称
		auto* nameItem = loadedModelsTable_->item(row, 0);
		const QString modelName = nameItem
			? nameItem->text().mid(2)  // 去掉 "● " 前缀
			: QString::fromStdString(id);

		// 读取当前偏移量
		auto current = bun->getUserOffset(id);

		// 读取当前匹配参数（角度制，直接供 UI 显示）
		auto matchParams = bun->getMatchParams(id);
		const double minAngleDeg = matchParams.angleStart;
		const double maxAngleDeg = matchParams.angleStart + matchParams.angleExtent;

		// 弹出编辑对话框
		OffsetEditorDialog dlg(modelName,
			current.offsetX, current.offsetY, current.offsetAngle,
			matchParams.numMatches, matchParams.minScore,
			minAngleDeg, maxAngleDeg,
			this);
		if (dlg.exec() != QDialog::Accepted)
			return;

		// 保存偏移量
		std::string err;
		if (!bun->setUserOffset(id,
			dlg.offsetX(), dlg.offsetY(), dlg.offsetAngle(),
			&err))
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("保存偏移量失败"),
				QString::fromStdString(err));
		}

		// 保存匹配参数（角度制，直接传递）
		const double angleStartDeg = dlg.minAngleDeg();
		const double angleExtentDeg = dlg.maxAngleDeg() - dlg.minAngleDeg();
		if (!bun->setMatchParams(id,
			dlg.numMatches(), dlg.minScore(),
			angleStartDeg, angleExtentDeg,
			&err))
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("保存匹配参数失败"),
				QString::fromStdString(err));
		}
		// 表格刷新由 modelOffsetChanged / matchParamsChanged 信号触发
	}

	void PunchPress::onTableContextMenu(const QPoint& pos)
	{
		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		// 获取右键点击的行
		const QModelIndex index = loadedModelsTable_->indexAt(pos);
		if (!index.isValid())
			return;

		const int row = index.row();
		auto* nameItem = loadedModelsTable_->item(row, 0);
		if (!nameItem)
			return;

		const QString modelId = nameItem->data(Qt::UserRole).toString();
		if (modelId.isEmpty())
			return;

		const QString modelName = nameItem->text().mid(2);  // 去掉 "● " 前缀

		QMenu menu(this);
		menu.setStyleSheet(QStringLiteral(
			"QMenu { font-size: 16px; padding: 4px; }"
			"QMenu::item { padding: 8px 24px; }"
			"QMenu::item:selected { background-color: #E3F2FD; color: #1976D2; }"));

		QAction* editAction = menu.addAction(QStringLiteral("设置参数"));
		connect(editAction, &QAction::triggered, this, [this, row]()
		{
			openOffsetEditor(row, 0);
		});

		menu.addSeparator();

		QAction* unloadAction = menu.addAction(QStringLiteral("卸载 \"%1\"").arg(modelName));
		auto* bunPtr = bun.get();
		connect(unloadAction, &QAction::triggered, this, [this, bunPtr, modelId]()
		{
			bunPtr->unloadModel(modelId.toStdString());
			// 表格刷新由 modelListChanged / modelsUnloaded 信号自动触发
		});

		menu.exec(loadedModelsTable_->viewport()->mapToGlobal(pos));
	}
}
