#include "ToolTwoCameraSpliceWindow.hpp"

#include <QApplication>
#include <QMetaType>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"

Q_DECLARE_METATYPE(global::CameraIndex)

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

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
