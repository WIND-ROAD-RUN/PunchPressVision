#include "ToolNinePointWindow.hpp"

#include <QApplication>
#include <QMetaType>

#include "halconcpp/HalconCpp.h"
#include "global/GlobalType.hpp"
#include "infrastructure/infrastructure.hpp"

Q_DECLARE_METATYPE(global::CameraIndex)

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	qRegisterMetaType<HalconCpp::HImage>("HalconCpp::HImage");
	qRegisterMetaType<global::CameraIndex>("global::CameraIndex");

	inf::infrastructure inf;
	inf.build();

	ToolNinePointWindow wnd(inf);
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
