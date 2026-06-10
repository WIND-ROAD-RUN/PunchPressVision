#include <QApplication>

#include "infrastructure/infrastructure.hpp"
#include "Business/Business.hpp"
#include "app/PunchPressApp.hpp"
#include "UI/PunchPress.h"

// PunchPressVision 启动时序（见 HLD 第 9 节）：
//   infrastructure.build() → Business.build()/start()
//   → PunchPressApp.build()/start() → PunchPress 主窗口
int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	// 1. 基础设施层
	inf::infrastructure infrastructure;
	infrastructure.build();

	// 2. 业务层
	bun::Business business(infrastructure);
	business.build();
	business.start();

	// 3. 应用层
	app::PunchPressApp app(business);
	app.build();
	app.start();

	// 4. UI 层（主窗口在构造中执行启动检查）
	ui::PunchPress w(app);
	w.show();

	const int code = a.exec();

	// 逆序清理
	app.stop();
	app.destroy();
	business.stop();
	business.destroy();
	infrastructure.destroy();

	return code;
}
