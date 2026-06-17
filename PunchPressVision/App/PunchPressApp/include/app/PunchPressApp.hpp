#pragma once

#include <atomic>
#include <memory>

#include <QObject>
#include <QString>
#include <QSystemSemaphore>

#include "global/GlobalType.hpp"
#include "global/GlobalReadiness.hpp"
#include "global/GlobalResult.hpp"
#include "Business/Business.hpp"

#include "halconcpp/HalconCpp.h"

namespace app
{
	// App 层聚合根：模式状态机、启动检查、就绪性检查、生产帧处理编排。
	// 通过构造函数接收 bun::Business& 实现依赖注入，对外向 UI 暴露信号。
	class PunchPressApp : public QObject
	{
		Q_OBJECT
	public:
		explicit PunchPressApp(bun::Business& business, QObject* parent = nullptr);
		~PunchPressApp() override;

		void build();
		void destroy();
		void start();
		void stop();

		// 模式切换（FR-005 ~ FR-020）
		bool switchToMode(global::RunMode mode, QString* errorMsg = nullptr);
		global::RunMode currentMode() const;

		// 启动检查（FR-001 ~ FR-004）
		bool performStartupCheck(QString* errorMsg = nullptr);

		// 就绪性查询
		global::CalibReadiness checkCalibReadiness() const;
		global::ProductionReadiness checkProductionReadiness() const;

		// 相机配置代理（FR-005 / FR-008 / FR-011 / Idle）
		bool configureCameraForDebug();
		bool configureCameraForProduction();
		bool configureCameraForCalibration();
		bool configureCameraForIdle();
		bool configureCameraForCreateModel();

		// 供 UI 访问 Business 能力
		bun::Business& business() { return business_; }

	signals:
		void modeChanged(global::RunMode newMode);
		void startupCheckFailed(const QString& reason);
		void frameReady(HalconCpp::HImage image);
		void positionResultReady(global::PositionResult result);
		void cameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason);
		void plcConnectionChanged(bool connected);

	private slots:
		void onFrameReady(HalconCpp::HImage image);
		void onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason);

	private:
		bun::Business& business_;
		std::atomic<global::RunMode> currentMode_{ global::RunMode::Idle };
		std::unique_ptr<QSystemSemaphore> instanceLock_;

		// 启动检查子方法
		bool checkSingleInstance();
		bool checkMVSProcessConflict();
		bool checkConfigIntegrity();

		// 工作模式图像处理（匹配 + 写 PLC）
		void processProductionFrame(const HalconCpp::HImage& image);
	};
}
