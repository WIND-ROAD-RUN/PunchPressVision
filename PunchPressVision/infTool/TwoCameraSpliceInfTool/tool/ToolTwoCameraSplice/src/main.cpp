#include "ToolTwoCameraSpliceWindow.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QMetaType>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "global/VersionChecker.hpp"
#include "infrastructure/infrastructure.hpp"

Q_DECLARE_METATYPE(global::CameraIndex)

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	// ======================================================
	// 版本一致性检查
	// ======================================================
	{
		const auto versionError = global::checkVersionConsistency();
		if (versionError.has_value())
		{
			const auto result = QMessageBox::question(
				nullptr,
				QStringLiteral("版本不一致"),
				QString::fromStdString(versionError.value())
					+ QStringLiteral("\n\n是否更新工作目录版本文件以匹配当前程序？"),
				QMessageBox::Yes | QMessageBox::No,
				QMessageBox::No);
			if (result == QMessageBox::Yes)
			{
				global::updateWorkDirBuildId();
			}
			else
			{
				return 1;
			}
		}
	}

	// 自定义枚举/类需要注册，才能在跨线程信号中传递
	qRegisterMetaType<HalconCpp::HImage>("HalconCpp::HImage");
	qRegisterMetaType<global::CameraIndex>("global::CameraIndex");

	inf::infrastructure inf;
	inf.build();
	inf.camera_module_->setFreeRunMode(global::CameraIndex::Camera1, 3);
	inf.camera_module_->setFreeRunMode(global::CameraIndex::Camera2, 3);
	ToolTwoCameraSpliceWindow wnd(inf);
#ifdef PPV_RELEASE_FULLSCREEN
	wnd.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	wnd.showFullScreen();
#else
	wnd.show();
#endif

	const int rc = app.exec();

	inf.destroy();
	return rc;
}
