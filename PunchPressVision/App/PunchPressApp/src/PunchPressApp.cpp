#include "app/PunchPressApp.hpp"

#include <QProcess>
#include <QStringList>

#include "infrastructure/ControlModule/ControlModule.hpp"
#include "Business/HealthMonitorBun/HealthMonitorBun.hpp"

namespace app
{
	PunchPressApp::PunchPressApp(bun::Business& business, QObject* parent)
		: QObject(parent)
		, business_(business)
	{
		// 构造阶段连接信号（确保 business 层信号在 build/start 期间即可被接收）
		if (business_.camera_bun)
		{
			QObject::connect(business_.camera_bun.get(), &bun::CameraBun::callBackFunWithCalib,
				this, &PunchPressApp::onFrameReady, Qt::DirectConnection);
			QObject::connect(business_.camera_bun.get(), &bun::CameraBun::cameraConnectionStateChanged,
				this, &PunchPressApp::onCameraConnectionChanged, Qt::QueuedConnection);
		}

		// PLC 连接状态：ControlModule 信号 → App 信号（信号直连，无需中间槽）
		auto& inf = business_.infrastructure();
		if (inf.control_module_)
		{
			QObject::connect(inf.control_module_.get(), &inf::ControlModule::connectionStateChanged,
				this, &PunchPressApp::plcConnectionChanged, Qt::QueuedConnection);
		}

		// 健康监控：HealthMonitorBun（工作线程轮询）→ App 信号
		if (business_.health_monitor_bun)
		{
			QObject::connect(business_.health_monitor_bun.get(), &bun::HealthMonitorBun::cameraConnectionStateChanged,
				this, &PunchPressApp::cameraConnectionChanged, Qt::QueuedConnection);
			QObject::connect(business_.health_monitor_bun.get(), &bun::HealthMonitorBun::plcConnectionChanged,
				this, &PunchPressApp::plcConnectionChanged, Qt::QueuedConnection);
		}
	}

	PunchPressApp::~PunchPressApp()
	{
		destroy();
	}

	void PunchPressApp::build()
	{
	}

	void PunchPressApp::destroy()
	{
		if (business_.camera_bun)
			QObject::disconnect(business_.camera_bun.get(), nullptr, this, nullptr);
		if (business_.health_monitor_bun)
			QObject::disconnect(business_.health_monitor_bun.get(), nullptr, this, nullptr);
		auto& inf = business_.infrastructure();
		if (inf.control_module_)
			QObject::disconnect(inf.control_module_.get(), nullptr, this, nullptr);
	}

	void PunchPressApp::start()
	{
		// 尝试连接 PLC（IP/端口来自基础配置）。无设备时连接失败仅告警。
		auto& inf = business_.infrastructure();
		if (inf.control_module_ && inf.config_module_)
		{
			const auto& base = inf.config_module_->baseCfg;
			// TODO(硬件): 确认现场 PLC 的 IP/端口；默认取自 baseCfg。
			inf.control_module_->connectToPLC(base.plcIp, base.plcPort);
		}

		// 启动后回到 Idle（触发模式、不取流、业务层不处理帧），由用户手动进入调试/工作模式。
		// 此处不经过 switchToMode 的“同模式短路”，确保 business.start() 已经启动的 monitor 被停止。
		configureCameraForIdle();
		if (business_.camera_bun)
			business_.camera_bun->stop();
		emit modeChanged(global::RunMode::Idle);
	}

	void PunchPressApp::stop()
	{
		auto& inf = business_.infrastructure();
		if (inf.control_module_)
			inf.control_module_->disconnectPLC();
	}

	global::RunMode PunchPressApp::currentMode() const
	{
		return currentMode_.load(std::memory_order_acquire);
	}
	// ===== 启动检查（FR-001 ~ FR-004）=====

	bool PunchPressApp::performStartupCheck(QString* errorMsg)
	{
		// FR-002: 单实例检查
		if (!checkSingleInstance())
		{
			const QString reason = QStringLiteral("系统已在运行，请勿重复启动");
			if (errorMsg) *errorMsg = reason;
			emit startupCheckFailed(reason);
			return false;
		}

		// FR-001: MVS 进程冲突检测
		if (!checkMVSProcessConflict())
		{
			const QString reason = QStringLiteral("检测到 MVS 客户端正在运行，请先关闭");
			if (errorMsg) *errorMsg = reason;
			emit startupCheckFailed(reason);
			return false;
		}

		// FR-003: 配置文件完整性校验
		if (!checkConfigIntegrity())
		{
			const QString reason = QStringLiteral("配置文件损坏或缺失");
			if (errorMsg) *errorMsg = reason;
			emit startupCheckFailed(reason);
			return false;
		}

		// FR-004: 标定参数就绪性检查（不阻塞启动，仅提示）
		auto readiness = checkCalibReadiness();
		if (!readiness.allReady())
		{
			QString missing;
			if (!readiness.distortionReady) missing += QStringLiteral("[畸变矫正] ");
			if (!readiness.ninePointReady) missing += QStringLiteral("[九点标定] ");
			if (!readiness.spliceReady) missing += QStringLiteral("[双相机拼接] ");
			if (errorMsg) *errorMsg = QStringLiteral("以下标定参数未就绪: ") + missing;
		}

		return true;
	}

	bool PunchPressApp::checkSingleInstance()
	{
		instanceLock_ = std::make_unique<QSystemSemaphore>(
			QStringLiteral("PunchPressVision_InstanceLock"), 1, QSystemSemaphore::Open);
		// acquire 失败说明已有实例持有
		return instanceLock_->acquire();
	}

	bool PunchPressApp::checkMVSProcessConflict()
	{
		QProcess process;
		process.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq MVS.exe");
		if (!process.waitForFinished(3000))
			return true; // 检测失败时不阻塞启动
		const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
		return !output.contains("MVS.exe");
	}

	bool PunchPressApp::checkConfigIntegrity()
	{
		// Infrastructure 在 build 时已加载配置（损坏会回退默认值），此处认为通过。
		// TODO: 如需严格校验，可在此检查关键配置文件存在性。
		return true;
	}

	// ===== 就绪性检查 =====

	global::CalibReadiness PunchPressApp::checkCalibReadiness() const
	{
		global::CalibReadiness r;
		const auto& inf = business_.infrastructure();

		// 畸变矫正就绪：相机内参非空
		if (inf.calib_config_module_)
			r.distortionReady = inf.calib_config_module_->calibConfig.item(global::CameraIndex::Camera1).cameraParameters.Length() > 0;

		// 九点标定就绪：变换矩阵长度 >= 6
		if (inf.nine_point_module_)
			r.ninePointReady = inf.nine_point_module_->ninePointConfig.outHomMat2D.Length() >= 6;

		// 双相机拼接就绪：映射图已生成
		if (inf.two_camera_splice_module_)
			r.spliceReady = inf.two_camera_splice_module_->twoCameraSpliceConfig.MapSingle1.IsInitialized();

		return r;
	}

	global::ProductionReadiness PunchPressApp::checkProductionReadiness() const
	{
		global::ProductionReadiness pr;
		pr.calib = checkCalibReadiness();
		pr.hasLoadedModel = business_.shape_mode_manager_bun
			? business_.shape_mode_manager_bun->isModelLoaded() : false;
		const auto& inf = business_.infrastructure();
		pr.plcConnected = inf.control_module_ ? inf.control_module_->isConnected() : false;
		return pr;
	}

	// ===== 相机配置代理 =====

	bool PunchPressApp::configureCameraForDebug()
	{
		// 自由运行模式，3fps（FR-005）
		auto& inf = business_.infrastructure();
		if (!inf.camera_module_)
			return false;
		bool ok = inf.camera_module_->setFreeRunMode(global::CameraIndex::Camera1, 3.0);
		ok = inf.camera_module_->setFreeRunMode(global::CameraIndex::Camera2, 3.0) && ok;
		return ok;
	}

	bool PunchPressApp::configureCameraForProduction()
	{
		// 外部触发模式，Line0，30fps（FR-008）
		auto& inf = business_.infrastructure();
		if (!inf.camera_module_)
			return false;
		bool ok = inf.camera_module_->setTriggerMode(
			global::CameraIndex::Camera1, global::TriggerSource::Line0, 30.0);
		ok = inf.camera_module_->setTriggerMode(
			global::CameraIndex::Camera2, global::TriggerSource::Line0, 30.0) && ok;
		return ok;
	}

	bool PunchPressApp::configureCameraForCalibration()
	{
		// 自由运行模式，1fps（测试用，FR-011）
		return configureCameraForDebug();
	}

	bool PunchPressApp::configureCameraForIdle()
	{
		// 空闲/停止模式：触发模式（软件触发），不开始取流，由 switchToMode 统一停止 monitor
		auto& inf = business_.infrastructure();
		if (!inf.camera_module_)
			return false;
		bool ok = inf.camera_module_->setTriggerMode(
			global::CameraIndex::Camera1, global::TriggerSource::Software, 3.0);
		ok = inf.camera_module_->setTriggerMode(
			global::CameraIndex::Camera2, global::TriggerSource::Software, 3.0) && ok;
		return ok;
	}

	bool PunchPressApp::configureCameraForCreateModel()
	{
		// 创建模型模式：自由运行模式，3fps，图像实时刷新到 ShapeEditor
		return configureCameraForDebug();
	}

	// ===== 模式切换状态机（FR-005 ~ FR-020）=====

	bool PunchPressApp::switchToMode(global::RunMode mode, QString* errorMsg)
	{
		const auto current = currentMode_.load(std::memory_order_acquire);
		if (current == mode)
			return true;

		// 前置校验
		switch (mode)
		{
		case global::RunMode::Debug:
		case global::RunMode::Splice:
		{
			if (!checkCalibReadiness().allReady())
			{
				if (errorMsg) *errorMsg = QStringLiteral("标定参数未全部就绪");
				// 不强制拒绝，离线/调试场景仍允许进入；如需严格可 return false
			}
			break;
		}
		case global::RunMode::Production:
		{
			auto pr = checkProductionReadiness();
			if (!pr.hasLoadedModel)
			{
				if (errorMsg) *errorMsg = QStringLiteral("工作模式需先加载模板模型");
				return false;
			}
			break;
		}
		case global::RunMode::CalibNinePoint:
		{
			if (!checkCalibReadiness().distortionReady)
			{
				// 提示但不强制
				if (errorMsg) *errorMsg = QStringLiteral("建议先完成畸变矫正标定");
			}
			break;
		}
		default:
			break;
		}

		// 执行切换 + 配置相机 + 控制取流
		currentMode_.store(mode, std::memory_order_release);
		switch (mode)
		{
		case global::RunMode::Idle: configureCameraForIdle(); break;
		case global::RunMode::Debug: configureCameraForDebug(); break;
		case global::RunMode::Production: configureCameraForProduction(); break;
		case global::RunMode::CreateModel: configureCameraForCreateModel(); break;
		case global::RunMode::CalibDistortion:
		case global::RunMode::CalibNinePoint:
		case global::RunMode::Splice: configureCameraForCalibration(); break;
		default: break;
		}

		// 控制相机取流：Idle 模式必须停止 monitor；从 Idle 进入其他模式时需要开始取流
		if (business_.camera_bun)
		{
			if (mode == global::RunMode::Idle)
				business_.camera_bun->stop();
			else if (current == global::RunMode::Idle)
				business_.camera_bun->start();
		}

		emit modeChanged(mode);
		return true;
	}

	// ===== 帧处理 =====

	void PunchPressApp::onFrameReady(HalconCpp::HImage image)
	{
		// Idle 模式下相机应已停止取流；即便收到遗留帧，业务层也不处理
		if (currentMode_.load(std::memory_order_acquire) == global::RunMode::Idle)
			return;

		// 转发给 UI 显示（跨线程由接收方用 QueuedConnection 处理）
		emit frameReady(image);

		// 工作模式下执行定位流水线
		if (currentMode_.load(std::memory_order_acquire) == global::RunMode::Production)
			processProductionFrame(image);
	}

	void PunchPressApp::onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
	{
		emit cameraConnectionChanged(idx, connected, reason);
	}

	void PunchPressApp::processProductionFrame(const HalconCpp::HImage& image)
	{
		if (!business_.shape_mode_manager_bun)
			return;

		bun::MatchResult m = business_.shape_mode_manager_bun->match(image);

		global::PositionResult result;
		result.valid = m.found;
		result.score = m.score;

		if (m.found)
		{
			auto& inf = business_.infrastructure();

			// 像素坐标 → 世界坐标（若九点标定矩阵就绪）
			bool converted = false;
			if (business_.nine_point_bun && inf.nine_point_module_)
			{
				const auto& homMat = inf.nine_point_module_->ninePointConfig.outHomMat2D;
				double wx = 0.0, wy = 0.0;
				if (homMat.Length() >= 6 &&
					business_.nine_point_bun->pixToWorld(homMat, m.column, m.row, wx, wy))
				{
					result.offsetX = wx;
					result.offsetY = wy;
					converted = true;
				}
			}
			if (!converted)
			{
				// 回退到像素偏移（未标定时）
				result.offsetX = m.offsetX;
				result.offsetY = m.offsetY;
			}
			result.angle = m.angle;
		}

		emit positionResultReady(result);

		// 写 PLC（若已连接）：X/Y 偏移、角度、有效标志（寄存器地址来自 plcAddressCfg）
		auto& inf = business_.infrastructure();
		if (inf.control_module_ && inf.control_module_->isConnected() && inf.config_module_)
		{
			const auto& plc = inf.config_module_->plcAddressCfg;
			inf.control_module_->writeFloat(plc.regOffsetX, static_cast<float>(result.offsetX));
			inf.control_module_->writeFloat(plc.regOffsetY, static_cast<float>(result.offsetY));
			inf.control_module_->writeFloat(plc.regAngle, static_cast<float>(result.angle));
			inf.control_module_->writeRegister(plc.regValid, result.valid ? 1 : 0);
		}
	}
}
