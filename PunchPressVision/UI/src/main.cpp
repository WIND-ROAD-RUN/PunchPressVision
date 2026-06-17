#include <QApplication>

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
	w.show();

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
