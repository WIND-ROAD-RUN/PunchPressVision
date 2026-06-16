#include "CameraModule.t.hpp"

#include <QCoreApplication>

#include "infrastructure/CameraModule/CameraModule.hpp"
#include "infrastructure/ConfigModule/ConfigModule.hpp"

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	inf::ConfigModule cfgModule;
	cfgModule.build();

	inf::CameraModule cameraModule;
	cameraModule.baseCfg = cfgModule.baseCfg;
	cameraModule.cameraCfg = cfgModule.cameraCfg;
	cameraModule.build();
	
	QObject::connect(&cameraModule, &inf::CameraModule::callBackFunc
		, [](rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
		{
			//std::cout << "CameraIndex" << static_cast<int>(cameraIndex) << " MatInfo: " << matInfo.frameInfo.width << "x" << matInfo.frameInfo.height << std::endl;
		});


	for (int i=0;i<10000;i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cout << "Connect status: " << cameraModule.isConnected(global::CameraIndex::Camera1) << ", " << cameraModule.isConnected(global::CameraIndex::Camera2) << std::endl;
	}

	cameraModule.startMonitor();
	
	return app.exec();
}