#include "CalibDistortionToolWindow.hpp"

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

    CalibDistortionToolWindow wnd(inf);
    wnd.setWindowTitle(QStringLiteral("CalibDistortionTool 畸变矫正工具"));
    wnd.resize(1024, 768);
    wnd.show();

    const int rc = app.exec();

    inf.destroy();
    return rc;
}
