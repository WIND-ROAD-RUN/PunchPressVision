#include "app/PunchPressApp.hpp"

#include <algorithm>
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

		// 启动后默认进入工作模式
		switchToMode(global::RunMode::Production);
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
		if (business_.shape_mode_manager_bun)
		{
			pr.hasLoadedModel = business_.shape_mode_manager_bun->isModelLoaded();
			pr.loadedModelCount = business_.shape_mode_manager_bun->getLoadedModelCount();
		}
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

	// ===== 模式切换状态机（FR-005 ~ FR-010）=====

	bool PunchPressApp::switchToMode(global::RunMode mode, QString* errorMsg)
	{
		const auto current = currentMode_.load(std::memory_order_acquire);
		if (current == mode)
			return true;

		// 前置校验
		switch (mode)
		{
		case global::RunMode::Debug:
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
		default:
			break;
		}

		// 执行切换 + 配置相机 + 控制取流
		currentMode_.store(mode, std::memory_order_release);
		switch (mode)
		{
		case global::RunMode::Idle:        configureCameraForIdle(); break;
		case global::RunMode::Debug:       configureCameraForDebug(); break;
		case global::RunMode::Production:  configureCameraForProduction(); break;
		case global::RunMode::CreateModel: configureCameraForCreateModel(); break;
		case global::RunMode::DrawMatchRegion: configureCameraForDebug(); break;  // 同 Debug：FreeRun 3fps
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

		if (currentMode_.load(std::memory_order_acquire) == global::RunMode::Production)
		{
			// 工作模式：由 processProductionFrame 完成匹配、绘制匹配位置并发送标注后图像到 UI
			processProductionFrame(image);
		}
		else
		{
			// 非工作模式（Debug / CreateModel）：直接转发原始图像到 UI 显示
			// 跨线程由接收方用 QueuedConnection 处理
			emit frameReady(image);
		}
	}

	void PunchPressApp::onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason)
	{
		emit cameraConnectionChanged(idx, connected, reason);
	}

	// TODO(多模型PLC): 根据实际需求决定每个模型的 PLC 地址映射策略。
	// 当前实现：仅取最佳匹配写入已有 PLC 地址。若需要多模型独立 PLC 地址，
	// 可扩展 plcAddressCfg 为数组映射，在此函数中遍历 matches 分别写入。
	static void writePositionToPLC(
		const global::PositionResult& result,
		inf::ControlModule& ctrl, const Config::PlcAddressCfg& plc)
	{
		ctrl.writeFloat(plc.regOffsetX, static_cast<float>(result.offsetX * 100.0));
		ctrl.writeFloat(plc.regOffsetY, static_cast<float>(result.offsetY * 100.0));
		ctrl.writeFloat(plc.regAngle, static_cast<float>(result.angle * 100.0));
		ctrl.writeRegister(plc.regValid, result.valid ? 1 : 0);
	}

	void PunchPressApp::processProductionFrame(const HalconCpp::HImage& image)
	{
		if (!business_.shape_mode_manager_bun)
		{
			emit frameReady(image);
			return;
		}

		try
		{
		// 多模型匹配：遍历所有已加载模型
		std::vector<bun::MatchResult> matches = business_.shape_mode_manager_bun->match(image);

		// 按分数降序排列
		std::sort(matches.begin(), matches.end(),
			[](const bun::MatchResult& a, const bun::MatchResult& b) {
				return a.score > b.score;
			});

		// 收集所有有效结果
		std::vector<global::PositionResult> allResults;
		for (const auto& m : matches)
		{
			if (!m.found) continue;
			global::PositionResult r;
			r.valid = true;
			r.score = m.score;
			r.modelId = m.modelId;
			r.modelName = m.modelName;
			r.offsetX = m.offsetX;
			r.offsetY = m.offsetY;
			r.angle = m.angle;  // 已为角度制
			allResults.push_back(r);
		}

		// 写 PLC：每个匹配结果占一组寄存器（间隔 10），无效槽位清零
		auto& inf = business_.infrastructure();
		if (inf.control_module_ && inf.control_module_->isConnected() && inf.config_module_)
		{
			const auto& plc = inf.config_module_->plcAddressCfg;
			constexpr int kRegSpacing = 10;   // 每组结果的寄存器间隔
			constexpr size_t kMaxSlots = 50;  // 最大结果槽位数

			if (!allResults.empty())
			{
				// 写入有效结果
				for (size_t i = 0; i < allResults.size() && i < kMaxSlots; ++i)
				{
					const int base = plc.regOffsetX + static_cast<int>(i) * kRegSpacing;
					inf.control_module_->writeFloat(base, static_cast<float>(1));
					inf.control_module_->writeFloat(base + 2, static_cast<float>(1 ));
					inf.control_module_->writeFloat(base + 4, static_cast<float>(1));
					inf.control_module_->writeRegister(base + 6, 1);  // valid
				}

				// 清空剩余槽位
				for (size_t i = allResults.size(); i < kMaxSlots; ++i)
				{
					const int base = plc.regOffsetX + static_cast<int>(i) * kRegSpacing;
					inf.control_module_->writeFloat(base,     0.0f);
					inf.control_module_->writeFloat(base + 2, 0.0f);
					inf.control_module_->writeFloat(base + 4, 0.0f);
					inf.control_module_->writeRegister(base + 6, 0);
				}

				// 发送完成信号：有数据
				inf.control_module_->writeUInt32(plc.plcSoftwareOk, 1);
			}
			else
			{
				// 无匹配结果：清空所有槽位的 valid 标志
				for (size_t i = 0; i < kMaxSlots; ++i)
				{
					const int base = plc.regOffsetX + static_cast<int>(i) * kRegSpacing;
					inf.control_module_->writeRegister(base + 6, 0);
				}

				// 发送完成信号：无数据
				inf.control_module_->writeUInt32(plc.plcSoftwareOk, 2);
			}
		}



		// 发出所有有效结果（供 UI tableWidget_info 显示）
		if (!allResults.empty())
			emit allPositionResultsReady(allResults);

		// 取最佳匹配用于图像标注与状态栏显示
		auto best = allResults.empty()
			? allResults.end()
			: allResults.begin();

		// 准备显示图像（默认原始图像，匹配成功时叠加标注）
		HalconCpp::HImage displayImage = image;

		// 找到 best 对应的原始 MatchResult（含轮廓等）
		auto bestMatch = best != allResults.end()
			? std::find_if(matches.begin(), matches.end(),
				[&](const bun::MatchResult& m) {
					return m.found && m.modelId == best->modelId;
				})
			: matches.end();

		if (!allResults.empty())
		{
			// 发出最佳匹配结果（供状态栏显示）
			emit positionResultReady(*best);

			// 在工作图像上叠加所有匹配轮廓与十字线（每个模型不同颜色）
			try
			{
				const char* kColors[] = {
					"#FF0000", "#00FF00", "#00FFFF", "#FFFF00", "#FF00FF",
					"#FF8000", "#00FF80", "#FF0080", "#80FF00", "#0080FF"
				};
				constexpr int kColorCount = sizeof(kColors) / sizeof(kColors[0]);

				HalconCpp::HTuple imgW, imgH;
				image.GetImageSize(&imgW, &imgH);
				HalconCpp::HTuple bufWin;
				HalconCpp::OpenWindow(0, 0, imgW[0].I(), imgH[0].I(), 0, "buffer", "", &bufWin);
				HalconCpp::SetPart(bufWin, 0, 0, imgH[0].I() - 1, imgW[0].I() - 1);
				HalconCpp::DispObj(image, bufWin);

				// 同一模型的所有匹配用同一颜色，不同模型用不同颜色
				std::map<std::string, int> modelColorMap;
				for (const auto& m : matches)
				{
					if (!m.found) continue;

					// 为每个模型分配唯一颜色索引
					auto it = modelColorMap.find(m.modelId);
					if (it == modelColorMap.end())
					{
						int idx = static_cast<int>(modelColorMap.size());
						modelColorMap[m.modelId] = idx;
					}
					const char* color = kColors[modelColorMap[m.modelId] % kColorCount];

					if (m.matchedContours.IsInitialized())
					{
						HalconCpp::SetColor(bufWin, color);
						HalconCpp::SetLineWidth(bufWin, 2);
						HalconCpp::DispObj(m.matchedContours, bufWin);
					}

					HalconCpp::SetColor(bufWin, color);
					HalconCpp::SetLineWidth(bufWin, 6);
					// m.angle 为角度制，Halcon DispCross 需要弧度
					HalconCpp::DispCross(bufWin, m.row, m.column, 75.0,
						m.angle * 3.14159265358979323846 / 180.0);
				}

				HalconCpp::DumpWindowImage(&displayImage, bufWin);
				HalconCpp::CloseWindow(bufWin);
			}
			catch (...)
			{
				displayImage = image;
			}
		}
		else
		{
			// 无匹配结果：发出无效结果信号（状态栏清除）
			global::PositionResult result;
			emit positionResultReady(result);
		}

		emit frameReady(displayImage);
		}
		catch (...)
		{
			emit frameReady(image);
		}
	}
}
