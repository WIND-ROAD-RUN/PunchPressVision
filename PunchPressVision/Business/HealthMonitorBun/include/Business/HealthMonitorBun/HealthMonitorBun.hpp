#pragma once

#include <QObject>
#include <QString>
#include <QThread>

#include "global/GlobalInterface.hpp"
#include "global/GlobalType.hpp"

namespace inf { class infrastructure; }

namespace bun
{
	// 基础设施资源健康监控（主线程侧对外接口）。
	// 内部将轮询逻辑委托给工作线程中的 HealthMonitorWorker，
	// 对外暴露 cameraConnectionStateChanged / plcConnectionChanged 信号。
	class HealthMonitorBun : public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit HealthMonitorBun(inf::infrastructure& inf);
		~HealthMonitorBun() override;

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

		/// 设置轮询间隔（毫秒），需在 start() 之前调用
		void setPollInterval(int ms);

	signals:
		/// 与 CameraModule::cameraConnectionStateChanged 保持相同签名，便于上游直接桥接
		void cameraConnectionStateChanged(global::CameraIndex idx, bool connected, QString reason);
		/// 与 ControlModule::connectionStateChanged 保持相同签名
		void plcConnectionChanged(bool connected);

	private:
		inf::infrastructure& inf_;
		QThread* thread_{ nullptr };
		int pollIntervalMs_{ 3000 };
	};
}
