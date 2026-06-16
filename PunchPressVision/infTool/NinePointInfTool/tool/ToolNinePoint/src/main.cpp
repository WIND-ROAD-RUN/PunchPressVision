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
	wnd.show();

	const int rc = app.exec();

	inf.destroy();
	return rc;
}
