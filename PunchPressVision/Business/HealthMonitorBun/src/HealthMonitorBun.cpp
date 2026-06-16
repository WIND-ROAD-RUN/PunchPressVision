#include "Business/HealthMonitorBun/HealthMonitorBun.hpp"

#include <QTimer>

#include "infrastructure/infrastructure.hpp"
#include "infrastructure/CameraModule/CameraModule.hpp"
#include "infrastructure/ControlModule/ControlModule.hpp"

namespace bun
{
	// ======================================================
	// HealthMonitorWorker（工作线程侧，.cpp 内部类，不对外暴露）
	// ======================================================

	class HealthMonitorWorker : public QObject
	{
		Q_OBJECT
	public:
		explicit HealthMonitorWorker(inf::infrastructure& inf, int intervalMs, QObject* parent = nullptr)
			: QObject(parent)
			, inf_(inf)
			, pollTimer_(new QTimer(this))
		{
			pollTimer_->setInterval(intervalMs);
			connect(pollTimer_, &QTimer::timeout, this, &HealthMonitorWorker::poll);
		}

	public slots:
		void startPolling()
		{
			// 首次启动时立即轮询一次，获取基线状态
			poll();
			pollTimer_->start();
		}

		void stopPolling()
		{
			pollTimer_->stop();
		}

	signals:
		void cameraStateChanged(global::CameraIndex idx, bool connected, QString reason);
		void plcStateChanged(bool connected);

	private slots:
		void poll()
		{
			// --- 相机连接状态 ---
			if (inf_.camera_module_)
			{
				for (auto idx : { global::CameraIndex::Camera1, global::CameraIndex::Camera2 })
				{
					const bool now = inf_.camera_module_->isConnected(idx);
					bool& cached = (idx == global::CameraIndex::Camera1) ? cam1Cached_ : cam2Cached_;

					if (now != cached)
					{
						cached = now;
						emit cameraStateChanged(idx, now,
							now ? QStringLiteral("ok") : QStringLiteral("连接丢失"));
					}
				}
			}

			// --- PLC 连接状态 ---
			if (inf_.control_module_)
			{
				const bool plcNow = inf_.control_module_->isConnected();
				if (plcNow != plcCached_)
				{
					plcCached_ = plcNow;
					emit plcStateChanged(plcNow);
				}
			}
		}

	private:
		inf::infrastructure& inf_;
		QTimer* pollTimer_;

		// 变更检测缓存（仅在工作线程访问，无竞争）
		bool cam1Cached_{ false };
		bool cam2Cached_{ false };
		bool plcCached_{ false };
	};

	// ======================================================
	// HealthMonitorBun（主线程侧对外接口）
	// ======================================================

	HealthMonitorBun::HealthMonitorBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	HealthMonitorBun::~HealthMonitorBun()
	{
		stop();
	}

	void HealthMonitorBun::build()
	{
	}

	void HealthMonitorBun::destroy()
	{
		stop();
	}

	void HealthMonitorBun::start()
	{
		if (thread_)
			return; // 已经启动

		auto* worker = new HealthMonitorWorker(inf_, pollIntervalMs_); // 无 parent，由 thread 管理生命周期
		thread_ = new QThread(this);

		worker->moveToThread(thread_);

		// 线程启动 → Worker 开始轮询
		QObject::connect(thread_, &QThread::started, worker, &HealthMonitorWorker::startPolling);
		// 线程停止 → Worker 销毁
		QObject::connect(thread_, &QThread::finished, worker, &QObject::deleteLater);

		// Worker 信号 → Bun 信号（跨线程，Qt 自动 QueuedConnection）
		QObject::connect(worker, &HealthMonitorWorker::cameraStateChanged,
			this, &HealthMonitorBun::cameraConnectionStateChanged);
		QObject::connect(worker, &HealthMonitorWorker::plcStateChanged,
			this, &HealthMonitorBun::plcConnectionChanged);

		thread_->start();
	}

	void HealthMonitorBun::stop()
	{
		if (!thread_)
			return;

		thread_->quit();
		thread_->wait();
		thread_->deleteLater();
		thread_ = nullptr;
	}

	void HealthMonitorBun::setPollInterval(int ms)
	{
		pollIntervalMs_ = ms;
	}
}

#include "HealthMonitorBun.moc"
