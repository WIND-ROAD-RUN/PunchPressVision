#include <QApplication>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"

#include "infrastructure/infrastructure.hpp"
#include "Business/Business.hpp"
#include "app/PunchPressApp.hpp"
#include "UI/PunchPress.h"

// PunchPressVision 启动时序：
//   Phase 1: 构造所有对象（构造 ≠ 初始化）
//   Phase 2: infrastructure.build() → business.build() → app.build() → w.build()
//   Phase 3: business.start() → app.start()
//   Phase 4: w.show() → a.exec()
//   Phase 5: app.stop() → business.stop()
//   Phase 6: w.destroy() → app.destroy() → business.destroy() → infrastructure.destroy()
int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	// 注册 Halcon 类型到 Qt 元类型系统——跨线程 QueuedConnection 必需。
	// 帧管线中 TwoCameraSpliceInfTool(相机线程) → CameraBun(主线程) 通过
	// QueuedConnection 跨线程传递 HImage，缺少此注册将导致连接静默失败。
	qRegisterMetaType<HalconCpp::HImage>("HalconCpp::HImage");
	qRegisterMetaType<global::CameraIndex>("global::CameraIndex");

	// ======================================================
	// Phase 1: 构造 — 只创建对象/共享指针，不执行初始化逻辑
	// ======================================================
	inf::infrastructure infrastructure;
	bun::Business business(infrastructure);
	app::PunchPressApp app(business);
	ui::PunchPress w(app);

	// ======================================================
	// Phase 2: build — 连接信号、打开资源、子组件初始化
	// ======================================================
	infrastructure.build();
	business.build();
	app.build();
	w.build();

	// ======================================================
	// Phase 3: start — 启动运行（开始帧流等）
	// ======================================================
	business.start();
	app.start();

	// ======================================================
	// Phase 4: UI 显示
	// ======================================================
#ifdef PPV_RELEASE_FULLSCREEN
	w.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	w.showFullScreen();
#else
	w.show();
#endif

	const int code = a.exec();

	// ======================================================
	// Phase 5: stop — 逆序停止运行
	// ======================================================
	app.stop();
	business.stop();

	// ======================================================
	// Phase 6: destroy — 逆序销毁（断开信号、释放资源）
	// ======================================================
	w.destroy();
	app.destroy();
	business.destroy();
	infrastructure.destroy();

	return code;
}
